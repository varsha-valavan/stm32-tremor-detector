/* Core/Src/main.c - TremorDetector
 *
 * Ready to paste into Core/Src/main.c
 *
 * Notes:
 * - Keep _write in syscalls.c; do not duplicate it here.
 * - If CubeMX generated SystemClock_Config() or Error_Handler(), remove the weak stubs here.
 */

#include "main.h"
#include "spi.h"
#include "usart.h"
#include "gpio.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>
#include <stdint.h>

#include "ai_run.h"
#include "mpu9255.h"

/* If CubeMX generated these, they will override the weak stubs below */
__attribute__((weak)) void SystemClock_Config(void) { /* noop placeholder */ }
__attribute__((weak)) void Error_Handler(void) { __disable_irq(); while (1) {} }

/* ---------------- normalization constants (from your print_norm output) ---------------- */
static const float NORM_MEAN[13] = {
  475.5448913574219f, -2324.07373046875f, -12489.7109375f,
  2917.511474609375f, 7171.20947265625f, 3751.285888671875f,
  -0.12948518991470337f, 0.05645456910133362f, -0.10224789381027222f,
  18359.15234375f, 3815.58251953125f, 27643.91796875f, 2792.54541015625f
};

static const float NORM_STD[13] = {
  4691.896484375f, 6483.91796875f, 4332.5068359375f,
  1875.8753662109375f, 6381.36083984375f, 2427.640380859375f,
  0.6607775092124939f, 0.3843545913696289f, 0.7560881972312927f,
  3534.201416015625f, 2746.9609375f, 8622.3212890625f, 2302.416015625f
};

/* Sampling/window parameters (match your python) */
#define FS      200
#define WIN     (FS * 1)   /* 200 */
#define STEP    (FS / 2)   /* 100 */

/* Circular window buffer
   NOTE: buffer holds raw counts (as floats) — not 'g' values.
*/
static float win_buf[WIN][3];
static int win_index = 0;
static int samples_in_buf = 0;

/* Calibration constants (in raw counts) */
#define CALIB_SAMPLES 100
static float calib_off_x = 0.0f, calib_off_y = 0.0f, calib_off_z = 0.0f;
static int calibrated = 0;

/* Toggle: printing floating-point numbers may cause newlib to allocate heap.
   Set to 1 only when you know heap is available (or for debugging). */
#define DEBUG_PRINT_FLOATS 0

/* small helper: safe "is bad" test for floats avoiding isfinite dependency */
static int value_is_bad(float v)
{
    if (v != v) return 1;            /* NaN */
    if (fabsf(v) > 1e30f) return 1;  /* extreme */
    return 0;
}

/* compute skew of array x length n */
static float compute_skew(const float *x, int n)
{
    if (n <= 1) return 0.0f;
    float mean = 0.0f;
    for (int i = 0; i < n; ++i) mean += x[i];
    mean /= n;
    float var = 0.0f;
    for (int i = 0; i < n; ++i) {
        float d = x[i] - mean;
        var += d * d;
    }
    var /= n;
    float std = sqrtf(var) + 1e-12f;
    float m3 = 0.0f;
    for (int i = 0; i < n; ++i) {
        float z = (x[i] - mean) / std;
        m3 += z * z * z;
    }
    m3 /= n;
    return m3;
}

/* simple insertion-sort median for a copy; returns median */
static float median_of_copy(float *arr, int n)
{
    /* sort in-place arr */
    for (int i = 1; i < n; ++i) {
        float key = arr[i];
        int j = i - 1;
        while (j >= 0 && arr[j] > key) {
            arr[j + 1] = arr[j];
            j--;
        }
        arr[j + 1] = key;
    }
    if (n % 2) return arr[n / 2];
    return 0.5f * (arr[n / 2 - 1] + arr[n / 2]);
}

/* compute the 13 features as in make_features.compute_feats
   IMPORTANT: buf contains raw int16 counts stored as floats (no /16384).
*/
static void compute_features_from_window(float feats[13], float buf[WIN][3])
{
    float ax[WIN], ay[WIN], az[WIN], mag[WIN];

    for (int i = 0; i < WIN; ++i) {
        ax[i] = buf[i][0];
        ay[i] = buf[i][1];
        az[i] = buf[i][2];
        /* magnitude in same units (counts) */
        mag[i] = sqrtf(ax[i]*ax[i] + ay[i]*ay[i] + az[i]*az[i]);
    }

    /* means */
    float mean_ax = 0, mean_ay = 0, mean_az = 0;
    for (int i = 0; i < WIN; ++i) { mean_ax += ax[i]; mean_ay += ay[i]; mean_az += az[i]; }
    mean_ax /= WIN; mean_ay /= WIN; mean_az /= WIN;

    /* stds */
    float var_ax = 0, var_ay = 0, var_az = 0;
    for (int i = 0; i < WIN; ++i) {
        float dx = ax[i] - mean_ax; var_ax += dx*dx;
        float dy = ay[i] - mean_ay; var_ay += dy*dy;
        float dz = az[i] - mean_az; var_az += dz*dz;
    }
    var_ax /= WIN; var_ay /= WIN; var_az /= WIN;
    float std_ax = sqrtf(var_ax);
    float std_ay = sqrtf(var_ay);
    float std_az = sqrtf(var_az);

    /* skew */
    float skew_ax = compute_skew(ax, WIN);
    float skew_ay = compute_skew(ay, WIN);
    float skew_az = compute_skew(az, WIN);

    /* mag stats */
    float mean_mag = 0.0f, var_mag = 0.0f, max_mag = mag[0];
    for (int i = 0; i < WIN; ++i) {
        mean_mag += mag[i];
        if (mag[i] > max_mag) max_mag = mag[i];
    }
    mean_mag /= WIN;
    for (int i = 0; i < WIN; ++i) { float d = mag[i] - mean_mag; var_mag += d*d; }
    var_mag /= WIN;
    float std_mag = sqrtf(var_mag);

    /* median absolute deviation (MAD) */
    float mag_copy[WIN];
    for (int i = 0; i < WIN; ++i) mag_copy[i] = mag[i];
    float med = median_of_copy(mag_copy, WIN);
    for (int i = 0; i < WIN; ++i) mag_copy[i] = fabsf(mag[i] - med);
    float mad = median_of_copy(mag_copy, WIN);

    /* fill features in the same order as Python (raw counts units) */
    feats[0]  = mean_ax;
    feats[1]  = mean_ay;
    feats[2]  = mean_az;
    feats[3]  = std_ax;
    feats[4]  = std_ay;
    feats[5]  = std_az;
    feats[6]  = skew_ax;
    feats[7]  = skew_ay;
    feats[8]  = skew_az;
    feats[9]  = mean_mag;
    feats[10] = std_mag;
    feats[11] = max_mag;
    feats[12] = mad;
}

/* normalize in-place */
static void normalize_feats_inplace(float x[13])
{
    for (int i = 0; i < 13; ++i) {
        x[i] = (x[i] - NORM_MEAN[i]) / (NORM_STD[i] + 1e-9f);
    }
}

/* slide buffer left by step samples */
static void slide_buffer_by_step(int step)
{
    if (step <= 0 || step >= WIN) return;
    memmove(win_buf, win_buf + step, sizeof(win_buf[0]) * (WIN - step));
    samples_in_buf -= step;
    if (samples_in_buf < 0) samples_in_buf = 0;
    win_index = samples_in_buf % WIN;
}

/* Optional: simple sensor calibration routine (averages a number of raw readings)
   Calibration returns offsets in raw counts (same units as features).
*/
static void calibrate_sensor_offsets(int samples)
{
    int64_t sx = 0, sy = 0, sz = 0;
    int got = 0;
    for (int i = 0; i < samples; ++i) {
        int16_t ax, ay, az;
        if (mpu9255_read_accel_raw(&ax, &ay, &az) != 0) {
            HAL_Delay(5);
            continue;
        }
        sx += ax; sy += ay; sz += az;
        got++;
        HAL_Delay(1000 / FS);
    }
    if (got > 0) {
        float mean_ax = (float)sx / got;   /* raw counts */
        float mean_ay = (float)sy / got;
        float mean_az = (float)sz / got;
        calib_off_x = mean_ax;
        calib_off_y = mean_ay;
        calib_off_z = mean_az;
        calibrated = 1;
    }
}

/* main */
int main(void)
{
    HAL_Init();
    SystemClock_Config();

    MX_GPIO_Init();
    MX_SPI1_Init();
    MX_USART2_UART_Init();

    printf("\r\nTremorDetector boot\r\n");

    /* quick malloc test to check heap availability (newlib) */
    void *ptest = malloc(32);
    if (ptest) {
#if DEBUG_PRINT_FLOATS
        printf("malloc OK ptr=%p\r\n", ptest);
#else
        printf("malloc OK ptr=%p\r\n", ptest);
#endif
        free(ptest);
    } else {
        printf("malloc returned NULL -> heap may be 0 or _sbrk wrong\r\n");
    }

    /* initialize MPU */
    mpu9255_init();
    printf("MPU9255 init done\r\n");

    /* optionally calibrate once (uncomment to run)
       The calibration now stores offsets in raw counts.
    */
    /* calibrate_sensor_offsets(CALIB_SAMPLES);
       if (calibrated) printf("Calib offs (counts): %.2f %.2f %.2f\r\n",
                             calib_off_x, calib_off_y, calib_off_z);
    */

    /* initialize AI library and check status */
    int ai_ok = AI_Init();
    if (ai_ok != 0) {
        printf("AI_Init returned %d (network not ready)\r\n", ai_ok);
        /* you may continue; AI_Run will error until AI_Init succeeds */
    } else {
        printf("AI initialized OK\r\n");
    }

    /* init buffer */
    memset(win_buf, 0, sizeof(win_buf));
    win_index = 0;
    samples_in_buf = 0;

    float feats[13];
    float ai_in[13];
    float ai_out[4];

    /* main loop: sample at FS */
    while (1)
    {
        int16_t ax_i = 0, ay_i = 0, az_i = 0;
        if (mpu9255_read_accel_raw(&ax_i, &ay_i, &az_i) != 0) {
            printf("MPU read err\r\n");
            HAL_Delay(10);
            continue;
        }

        /* WORK IN RAW COUNTS (do NOT divide by 16384) */
        float ax = (float)ax_i;
        float ay = (float)ay_i;
        float az = (float)az_i;

        /* apply optional simple offset calibration (offsets in raw counts) */
        if (calibrated) {
            ax -= calib_off_x;
            ay -= calib_off_y;
            az -= calib_off_z;
        }

#if DEBUG_PRINT_FLOATS
        /* If you enable DEBUG_PRINT_FLOATS be aware of newlib float formatting heap use */
        printf("sample counts: %.2f %.2f %.2f\r\n", ax, ay, az);
#else
        int iax = (int)ax;
        int iay = (int)ay;
        int iaz = (int)az;
        //printf("counts: %d %d %d\r\n", iax, iay, iaz);
#endif

        /* store into circular buffer (raw counts) */
        win_buf[win_index][0] = ax;
        win_buf[win_index][1] = ay;
        win_buf[win_index][2] = az;
        win_index = (win_index + 1) % WIN;
        if (samples_in_buf < WIN) ++samples_in_buf;

        /* process when we have full window */
        if (samples_in_buf >= WIN) {
            /* copy in chronological order to tmp_win */
            float tmp_win[WIN][3];
            int start = win_index % WIN; /* oldest sample position */
            for (int i = 0; i < WIN; ++i) {
                int src = (start + i) % WIN;
                tmp_win[i][0] = win_buf[src][0];
                tmp_win[i][1] = win_buf[src][1];
                tmp_win[i][2] = win_buf[src][2];
            }

            /* compute features (in raw counts), then normalize */
            compute_features_from_window(feats, tmp_win);
            memcpy(ai_in, feats, sizeof(feats));
            normalize_feats_inplace(ai_in);

#if DEBUG_PRINT_FLOATS
            //printf("AI in:");
            //for (int i = 0; i < 13; ++i) printf(" %.4f", ai_in[i]);
            //printf("\r\n");
#else
            /* print scaled ints for quick debug without %f allocation */
            //printf("AI in (x1000):");
            //for (int i = 0; i < 13; ++i) printf(" %d", (int)(ai_in[i] * 1000.0f));
            //printf("\r\n");
#endif

            /* run model */
            int ret = AI_Run(ai_in, ai_out);
            if (ret != 0) {
                printf("AI_Run error %d\r\n", ret);
            } else {
                /* validate outputs */
                int bad = 0;
                for (int i = 0; i < 4; ++i) if (value_is_bad(ai_out[i])) { bad = 1; break; }

                if (bad) {
                    printf("AI out invalid (NaN/Inf). Skipping.\r\n");
                } else {
#if DEBUG_PRINT_FLOATS
                    //printf("Raw: %.6f %.6f %.6f %.6f\r\n", ai_out[0], ai_out[1], ai_out[2], ai_out[3]);
#else
                    int ai0 = (int)(ai_out[0] * 1000.0f);
                    int ai1 = (int)(ai_out[1] * 1000.0f);
                    int ai2 = (int)(ai_out[2] * 1000.0f);
                    int ai3 = (int)(ai_out[3] * 1000.0f);
                    //printf("Raw(x1000): %d %d %d %d\r\n", ai0, ai1, ai2, ai3);
#endif
                    /* choose best */
                    int best = 0;
                    float bestv = ai_out[0];
                    for (int i = 1; i < 4; ++i) { if (ai_out[i] > bestv) { bestv = ai_out[i]; best = i; } }
                    const char *labels[4] = {"NORMAL","MILD","MODERATE","SEVERE"};
#if DEBUG_PRINT_FLOATS
                    printf("Pred: %s (%.3f)\r\n", labels[best], bestv);
#else
                    printf("Pred: %s (score x1000=%d)\r\n", labels[best], (int)(bestv * 1000.0f));
#endif
                }
            }

            /* advance by hop (STEP) */
            slide_buffer_by_step(STEP);
            win_index = samples_in_buf % WIN;
        }

        /* wait until next sample */
        HAL_Delay(1000 / FS);
    } /* while */
}
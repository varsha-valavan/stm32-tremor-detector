#include "mpu9255.h"
#include "stm32f4xx_hal.h"

#define REG_PWR_MGMT_1    0x6B
#define REG_ACCEL_CONFIG  0x1C
#define REG_ACCEL_XOUT_H  0x3B
#define REG_SMPLRT_DIV    0x19
#define REG_CONFIG        0x1A
#define REG_INT_PIN_CFG   0x37

static void cs_low(void)  { HAL_GPIO_WritePin(MPU9255_CS_PORT, MPU9255_CS_PIN, GPIO_PIN_RESET); }
static void cs_high(void) { HAL_GPIO_WritePin(MPU9255_CS_PORT, MPU9255_CS_PIN, GPIO_PIN_SET); }

void mpu9255_write_reg(uint8_t reg, uint8_t val) {
    uint8_t data[2] = { reg & 0x7F, val };
    cs_low();
    HAL_SPI_Transmit(&hspi1, data, 2, HAL_MAX_DELAY);
    cs_high();
}

uint8_t mpu9255_read_reg(uint8_t reg) {
    uint8_t tx = reg | 0x80, rx;
    cs_low();
    HAL_SPI_Transmit(&hspi1, &tx, 1, HAL_MAX_DELAY);
    HAL_SPI_Receive(&hspi1, &rx, 1, HAL_MAX_DELAY);
    cs_high();
    return rx;
}

int mpu9255_read_accel_raw(int16_t *ax, int16_t *ay, int16_t *az) {
    uint8_t tx = REG_ACCEL_XOUT_H | 0x80;
    uint8_t buf[6];
    cs_low();
    HAL_SPI_Transmit(&hspi1, &tx, 1, HAL_MAX_DELAY);
    HAL_SPI_Receive(&hspi1, buf, 6, HAL_MAX_DELAY);
    cs_high();
    *ax = (int16_t)(buf[0] << 8 | buf[1]);
    *ay = (int16_t)(buf[2] << 8 | buf[3]);
    *az = (int16_t)(buf[4] << 8 | buf[5]);
    return 0;
}

int mpu9255_init(void) {
    HAL_Delay(100);
    cs_high();
    mpu9255_write_reg(REG_PWR_MGMT_1, 0x00); // wake up
    HAL_Delay(50);
    mpu9255_write_reg(REG_SMPLRT_DIV, 4);
    mpu9255_write_reg(REG_CONFIG, 0x03);
    mpu9255_write_reg(REG_ACCEL_CONFIG, 0x00); // ±2g
    mpu9255_write_reg(REG_INT_PIN_CFG, 0x00);
    return 0;
}
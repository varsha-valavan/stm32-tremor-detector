#ifndef INC_MPU9255_H_
#define INC_MPU9255_H_

#include "stm32f4xx_hal.h"
#include <stdint.h>

extern SPI_HandleTypeDef hspi1;

#define MPU9255_CS_PORT GPIOA
#define MPU9255_CS_PIN GPIO_PIN_4

int mpu9255_init(void);
int mpu9255_read_accel_raw(int16_t *ax, int16_t *ay, int16_t *az);
void mpu9255_write_reg(uint8_t reg, uint8_t val);
uint8_t mpu9255_read_reg(uint8_t reg);

#endif /* INC_MPU9255_H_ */
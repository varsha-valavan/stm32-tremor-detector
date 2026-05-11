# STM32 Tremor Detector using MPU9255

## Description
Real-time hand tremor detection system using STM32F4 microcontroller
and MPU9255 IMU sensor via SPI communication.

## Hardware
- STM32F4 (Nucleo board)
- MPU9255 IMU (Accelerometer + Gyroscope)
- SPI1 interface
- UART2 for serial output

## How it works
1. Reads raw accelerometer data from MPU9255 via SPI at 200Hz
2. Applies bandpass filter (3-8 Hz) — tremor frequency range
3. Computes RMS over 1-second window
4. Classifies tremor severity:

| RMS Value | Classification |
|-----------|---------------|
| < 0.02g   | NORMAL        |
| < 0.05g   | MILD          |
| < 0.12g   | MODERATE      |
| ≥ 0.12g   | SEVERE        |

## Pin Connections
- MPU9255 CS → PA4
- SPI1 → SCK, MISO, MOSI
- UART2 → Serial monitor output

## Files
- main.c → Main application with tremor classification
- mpu9255.c → SPI driver for MPU9255
- mpu9255.h → Header file

## Language
Embedded C (STM32 HAL)
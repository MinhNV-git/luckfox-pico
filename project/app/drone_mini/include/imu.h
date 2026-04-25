#ifndef IMU_H
#define IMU_H
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#pragma pack(push, 1)
typedef struct {
    int16_t accel_x;
    int16_t accel_y;
    int16_t accel_z;
    int16_t gyro_x;
    int16_t gyro_y;
    int16_t gyro_z;
} S_IMU_DATA;
#pragma pack(pop)

// Function declarations for IMU module
void init_imu();
void read_imu_data();

#endif // IMU_H
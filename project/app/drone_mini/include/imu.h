#ifndef IMU_H
#define IMU_H
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "filter.h"

#define IMU_ACCEL_RAW_X_OFFSET (0)
#define IMU_ACCEL_RAW_Y_OFFSET (0)
#define IMU_ACCEL_RAW_Z_OFFSET (0)
#define IMU_GYRO_RAW_X_OFFSET (0)
#define IMU_GYRO_RAW_Y_OFFSET (0)
#define IMU_GYRO_RAW_Z_OFFSET (0)

#define IMU_ACCEL_X_LPF_FREQ_CUTOFF_HZ (50)
#define IMU_ACCEL_Y_LPF_FREQ_CUTOFF_HZ (100)
#define IMU_ACCEL_Z_LPF_FREQ_CUTOFF_HZ (100)
#define IMU_GYRO_X_LPF_FREQ_CUTOFF_HZ (50)
#define IMU_GYRO_Y_LPF_FREQ_CUTOFF_HZ (50)
#define IMU_GYRO_Z_LPF_FREQ_CUTOFF_HZ (50)

#define IMU_ACCEL_X_LPF_FREQ_SAMPLING_HZ (1000)
#define IMU_ACCEL_Y_LPF_FREQ_SAMPLING_HZ (1000)
#define IMU_ACCEL_Z_LPF_FREQ_SAMPLING_HZ (1000)
#define IMU_GYRO_X_LPF_FREQ_SAMPLING_HZ (1000)
#define IMU_GYRO_Y_LPF_FREQ_SAMPLING_HZ (1000)
#define IMU_GYRO_Z_LPF_FREQ_SAMPLING_HZ (1000)

#pragma pack(push, 1)
typedef struct {
    int16_t accel_x;
    int16_t accel_y;
    int16_t accel_z;
    int16_t gyro_x;
    int16_t gyro_y;
    int16_t gyro_z;
    int16_t temp;
} S_IMU_DATA;

typedef struct {
    float accel_scale;
    float gyro_scale;
}S_IMU_SCALE;
#pragma pack(pop)

typedef enum {
    IMU_FIELD_ACCEL_X = 0,
    IMU_FIELD_ACCEL_Y,
    IMU_FIELD_ACCEL_Z,
    IMU_FIELD_GYRO_X,
    IMU_FIELD_GYRO_Y,
    IMU_FIELD_GYRO_Z,
    IMU_FIELD_TEMP,
    IMU_FIELD_COUNT
} ImuFieldId;

extern LpfFilter imu_lpf_filters[IMU_FIELD_COUNT];
// Function declarations for IMU module
int imu_setup(void);
int imu_read_raw(S_IMU_DATA *imu_data);
int imu_read_raw_LPF(S_IMU_DATA *imu_data);
void init_imu();

#endif // IMU_H

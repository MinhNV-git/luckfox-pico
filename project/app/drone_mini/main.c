#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>
#include "calibapp.h"
#include "imu.h"
#include "main.h"

S_IMU_DATA imu_data = {
    .accel_x = 0,
    .accel_y = 0,
    .accel_z = 0,
    .gyro_x = 0,
    .gyro_y = 0,
    .gyro_z = 0
};

static void simulate_imu_data(void)
{
    static int initialized = 0;
    static int16_t accel_x_fp = 0;
    static int16_t accel_y_fp = 0;
    static int16_t accel_z_fp = 10000;
    static int16_t gyro_x_fp = 0;
    static int16_t gyro_y_fp = 0;
    static int16_t gyro_z_fp = 0;
    int accel_dx;
    int accel_dy;
    int accel_dz;
    int gyro_dx;
    int gyro_dy;
    int gyro_dz;

    if (!initialized) {
        srand((unsigned int)time(NULL));
        initialized = 1;
    }

    accel_dx = (rand() % 401) - 200;
    accel_dy = (rand() % 401) - 200;
    accel_dz = (rand() % 201) - 100;
    gyro_dx = (rand() % 601) - 300;
    gyro_dy = (rand() % 601) - 300;
    gyro_dz = (rand() % 601) - 300;

    accel_x_fp = (int16_t)(accel_x_fp + accel_dx);
    accel_y_fp = (int16_t)(accel_y_fp + accel_dy);
    accel_z_fp = (int16_t)(accel_z_fp + accel_dz);
    gyro_x_fp = (int16_t)(gyro_x_fp + gyro_dx);
    gyro_y_fp = (int16_t)(gyro_y_fp + gyro_dy);
    gyro_z_fp = (int16_t)(gyro_z_fp + gyro_dz);

    if (accel_x_fp > 3000) accel_x_fp = 3000;
    if (accel_x_fp < -3000) accel_x_fp = -3000;
    if (accel_y_fp > 3000) accel_y_fp = 3000;
    if (accel_y_fp < -3000) accel_y_fp = -3000;
    if (accel_z_fp > 12000) accel_z_fp = 12000;
    if (accel_z_fp < 8000) accel_z_fp = 8000;
    if (gyro_x_fp > 15000) gyro_x_fp = 15000;
    if (gyro_x_fp < -15000) gyro_x_fp = -15000;
    if (gyro_y_fp > 15000) gyro_y_fp = 15000;
    if (gyro_y_fp < -15000) gyro_y_fp = -15000;
    if (gyro_z_fp > 15000) gyro_z_fp = 15000;
    if (gyro_z_fp < -15000) gyro_z_fp = -15000;

    imu_data.accel_x = accel_x_fp;
    imu_data.accel_y = accel_y_fp;
    imu_data.accel_z = accel_z_fp;
    imu_data.gyro_x = gyro_x_fp;
    imu_data.gyro_y = gyro_y_fp;
    imu_data.gyro_z = gyro_z_fp;
}

int main(void) {
    printf("Drone Mini starting...\n");
    init_imu();
#ifdef CALIBAPP_ENABLE
    calibapp_init();
#endif

    //main loop
    for (;;) {
#ifdef CALIBAPP_ENABLE
        imu_read_sample(&imu_data);
        calibapp_transfer_data((const uint8_t *)&imu_data, sizeof(imu_data));
#endif
        usleep(CALIBAPP_PERIODIC_UPDATED * 1000 / 2);
    }

    return 0;
}

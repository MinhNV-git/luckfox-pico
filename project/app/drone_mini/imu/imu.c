#include "imu.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define ICM20602_IIO_PATH "/sys/bus/iio/devices/iio:device1"
#define ICM20602_NAME_PATH ICM20602_IIO_PATH "/name"
#define ICM20602_SAMPLING_FREQ_PATH ICM20602_IIO_PATH "/sampling_frequency"
#define ICM20602_ACCEL_X_RAW_PATH ICM20602_IIO_PATH "/in_accel_x_raw"
#define ICM20602_ACCEL_Y_RAW_PATH ICM20602_IIO_PATH "/in_accel_y_raw"
#define ICM20602_ACCEL_Z_RAW_PATH ICM20602_IIO_PATH "/in_accel_z_raw"
#define ICM20602_GYRO_X_RAW_PATH ICM20602_IIO_PATH "/in_anglvel_x_raw"
#define ICM20602_GYRO_Y_RAW_PATH ICM20602_IIO_PATH "/in_anglvel_y_raw"
#define ICM20602_GYRO_Z_RAW_PATH ICM20602_IIO_PATH "/in_anglvel_z_raw"
#define IMU_OUTPUT_HZ 100
#define IMU_OUTPUT_PERIOD_NS (1000000000L / IMU_OUTPUT_HZ)
#define ANSI_CLEAR_SCREEN "\033[2J"
#define ANSI_CURSOR_HOME "\033[H"

static int read_int_from_file(const char *path, int *value)
{
    FILE *file;

    file = fopen(path, "r");
    if (file == NULL) {
        fprintf(stderr, "Failed to open %s: %s\n", path, strerror(errno));
        return -1;
    }

    if (fscanf(file, "%d", value) != 1) {
        fprintf(stderr, "Failed to parse integer from %s\n", path);
        fclose(file);
        return -1;
    }

    fclose(file);
    return 0;
}

static int write_string_to_file(const char *path, const char *value)
{
    FILE *file;

    file = fopen(path, "w");
    if (file == NULL) {
        fprintf(stderr, "Failed to open %s for write: %s\n", path, strerror(errno));
        return -1;
    }

    if (fprintf(file, "%s", value) < 0) {
        fprintf(stderr, "Failed to write %s to %s: %s\n", value, path, strerror(errno));
        fclose(file);
        return -1;
    }

    fclose(file);
    return 0;
}

static void print_sensor_name(void)
{
    FILE *file;
    char name[128];

    file = fopen(ICM20602_NAME_PATH, "r");
    if (file == NULL) {
        fprintf(stderr, "Failed to open %s: %s\n", ICM20602_NAME_PATH, strerror(errno));
        return;
    }

    if (fgets(name, sizeof(name), file) != NULL) {
        name[strcspn(name, "\r\n")] = '\0';
        printf("IMU detected: %s\n", name);
    }

    fclose(file);
}

void init_imu(void)
{
    print_sensor_name();

    if (write_string_to_file(ICM20602_SAMPLING_FREQ_PATH, "100") == 0) {
        printf("IMU sampling frequency set to %d Hz\n", IMU_OUTPUT_HZ);
    } else {
        fprintf(stderr, "Continuing with existing IMU sampling frequency\n");
    }
}

void read_imu_data(void)
{
    struct timespec sleep_time = {
        .tv_sec = 0,
        .tv_nsec = IMU_OUTPUT_PERIOD_NS
    };

    printf("Reading IMU raw data from %s at %d Hz\n", ICM20602_IIO_PATH, IMU_OUTPUT_HZ);

    while (1) {
        int accel_x;
        int accel_y;
        int accel_z;
        int gyro_x;
        int gyro_y;
        int gyro_z;

        if (read_int_from_file(ICM20602_ACCEL_X_RAW_PATH, &accel_x) != 0 ||
            read_int_from_file(ICM20602_ACCEL_Y_RAW_PATH, &accel_y) != 0 ||
            read_int_from_file(ICM20602_ACCEL_Z_RAW_PATH, &accel_z) != 0 ||
            read_int_from_file(ICM20602_GYRO_X_RAW_PATH, &gyro_x) != 0 ||
            read_int_from_file(ICM20602_GYRO_Y_RAW_PATH, &gyro_y) != 0 ||
            read_int_from_file(ICM20602_GYRO_Z_RAW_PATH, &gyro_z) != 0) {
            fprintf(stderr, "Failed to read one or more IMU raw channels\n");
            nanosleep(&sleep_time, NULL);
            continue;
        }

        printf(ANSI_CLEAR_SCREEN ANSI_CURSOR_HOME);
        printf("ICM20602 Raw Data (%d Hz)\n", IMU_OUTPUT_HZ);
        printf("Device: %s\n\n", ICM20602_IIO_PATH);
        printf("ACCEL_RAW\n");
        printf("  X: %8d\n", accel_x);
        printf("  Y: %8d\n", accel_y);
        printf("  Z: %8d\n\n", accel_z);
        printf("GYRO_RAW\n");
        printf("  X: %8d\n", gyro_x);
        printf("  Y: %8d\n", gyro_y);
        printf("  Z: %8d\n", gyro_z);
        fflush(stdout);

        nanosleep(&sleep_time, NULL);
    }
}

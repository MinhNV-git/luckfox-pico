#ifndef CALIBAPP_H
#define CALIBAPP_H

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <pthread.h>
#include <unistd.h>
#include <fcntl.h>
#include <termios.h>
#include <string.h>
#include <errno.h>

#include "crc16.h"

#define TOOLS_PATH_DEFAULT  "/dev/ttyS3"
#define CALIBAPP_BAUDRATE B115200
#define CALIBAPP_WRITE_FRAME_SIZE (64+2+2) // header(1) + size(1) + data(64) + crc(2)
#define CALIBAPP_READ_FRAME_SIZE (6+1+2) 
#define CALIBAPP_PERIODIC_UPDATED 10 //ms

#define CALIBAPP_HEADER_IMU     (0xAA)

typedef struct {
    int fb;
    int thread_running;
    pthread_t thread_id;
    pthread_mutex_t write_mutex;
    char port[64];
    int baudrate;
    uint8_t buffer_read[CALIBAPP_READ_FRAME_SIZE];
    uint8_t buffer_write[CALIBAPP_WRITE_FRAME_SIZE];
} CalibAppConfig;

int calibapp_init(void);
ssize_t calibapp_read(uint8_t *buffer, size_t size);
ssize_t calibapp_write(const uint8_t *data, size_t size);
void calibapp_deinit(void);

void calibapp_transfer_data(const uint8_t *data, size_t size);

#endif // CALIBAPP_H

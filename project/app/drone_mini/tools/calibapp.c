#include "calibapp.h"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <termios.h>
#include <string.h>
#include <errno.h>
#include <stdbool.h>
#include <sys/select.h>
#include <time.h>

#define CALIBAPP_RESPONSE_PREFIX "I can read"

static CalibAppConfig s_calibapp = {
    .fb = -1,
    .thread_running = 0,
    .thread_id = 0,
    .port = TOOLS_PATH_DEFAULT,
    .baudrate = CALIBAPP_BAUDRATE,
    .buffer_read = {0},
    .buffer_write = {0}
};

static int uart_init(CalibAppConfig *config) {
    int fd = open(config->port, O_RDWR | O_NOCTTY);
    if (fd < 0) {
        perror("Failed to open UART port"); 
        return -1;
    }

    struct termios options;
    if (tcgetattr(fd, &options) != 0) {
        perror("Failed to get UART attributes");
        close(fd);
        return -1;
    }
    
    cfsetispeed(&options, config->baudrate);
    cfsetospeed(&options, config->baudrate);
    
    options.c_cflag |= (CLOCAL | CREAD);
    options.c_cflag &= ~PARENB;
    options.c_cflag &= ~CSTOPB;
    options.c_cflag &= ~CSIZE;
    options.c_cflag |= CS8;
    options.c_lflag &= ~(ICANON | ECHO | ECHOE | ISIG);
    options.c_iflag &= ~(IXON | IXOFF | IXANY);
    options.c_oflag &= ~OPOST;
    options.c_cc[VMIN] = 0;
    options.c_cc[VTIME] = 1;
    
    if (tcsetattr(fd, TCSANOW, &options) != 0) {
        perror("Failed to set UART attributes");
        close(fd);
        return -1;
    }
    
    printf("UART initialized successfully on %s\n", config->port);
    return fd;
}

static int calibapp_wait_readable(int timeout_ms)
{
    fd_set readfds;
    struct timeval timeout;

    if (s_calibapp.fb < 0) {
        errno = ENODEV;
        return -1;
    }

    FD_ZERO(&readfds);
    FD_SET(s_calibapp.fb, &readfds);

    timeout.tv_sec = timeout_ms / 1000;
    timeout.tv_usec = (timeout_ms % 1000) * 1000;

    return select(s_calibapp.fb + 1, &readfds, NULL, NULL, &timeout);
}

static void calibapp_build_response(const uint8_t *payload, size_t payload_size, uint8_t *response, size_t *response_size)
{
    static const char prefix[] = CALIBAPP_RESPONSE_PREFIX;
    size_t prefix_size = sizeof(prefix) - 1;
    size_t copy_size = payload_size;

    if (response == NULL || response_size == NULL) {
        return;
    }

    if (copy_size > (size_t)(CALIBAPP_WRITE_FRAME_SIZE - prefix_size)) {
        copy_size = (size_t)(CALIBAPP_WRITE_FRAME_SIZE - prefix_size);
    }

    memcpy(response, prefix, prefix_size);
    if (payload != NULL && copy_size > 0U) {
        memcpy(response + prefix_size, payload, copy_size);
    }

    *response_size = prefix_size + copy_size;
}

static void *calibapp_worker_thread(void *arg)
{
    uint8_t read_buffer[CALIBAPP_READ_FRAME_SIZE];
    uint8_t response_buffer[CALIBAPP_WRITE_FRAME_SIZE];
    size_t total_read = 0;
    size_t response_size = 0;
    int wait_status;

    (void)arg;

    while (s_calibapp.thread_running) {
        total_read = 0;
        while (total_read < sizeof(read_buffer) && s_calibapp.thread_running) {
            wait_status = calibapp_wait_readable(CALIBAPP_PERIODIC_UPDATED);
            if (wait_status < 0) {
                if (errno == EINTR) {
                    continue;
                }
                perror("calibapp select failed");
                break;
            }

            if (wait_status == 0) {
                break;
            }

            ssize_t bytes_read = calibapp_read(read_buffer + total_read, sizeof(read_buffer) - total_read);
            if (bytes_read < 0) {
                if (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR) {
                    continue;
                }
                perror("calibapp read failed");
                break;
            }

            if (bytes_read == 0) {
                break;
            }

            total_read += (size_t)bytes_read;
        }

        if (total_read == 0) {
            continue;
        }
        pthread_mutex_lock(&s_calibapp.write_mutex);
        calibapp_write(s_calibapp.buffer_write, (s_calibapp.buffer_write[1] + 4));
        pthread_mutex_unlock(&s_calibapp.write_mutex);
    }

    return NULL;
}

int calibapp_init(void) {
    if (pthread_mutex_init(&s_calibapp.write_mutex, NULL) != 0) {
        perror("Failed to initialize calibapp write mutex");
        return -1;
    }

    s_calibapp.fb = uart_init(&s_calibapp);
    if (s_calibapp.fb < 0) {
        fprintf(stderr, "Failed to initialize UART\n");
        pthread_mutex_destroy(&s_calibapp.write_mutex);
        return -1;
    }

    s_calibapp.thread_running = 1;
    if (pthread_create(&s_calibapp.thread_id, NULL, calibapp_worker_thread, NULL) != 0) {
        perror("Failed to create calibapp thread");
        s_calibapp.thread_running = 0;
        close(s_calibapp.fb);
        s_calibapp.fb = -1;
        pthread_mutex_destroy(&s_calibapp.write_mutex);
        return -1;
    }

    return s_calibapp.fb;
}

ssize_t calibapp_read(uint8_t *buffer, size_t size)
{
    size_t read_size;
    ssize_t bytes_read;

    if (s_calibapp.fb < 0) {
        errno = ENODEV;
        return -1;
    }

    if (buffer == NULL || size == 0) {
        errno = EINVAL;
        return -1;
    }

    read_size = size;
    if (read_size > sizeof(s_calibapp.buffer_read)) {
        read_size = sizeof(s_calibapp.buffer_read);
    }

    bytes_read = read(s_calibapp.fb, s_calibapp.buffer_read, read_size);
    if (bytes_read <= 0) {
        return bytes_read;
    }

    memcpy(buffer, s_calibapp.buffer_read, (size_t)bytes_read);
    return bytes_read;
}

ssize_t calibapp_write(const uint8_t *data, size_t size)
{
    size_t write_size;

    if (s_calibapp.fb < 0) {
        errno = ENODEV;
        return -1;
    }

    if (data == NULL || size == 0) {
        errno = EINVAL;
        return -1;
    }

    write_size = size;
    if (write_size > sizeof(s_calibapp.buffer_write)) {
        write_size = sizeof(s_calibapp.buffer_write);
    }

    // memcpy(s_calibapp.buffer_write, data, write_size);
    return write(s_calibapp.fb, s_calibapp.buffer_write, write_size);
}

void calibapp_transfer_data(const uint8_t *data, size_t size)
{
    uint16_t crc=0;

    if (data == NULL || size == 0U) {
        return;
    }

    if(size > (CALIBAPP_WRITE_FRAME_SIZE - 4)) {
        printf("%s: Data size exceeds maximum frame size %d bytes\n", __func__, (CALIBAPP_WRITE_FRAME_SIZE - 4) );
        return;
    }

    pthread_mutex_lock(&s_calibapp.write_mutex);
    s_calibapp.buffer_write[0] = CALIBAPP_HEADER_IMU;
    s_calibapp.buffer_write[1] = (uint8_t)size;
    memcpy(s_calibapp.buffer_write + 2, data, size);
    
    crc = crc16_default(s_calibapp.buffer_write, (size_t)(size + 2));

    s_calibapp.buffer_write[size + 2] = (uint8_t)(crc & 0xFF);
    s_calibapp.buffer_write[size + 3] = (uint8_t)((crc >> 8) & 0xFF);
    pthread_mutex_unlock(&s_calibapp.write_mutex);
}

void calibapp_deinit(void)
{
    if (s_calibapp.thread_running) {
        s_calibapp.thread_running = 0;
        pthread_join(s_calibapp.thread_id, NULL);
        s_calibapp.thread_id = 0;
    }

    if (s_calibapp.fb >= 0) {
        close(s_calibapp.fb);
        s_calibapp.fb = -1;
    }

    pthread_mutex_destroy(&s_calibapp.write_mutex);
}

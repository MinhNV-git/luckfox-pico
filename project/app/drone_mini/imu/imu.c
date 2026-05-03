#include "imu.h"

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define IMU_IIO_DEVICE_PATH "/sys/bus/iio/devices/iio:device1"
#define IMU_DEVICE_NODE_PATH "/dev/iio:device1"
#define IMU_BUFFER_PATH IMU_IIO_DEVICE_PATH "/buffer"
#define IMU_SCAN_ELEMENTS_PATH IMU_IIO_DEVICE_PATH "/scan_elements"

#define IMU_DEFAULT_SAMPLING_FREQUENCY_HZ 100
#define IMU_DEFAULT_BUFFER_LENGTH 32
#define IMU_MAX_FRAME_SIZE 64

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

typedef struct {
    const char *enable_name;
    const char *index_name;
    const char *type_name;
    const char *label;
    ImuFieldId field_id;
    int index;
    int bits;
    int storage_bits;
    int shift;
    int is_signed;
    int is_big_endian;
    int storage_bytes;
    int frame_offset;
} ImuScanChannel;

typedef struct {
    int device_fd;
    int frame_size;
    int setup_done;
} ImuState;

static ImuScanChannel g_imu_scan_channels[] = {
    { "in_accel_x_en", "in_accel_x_index", "in_accel_x_type", "accel_x", IMU_FIELD_ACCEL_X, -1, 0, 0, 0, 0, 0, 0, -1 },
    { "in_accel_y_en", "in_accel_y_index", "in_accel_y_type", "accel_y", IMU_FIELD_ACCEL_Y, -1, 0, 0, 0, 0, 0, 0, -1 },
    { "in_accel_z_en", "in_accel_z_index", "in_accel_z_type", "accel_z", IMU_FIELD_ACCEL_Z, -1, 0, 0, 0, 0, 0, 0, -1 },
    { "in_anglvel_x_en", "in_anglvel_x_index", "in_anglvel_x_type", "gyro_x", IMU_FIELD_GYRO_X, -1, 0, 0, 0, 0, 0, 0, -1 },
    { "in_anglvel_y_en", "in_anglvel_y_index", "in_anglvel_y_type", "gyro_y", IMU_FIELD_GYRO_Y, -1, 0, 0, 0, 0, 0, 0, -1 },
    { "in_anglvel_z_en", "in_anglvel_z_index", "in_anglvel_z_type", "gyro_z", IMU_FIELD_GYRO_Z, -1, 0, 0, 0, 0, 0, 0, -1 },
    { "in_temp_en", "in_temp_index", "in_temp_type", "temp", IMU_FIELD_TEMP, -1, 0, 0, 0, 0, 0, 0, -1 }
};

static ImuState g_imu_state = {
    .device_fd = -1,
    .frame_size = 0,
    .setup_done = 0
};

static int imu_path_exists(const char *path)
{
    return access(path, F_OK) == 0;
}

static int imu_read_int(const char *path, int *value)
{
    FILE *file;

    if (value == NULL) {
        errno = EINVAL;
        return -1;
    }

    file = fopen(path, "r");
    if (file == NULL) {
        fprintf(stderr, "%s: fopen(%s) failed: %s\n", __func__, path, strerror(errno));
        return -1;
    }

    if (fscanf(file, "%d", value) != 1) {
        fprintf(stderr, "%s: failed to parse integer from %s\n", __func__, path);
        fclose(file);
        return -1;
    }

    fclose(file);
    return 0;
}

static int imu_read_string(const char *path, char *buffer, size_t buffer_size)
{
    FILE *file;

    if (buffer == NULL || buffer_size == 0U) {
        errno = EINVAL;
        return -1;
    }

    file = fopen(path, "r");
    if (file == NULL) {
        fprintf(stderr, "%s: fopen(%s) failed: %s\n", __func__, path, strerror(errno));
        return -1;
    }

    if (fgets(buffer, (int)buffer_size, file) == NULL) {
        fprintf(stderr, "%s: failed to read string from %s\n", __func__, path);
        fclose(file);
        return -1;
    }

    buffer[strcspn(buffer, "\r\n")] = '\0';
    fclose(file);
    return 0;
}

static int imu_write_string(const char *path, const char *value)
{
    int fd;
    ssize_t bytes_written;
    size_t value_len;

    fd = open(path, O_WRONLY);
    if (fd < 0) {
        fprintf(stderr, "%s: open(%s) failed: %s\n", __func__, path, strerror(errno));
        return -1;
    }

    value_len = strlen(value);
    bytes_written = write(fd, value, value_len);
    close(fd);

    if (bytes_written != (ssize_t)value_len) {
        fprintf(stderr, "%s: write(%s) failed: %s\n", __func__, path, strerror(errno));
        return -1;
    }

    return 0;
}

static int imu_write_int(const char *path, int value)
{
    char buffer[32];

    (void)snprintf(buffer, sizeof(buffer), "%d", value);
    return imu_write_string(path, buffer);
}

static int imu_enable_scan_element(const char *element_name)
{
    char path[256];

    (void)snprintf(path, sizeof(path), "%s/%s", IMU_SCAN_ELEMENTS_PATH, element_name);
    return imu_write_int(path, 1);
}

static int imu_read_scan_index(const char *index_name, int *index_value)
{
    char path[256];

    (void)snprintf(path, sizeof(path), "%s/%s", IMU_SCAN_ELEMENTS_PATH, index_name);
    return imu_read_int(path, index_value);
}

static int imu_read_scan_type(const char *type_name, char *buffer, size_t buffer_size)
{
    char path[256];

    (void)snprintf(path, sizeof(path), "%s/%s", IMU_SCAN_ELEMENTS_PATH, type_name);
    return imu_read_string(path, buffer, buffer_size);
}

static int imu_parse_type(ImuScanChannel *channel)
{
    char type_buffer[64];
    char endian[3];
    char sign_code;
    int bits;
    int storage_bits;
    int shift;

    if (channel == NULL) {
        errno = EINVAL;
        return -1;
    }

    if (imu_read_scan_type(channel->type_name, type_buffer, sizeof(type_buffer)) != 0) {
        return -1;
    }

    if (sscanf(type_buffer, "%2[^:]:%c%d/%d>>%d", endian, &sign_code, &bits, &storage_bits, &shift) != 5) {
        fprintf(stderr, "%s: unsupported IIO type format for %s: %s\n",
                __func__, channel->label, type_buffer);
        return -1;
    }

    channel->bits = bits;
    channel->storage_bits = storage_bits;
    channel->shift = shift;
    channel->is_signed = (sign_code == 's');
    channel->is_big_endian = (strcmp(endian, "be") == 0);
    channel->storage_bytes = storage_bits / 8;

    if (channel->storage_bits % 8 != 0 || channel->storage_bytes <= 0) {
        fprintf(stderr, "%s: unsupported storage width for %s: %s\n",
                __func__, channel->label, type_buffer);
        return -1;
    }

    return 0;
}

static int imu_build_frame_layout(void)
{
    size_t i;
    int max_index = -1;
    int current_offset = 0;
    size_t channel_count = sizeof(g_imu_scan_channels) / sizeof(g_imu_scan_channels[0]);

    for (i = 0; i < channel_count; ++i) {
        g_imu_scan_channels[i].frame_offset = -1;
        if (g_imu_scan_channels[i].index > max_index) {
            max_index = g_imu_scan_channels[i].index;
        }
    }

    if (max_index < 0) {
        fprintf(stderr, "%s: no valid channel indexes found\n", __func__);
        return -1;
    }

    {
        int scan_index;

        for (scan_index = 0; scan_index <= max_index; ++scan_index) {
            for (i = 0; i < channel_count; ++i) {
                if (g_imu_scan_channels[i].index == scan_index) {
                    g_imu_scan_channels[i].frame_offset = current_offset;
                    current_offset += g_imu_scan_channels[i].storage_bytes;
                }
            }
        }
    }

    if (current_offset <= 0 || current_offset > IMU_MAX_FRAME_SIZE) {
        fprintf(stderr, "%s: invalid frame size %d bytes\n", __func__, current_offset);
        return -1;
    }

    g_imu_state.frame_size = current_offset;
    return 0;
}

static int32_t imu_sign_extend(uint32_t value, int bits)
{
    uint32_t sign_mask;

    if (bits <= 0 || bits >= 32) {
        return (int32_t)value;
    }

    sign_mask = 1U << (bits - 1);
    if ((value & sign_mask) != 0U) {
        value |= ~((1U << bits) - 1U);
    }

    return (int32_t)value;
}

static int32_t imu_extract_channel_value(const uint8_t *frame, const ImuScanChannel *channel)
{
    uint32_t raw_value = 0U;
    uint32_t masked_value;
    int byte_index;

    for (byte_index = 0; byte_index < channel->storage_bytes; ++byte_index) {
        int source_index = channel->frame_offset + byte_index;
        int target_shift;

        if (channel->is_big_endian) {
            target_shift = (channel->storage_bytes - 1 - byte_index) * 8;
        } else {
            target_shift = byte_index * 8;
        }

        raw_value |= ((uint32_t)frame[source_index]) << target_shift;
    }

    raw_value >>= channel->shift;
    if (channel->bits >= 32) {
        masked_value = raw_value;
    } else {
        masked_value = raw_value & ((1U << channel->bits) - 1U);
    }

    if (channel->is_signed) {
        return imu_sign_extend(masked_value, channel->bits);
    }

    return (int32_t)masked_value;
}

static void imu_assign_value(S_IMU_DATA *imu_data, ImuFieldId field_id, int32_t value)
{
    int16_t sample_value = (int16_t)value;

    switch (field_id) {
        case IMU_FIELD_ACCEL_X:
            imu_data->accel_x = sample_value;
            break;
        case IMU_FIELD_ACCEL_Y:
            imu_data->accel_y = sample_value;
            break;
        case IMU_FIELD_ACCEL_Z:
            imu_data->accel_z = sample_value;
            break;
        case IMU_FIELD_GYRO_X:
            imu_data->gyro_x = sample_value;
            break;
        case IMU_FIELD_GYRO_Y:
            imu_data->gyro_y = sample_value;
            break;
        case IMU_FIELD_GYRO_Z:
            imu_data->gyro_z = sample_value;
            break;
        case IMU_FIELD_TEMP:
            imu_data->temp = sample_value;
            break;
        default:
            break;
    }
}

int imu_setup(void)
{
    char path[256];
    int status;
    size_t i;

    if (!imu_path_exists(IMU_IIO_DEVICE_PATH)) {
        fprintf(stderr, "%s: IIO device path not found: %s\n", __func__, IMU_IIO_DEVICE_PATH);
        return -1;
    }

    (void)snprintf(path, sizeof(path), "%s/enable", IMU_BUFFER_PATH);
    status = imu_write_int(path, 0);
    if (status != 0) {
        return -1;
    }

    (void)snprintf(path, sizeof(path), "%s/length", IMU_BUFFER_PATH);
    status = imu_write_int(path, IMU_DEFAULT_BUFFER_LENGTH);
    if (status != 0) {
        return -1;
    }

    for (i = 0; i < (sizeof(g_imu_scan_channels) / sizeof(g_imu_scan_channels[0])); ++i) {
        status = imu_enable_scan_element(g_imu_scan_channels[i].enable_name);
        if (status != 0) {
            return -1;
        }
    }

    (void)snprintf(path, sizeof(path), "%s/sampling_frequency", IMU_IIO_DEVICE_PATH);
    if (imu_path_exists(path)) {
        status = imu_write_int(path, IMU_DEFAULT_SAMPLING_FREQUENCY_HZ);
        if (status != 0) {
            return -1;
        }
    }

    for (i = 0; i < (sizeof(g_imu_scan_channels) / sizeof(g_imu_scan_channels[0])); ++i) {
        status = imu_read_scan_index(g_imu_scan_channels[i].index_name, &g_imu_scan_channels[i].index);
        if (status != 0) {
            return -1;
        }

        status = imu_parse_type(&g_imu_scan_channels[i]);
        if (status != 0) {
            return -1;
        }
    }

    status = imu_build_frame_layout();
    if (status != 0) {
        return -1;
    }

    if (g_imu_state.device_fd >= 0) {
        close(g_imu_state.device_fd);
        g_imu_state.device_fd = -1;
    }

    g_imu_state.device_fd = open(IMU_DEVICE_NODE_PATH, O_RDONLY);
    if (g_imu_state.device_fd < 0) {
        fprintf(stderr, "%s: open(%s) failed: %s\n", __func__, IMU_DEVICE_NODE_PATH, strerror(errno));
        return -1;
    }

    (void)snprintf(path, sizeof(path), "%s/enable", IMU_BUFFER_PATH);
    status = imu_write_int(path, 1);
    if (status != 0) {
        close(g_imu_state.device_fd);
        g_imu_state.device_fd = -1;
        return -1;
    }

    g_imu_state.setup_done = 1;

    printf("IMU buffer configured: device=%s, sample_hz=%d, buffer_length=%d, frame_size=%d\n",
           IMU_IIO_DEVICE_PATH,
           IMU_DEFAULT_SAMPLING_FREQUENCY_HZ,
           IMU_DEFAULT_BUFFER_LENGTH,
           g_imu_state.frame_size);
    printf("IMU frame mapping:\n");
    for (i = 0; i < (sizeof(g_imu_scan_channels) / sizeof(g_imu_scan_channels[0])); ++i) {
        printf("  %-7s -> index=%d offset=%d bytes=%d\n",
               g_imu_scan_channels[i].label,
               g_imu_scan_channels[i].index,
               g_imu_scan_channels[i].frame_offset,
               g_imu_scan_channels[i].storage_bytes);
    }

    return 0;
}

int imu_read_sample(S_IMU_DATA *imu_data)
{
    uint8_t frame[IMU_MAX_FRAME_SIZE];
    ssize_t bytes_read;
    size_t i;

    if (imu_data == NULL) {
        errno = EINVAL;
        return -1;
    }

    if (!g_imu_state.setup_done) {
        if (imu_setup() != 0) {
            return -1;
        }
    }

    if (g_imu_state.device_fd < 0 || g_imu_state.frame_size <= 0) {
        errno = ENODEV;
        return -1;
    }

    memset(imu_data, 0, sizeof(*imu_data));
    bytes_read = read(g_imu_state.device_fd, frame, (size_t)g_imu_state.frame_size);
    if (bytes_read < 0) {
        fprintf(stderr, "%s: read(%s) failed: %s\n", __func__, IMU_DEVICE_NODE_PATH, strerror(errno));
        return -1;
    }

    if (bytes_read != g_imu_state.frame_size) {
        fprintf(stderr, "%s: short frame read: got %zd expected %d\n",
                __func__, bytes_read, g_imu_state.frame_size);
        errno = EIO;
        return -1;
    }

    for (i = 0; i < (sizeof(g_imu_scan_channels) / sizeof(g_imu_scan_channels[0])); ++i) {
        int32_t value = imu_extract_channel_value(frame, &g_imu_scan_channels[i]);
        imu_assign_value(imu_data, g_imu_scan_channels[i].field_id, value);
    }

    return 0;
}

void init_imu(void)
{
    (void)imu_setup();
}

void read_imu_data(void)
{
    S_IMU_DATA imu_data;

    if (imu_read_sample(&imu_data) != 0) {
        return;
    }

    printf("IMU sample: acc=(%d,%d,%d) gyro=(%d,%d,%d) temp=%d\n",
           imu_data.accel_x,
           imu_data.accel_y,
           imu_data.accel_z,
           imu_data.gyro_x,
           imu_data.gyro_y,
           imu_data.gyro_z,
           imu_data.temp);
}

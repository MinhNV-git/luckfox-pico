#ifndef CRC16_H
#define CRC16_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef struct {
    uint16_t poly;
    uint16_t init;
    uint16_t xorout;
    bool refin;
    bool refout;
} Crc16Config;

typedef struct {
    Crc16Config config;
    uint16_t value;
} Crc16Context;

#define CRC16_CONFIG_CCITT_FALSE \
    { \
        .poly = 0x1021, \
        .init = 0xFFFF, \
        .xorout = 0x0000, \
        .refin = false, \
        .refout = false \
    }

#define CRC16_CONFIG_XMODEM \
    { \
        .poly = 0x1021, \
        .init = 0x0000, \
        .xorout = 0x0000, \
        .refin = false, \
        .refout = false \
    }

#define CRC16_CONFIG_MODBUS \
    { \
        .poly = 0xA001, \
        .init = 0xFFFF, \
        .xorout = 0x0000, \
        .refin = true, \
        .refout = true \
    }

#define CRC16_CONFIG_DEFAULT CRC16_CONFIG_MODBUS

void crc16_init(Crc16Context *ctx, const Crc16Config *config);
void crc16_update(Crc16Context *ctx, const uint8_t *data, size_t size);
uint16_t crc16_finalize(const Crc16Context *ctx);
uint16_t crc16_compute(const uint8_t *data, size_t size, const Crc16Config *config);
uint16_t crc16_default(const uint8_t *data, size_t size);

#endif // CRC16_H

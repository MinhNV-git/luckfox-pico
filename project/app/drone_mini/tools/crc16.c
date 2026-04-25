#include "crc16.h"

static uint16_t crc16_reflect16(uint16_t value)
{
    uint16_t result = 0;
    int i;

    for (i = 0; i < 16; ++i) {
        if ((value & (uint16_t)(1U << i)) != 0U) {
            result |= (uint16_t)(1U << (15 - i));
        }
    }

    return result;
}

void crc16_init(Crc16Context *ctx, const Crc16Config *config)
{
    if (ctx == NULL || config == NULL) {
        return;
    }

    ctx->config = *config;
    ctx->value = config->init;
}

void crc16_update(Crc16Context *ctx, const uint8_t *data, size_t size)
{
    size_t index;
    int bit;

    if (ctx == NULL || data == NULL || size == 0U) {
        return;
    }

    for (index = 0; index < size; ++index) {
        uint8_t current = data[index];

        if (ctx->config.refin) {
            ctx->value ^= current;
            for (bit = 0; bit < 8; ++bit) {
                if ((ctx->value & 0x0001U) != 0U) {
                    ctx->value = (uint16_t)((ctx->value >> 1) ^ ctx->config.poly);
                } else {
                    ctx->value >>= 1;
                }
            }
        } else {
            ctx->value ^= (uint16_t)(current << 8);
            for (bit = 0; bit < 8; ++bit) {
                if ((ctx->value & 0x8000U) != 0U) {
                    ctx->value = (uint16_t)((ctx->value << 1) ^ ctx->config.poly);
                } else {
                    ctx->value <<= 1;
                }
            }
        }
    }
}

uint16_t crc16_finalize(const Crc16Context *ctx)
{
    uint16_t crc;

    if (ctx == NULL) {
        return 0U;
    }

    crc = ctx->value;
    if (ctx->config.refin != ctx->config.refout) {
        crc = crc16_reflect16(crc);
    }

    return (uint16_t)(crc ^ ctx->config.xorout);
}

uint16_t crc16_compute(const uint8_t *data, size_t size, const Crc16Config *config)
{
    Crc16Context ctx;

    if (config == NULL) {
        return 0U;
    }

    crc16_init(&ctx, config);
    crc16_update(&ctx, data, size);
    return crc16_finalize(&ctx);
}

uint16_t crc16_default(const uint8_t *data, size_t size)
{
    static const Crc16Config default_config = CRC16_CONFIG_DEFAULT;

    return crc16_compute(data, size, &default_config);
}

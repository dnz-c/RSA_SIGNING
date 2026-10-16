#include <string.h>

#include "mbedtls/pk.h"
#include "mbedtls/ctr_drbg.h"

#define KEY_SECTOR_ADDR  0x08060000UL // Sector 7
#define KEY_MAGIC        0x13371337UL
#define KEY_DER_MAX      1400

typedef struct
{
	uint32_t magic, len, crc;
} key_hdr_t;

static uint32_t crc32_sw(const uint8_t *d, size_t n)
{
    uint32_t c = 0xFFFFFFFF;
    while (n--) { c ^= *d++; for (int i = 0; i < 8; i++) c = (c >> 1) ^ (0xEDB88320 & -(c & 1)); }
    return ~c;
}

static int keystore_load(mbedtls_pk_context *pk, mbedtls_ctr_drbg_context *drbg)
{
    const key_hdr_t *h = (const key_hdr_t *)KEY_SECTOR_ADDR;
    if (h->magic != KEY_MAGIC || h->len == 0 || h->len > KEY_DER_MAX) return -1;
    const uint8_t *der = (const uint8_t *)(KEY_SECTOR_ADDR + sizeof(key_hdr_t));
    if (crc32_sw(der, h->len) != h->crc) return -1;
    return mbedtls_pk_parse_key(pk, der, h->len, NULL, 0, mbedtls_ctr_drbg_random, drbg);
}

static int keystore_save(mbedtls_pk_context *pk)
{
    static uint8_t buf[KEY_DER_MAX];
    int len = mbedtls_pk_write_key_der(pk, buf, sizeof buf);
    if (len <= 0) return -1;
    const uint8_t *der = buf + sizeof buf - len;

    key_hdr_t h = { KEY_MAGIC, (uint32_t)len, crc32_sw(der, len) };

    HAL_FLASH_Unlock();
    FLASH_EraseInitTypeDef er = { .TypeErase = FLASH_TYPEERASE_SECTORS,
        .VoltageRange = FLASH_VOLTAGE_RANGE_3, .Sector = FLASH_SECTOR_7, .NbSectors = 1 };
    uint32_t err; int ret = 0;
    if (HAL_FLASHEx_Erase(&er, &err) != HAL_OK) ret = -1;

    uint32_t addr = KEY_SECTOR_ADDR;
    const uint32_t *w = (const uint32_t *)&h;
    for (int i = 0; ret == 0 && i < 3; i++, addr += 4)
        if (HAL_FLASH_Program(FLASH_TYPEPROGRAM_WORD, addr, w[i]) != HAL_OK) ret = -1;

    for (int i = 0; ret == 0 && i < len; i += 4, addr += 4)
    {
        uint32_t word = 0xFFFFFFFF;
        memcpy(&word, der + i, (len - i) < 4 ? (len - i) : 4);
        if (HAL_FLASH_Program(FLASH_TYPEPROGRAM_WORD, addr, word) != HAL_OK) ret = -1;
    }
    HAL_FLASH_Lock();
    memset(buf, 0, sizeof buf);
    return ret;
}

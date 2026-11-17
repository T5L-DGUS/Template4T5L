#include "v851_factory.h"

#if v851PROTOCOL_ENABLED

#include "v851_protocol.h"
#include <string.h>

#pragma optimize(8, size)

typedef char V851FactoryCountryLengthCheck[
    ((sizeof(v851FACTORY_SALES_COUNTRY) > 1U) &&
     (sizeof(v851FACTORY_SALES_COUNTRY) <= 8U)) ? 1 : -1];
typedef char V851FactoryProductKeyLengthCheck[
    ((sizeof(v851FACTORY_PRODUCT_KEY) > 1U) &&
     (sizeof(v851FACTORY_PRODUCT_KEY) <= 64U)) ? 1 : -1];
typedef char V851FactoryModelLengthCheck[
    ((sizeof(v851FACTORY_MODEL) > 1U) &&
     (sizeof(v851FACTORY_MODEL) <= 64U)) ? 1 : -1];
typedef char V851FactoryHardwareLengthCheck[
    ((sizeof(v851FACTORY_HARDWARE_VERSION) > 1U) &&
     (sizeof(v851FACTORY_HARDWARE_VERSION) <= 32U)) ? 1 : -1];
typedef char V851FactoryFirmwareLengthCheck[
    ((sizeof(v851FACTORY_FIRMWARE_VERSION) > 1U) &&
     (sizeof(v851FACTORY_FIRMWARE_VERSION) <= 32U)) ? 1 : -1];
typedef char V851FactoryFlashRangeCheck[
    (((V851_FACTORY_FLASH_ADDR & 1U) == 0U) &&
     (V851_FACTORY_RECORD_WORDS * 2U == V851_FACTORY_RECORD_BYTES) &&
     (V851_FACTORY_FLASH_ADDR + V851_FACTORY_RECORD_WORDS <= 0x0800U)) ? 1 : -1];

typedef struct {
    uint8_t tag;
    uint8_t offset;
    uint8_t capacity;
} V851FactoryFieldInfo;

static const V851FactoryFieldInfo code v851_factory_fields[5] = {
    {V851_TLV_TAG_FACTORY_SALES_COUNTRY, V851_FACTORY_COUNTRY_OFFSET, 8U},
    {V851_TLV_TAG_FACTORY_PRODUCT_KEY, V851_FACTORY_PRODUCT_OFFSET, 64U},
    {V851_TLV_TAG_FACTORY_MODEL, V851_FACTORY_MODEL_OFFSET, 64U},
    {V851_TLV_TAG_FACTORY_HW_VERSION, V851_FACTORY_HARDWARE_OFFSET, 32U},
    {V851_TLV_TAG_FACTORY_FW_VERSION, V851_FACTORY_FIRMWARE_OFFSET, 32U}
};
static const uint8_t code v851_factory_header[8] = {
    'V', 'F', 'C', '1', 0U, 1U, 1U, 0U
};
static uint8_t xdata v851_factory_record[V851_FACTORY_RECORD_BYTES];
static uint8_t v851_factory_valid;

static uint8_t V851FactoryTransfer(uint8_t operation)
{
    uint8_t status[2];

    T5lNorFlashRW(operation, V851_FACTORY_FLASH_BLOCK,
                  V851_FACTORY_FLASH_ADDR, V851_FACTORY_CACHE_VP,
                  v851_factory_record, V851_FACTORY_RECORD_WORDS);
    /* The existing driver returns no status. A nonzero command after its
     * timeout must not be mistaken for blank Flash or a verified write. */
    read_dgus_vp(sysDGUS_FLASH_RW_CMD, status, 1U);
    return (uint8_t)(status[0] == 0U);
}

static uint8_t V851FactoryStringLength(uint8_t index)
{
    uint8_t length;
    uint8_t value;

    for(length = 0U; length < v851_factory_fields[index].capacity; ++length)
    {
        value = v851_factory_record[v851_factory_fields[index].offset + length];
        if(value == 0U)
        {
            return length;
        }
        if((value < 0x20U) || (value > 0x7EU))
        {
            return 0U;
        }
    }
    return 0U;
}

static uint8_t V851FactoryValidate(void)
{
    uint8_t index;

    if((memcmp(v851_factory_record, v851_factory_header,
               sizeof(v851_factory_header)) != 0) ||
       (crc_16(v851_factory_record, V851_FACTORY_RECORD_BYTES) != 0U))
    {
        return 0U;
    }
    for(index = 0U; index < 5U; ++index)
    {
        if(V851FactoryStringLength(index) == 0U)
        {
            return 0U;
        }
    }
    return 1U;
}

void V851FactoryInit(void)
{
    uint16_t index;
    uint16_t crc;
    uint8_t blank;

    v851_factory_valid = 0U;
    if(V851FactoryTransfer(flashREAD_FLAG) == 0U)
    {
        return;
    }
    if(V851FactoryValidate() != 0U)
    {
        v851_factory_valid = 1U;
        return;
    }
    /* Never replace a damaged/unknown record with new firmware defaults. */
    blank = v851_factory_record[0];
    if((blank != 0U) && (blank != 0xFFU))
    {
        return;
    }
    for(index = 1U; index < V851_FACTORY_RECORD_BYTES; ++index)
    {
        if(v851_factory_record[index] != blank)
        {
            return;
        }
    }

    memset(v851_factory_record, 0, sizeof(v851_factory_record));
    memcpy(v851_factory_record, v851_factory_header, sizeof(v851_factory_header));
    memcpy(&v851_factory_record[V851_FACTORY_COUNTRY_OFFSET],
           v851FACTORY_SALES_COUNTRY, sizeof(v851FACTORY_SALES_COUNTRY));
    memcpy(&v851_factory_record[V851_FACTORY_PRODUCT_OFFSET],
           v851FACTORY_PRODUCT_KEY, sizeof(v851FACTORY_PRODUCT_KEY));
    memcpy(&v851_factory_record[V851_FACTORY_MODEL_OFFSET],
           v851FACTORY_MODEL, sizeof(v851FACTORY_MODEL));
    memcpy(&v851_factory_record[V851_FACTORY_HARDWARE_OFFSET],
           v851FACTORY_HARDWARE_VERSION, sizeof(v851FACTORY_HARDWARE_VERSION));
    memcpy(&v851_factory_record[V851_FACTORY_FIRMWARE_OFFSET],
           v851FACTORY_FIRMWARE_VERSION, sizeof(v851FACTORY_FIRMWARE_VERSION));
    crc = crc_16(v851_factory_record, V851_FACTORY_CRC_OFFSET);
    v851_factory_record[V851_FACTORY_CRC_OFFSET] = (uint8_t)crc;
    v851_factory_record[V851_FACTORY_CRC_OFFSET + 1U] = (uint8_t)(crc >> 8);
    if(V851FactoryTransfer(flashWRITE_FLAG) == 0U)
    {
        return;
    }
    if(V851FactoryTransfer(flashREAD_FLAG) != 0U)
    {
        v851_factory_valid = V851FactoryValidate();
    }
}

uint8_t V851FactoryReady(void)
{
    return v851_factory_valid;
}

uint16_t V851FactoryBuildFields(uint8_t *fields, uint16_t capacity)
{
    uint16_t offset;
    uint8_t index;

    if(v851_factory_valid == 0U)
    {
        return 0U;
    }
    offset = 0U;
    for(index = 0U; index < 5U; ++index)
    {
        if(V851TlvWriteBytes(fields, capacity, &offset,
                            v851_factory_fields[index].tag,
                            &v851_factory_record[v851_factory_fields[index].offset],
                            V851FactoryStringLength(index)) == 0U)
        {
            return 0U;
        }
    }
    return offset;
}

#endif /* v851PROTOCOL_ENABLED */

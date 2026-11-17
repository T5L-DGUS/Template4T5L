#ifndef V851_FACTORY_H
#define V851_FACTORY_H

#include "sys.h"

#if v851PROTOCOL_ENABLED

/* NOR word addresses, block 0. Byte range 0x0200..0x02FF, below 0x0800.
 * Do not include this range in settings reset or firmware download images. */
#define V851_FACTORY_FLASH_BLOCK                  0U
#define V851_FACTORY_FLASH_ADDR                   0x0100U
#define V851_FACTORY_RECORD_BYTES                 256U
#define V851_FACTORY_RECORD_WORDS                 128U
#define V851_FACTORY_CACHE_VP                     0x2000U

/* Byte offsets in the record; strings are ASCII, NUL terminated/zero padded. */
#define V851_FACTORY_COUNTRY_OFFSET               8U
#define V851_FACTORY_PRODUCT_OFFSET               16U
#define V851_FACTORY_MODEL_OFFSET                 80U
#define V851_FACTORY_HARDWARE_OFFSET              144U
#define V851_FACTORY_FIRMWARE_OFFSET              176U
#define V851_FACTORY_CRC_OFFSET                   254U
#define V851_FACTORY_FIELD_MAX_BYTES              210U

/* Called at startup after DGUS initialization. Only a blank record is written. */
void V851FactoryInit(void);
uint8_t V851FactoryReady(void);
uint16_t V851FactoryBuildFields(uint8_t *fields, uint16_t capacity);

#endif /* v851PROTOCOL_ENABLED */
#endif /* V851_FACTORY_H */

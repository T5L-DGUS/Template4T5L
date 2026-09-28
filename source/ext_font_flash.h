#ifndef EXT_FONT_FLASH_H
#define EXT_FONT_FLASH_H

#include "sys.h"

/* This package occupies external NOR font resources 16 through 21 only. */
#define EXT_FONT_FLASH_FIRST_ID          16U
#define EXT_FONT_FLASH_BLOCK_BYTES       0x00040000UL
#define EXT_FONT_FLASH_CAPACITY_BYTES    0x00180000UL
#define EXT_FONT_FLASH_BUFFER_BYTES      256U
#define EXT_FONT_FLASH_BUFFER_VP         0x0800U
#define EXT_FONT_FLASH_COMMAND_VP        0x00AAU
#define EXT_FONT_FLASH_TIMEOUT_MS        1000UL

/* Select the opcode documented for the installed GUI kernel. No probing. */
#ifndef EXT_FONT_FLASH_READ_MODE
#define EXT_FONT_FLASH_READ_MODE         0x01U
#endif

#define EXT_FONT_FLASH_IDLE              0U
#define EXT_FONT_FLASH_BUSY              1U
#define EXT_FONT_FLASH_READY             2U
#define EXT_FONT_FLASH_ERROR             3U

/* Call at startup. Does not cancel BUSY or clear a latched ERROR. */
void ExtFontFlashInit(void);

/*
 * Queue a read using a byte offset relative to resource 16.
 * Both offset and length must be multiples of four, 4 <= length <= 256,
 * and the request must fit within one 256 KiB resource and the package.
 * Returns 1 when queued, or 0 without hardware access for invalid/non-IDLE
 * requests. Other users must not issue 0x00AA operations while BUSY.
 */
uint8_t ExtFontFlashStart(uint32_t offset, uint16_t length);

/* Advance once from the foreground task loop; never waits for Flash. */
void ExtFontFlashTask(void);
uint8_t ExtFontFlashStatus(void);

/* Only bytes [0, requested length) are valid, and only while READY. */
uint8_t *ExtFontFlashData(void);

/* READY -> IDLE only. BUSY and ERROR cannot be cancelled/released. */
void ExtFontFlashRelease(void);

#endif

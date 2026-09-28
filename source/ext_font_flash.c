#include "ext_font_flash.h"
#include "timer.h"

#define EXT_FONT_FLASH_WAIT_IDLE         0U
#define EXT_FONT_FLASH_WAIT_COMPLETE     1U

static uint8_t xdata ExtFontFlashBuffer[EXT_FONT_FLASH_BUFFER_BYTES];
static uint8_t xdata ExtFontFlashCommand[12];
static uint32_t xdata ExtFontFlashStartTick;
static uint16_t xdata ExtFontFlashLength;
/* STARTUP_M5 leaves XDATA uncleared; use the C initialization table. */
static uint8_t xdata ExtFontFlashState = EXT_FONT_FLASH_IDLE;
static uint8_t xdata ExtFontFlashPhase;

static uint32_t ExtFontFlashTick(void)
{
    uint8_t saved_ea;
    uint32_t tick;

    saved_ea = EA;
    EA = 0;
    /* SysCurrentTick is not declared volatile by the existing timer API. */
    tick = *((volatile uint32_t *)&SysCurrentTick);
    EA = saved_ea;
    return tick;
}

void ExtFontFlashInit(void)
{
    /* An outstanding GUI command may survive any software-level reset. */
    if (ExtFontFlashState == EXT_FONT_FLASH_BUSY ||
        ExtFontFlashState == EXT_FONT_FLASH_ERROR)
    {
        return;
    }
    ExtFontFlashState = EXT_FONT_FLASH_IDLE;
    ExtFontFlashPhase = EXT_FONT_FLASH_WAIT_IDLE;
    ExtFontFlashLength = 0U;
}

uint8_t ExtFontFlashStart(uint32_t offset, uint16_t length)
{
    uint32_t block_offset;
    uint32_t word_offset;
    uint16_t word_count;

    if (ExtFontFlashState != EXT_FONT_FLASH_IDLE ||
        length == 0U || length > EXT_FONT_FLASH_BUFFER_BYTES ||
        (length & 3U) != 0U || (offset & 3UL) != 0UL ||
        offset >= EXT_FONT_FLASH_CAPACITY_BYTES)
    {
        return 0U;
    }
    if ((uint32_t)length > EXT_FONT_FLASH_CAPACITY_BYTES - offset)
    {
        return 0U;
    }
    block_offset = offset & (EXT_FONT_FLASH_BLOCK_BYTES - 1UL);
    if ((uint32_t)length > EXT_FONT_FLASH_BLOCK_BYTES - block_offset)
    {
        return 0U;
    }

    word_offset = block_offset >> 1;
    word_count = length >> 1;
    ExtFontFlashCommand[0] = 0x5AU;
    ExtFontFlashCommand[1] = EXT_FONT_FLASH_READ_MODE;
    ExtFontFlashCommand[2] = (uint8_t)(EXT_FONT_FLASH_FIRST_ID +
                                     (offset >> 18));
    ExtFontFlashCommand[3] = (uint8_t)(word_offset >> 16);
    ExtFontFlashCommand[4] = (uint8_t)(word_offset >> 8);
    ExtFontFlashCommand[5] = (uint8_t)word_offset;
    ExtFontFlashCommand[6] = (uint8_t)(EXT_FONT_FLASH_BUFFER_VP >> 8);
    ExtFontFlashCommand[7] = (uint8_t)EXT_FONT_FLASH_BUFFER_VP;
    ExtFontFlashCommand[8] = (uint8_t)(word_count >> 8);
    ExtFontFlashCommand[9] = (uint8_t)word_count;
    ExtFontFlashCommand[10] = 0U;
    ExtFontFlashCommand[11] = 0U;
    ExtFontFlashLength = length;
    ExtFontFlashStartTick = ExtFontFlashTick();
    ExtFontFlashPhase = EXT_FONT_FLASH_WAIT_IDLE;
    ExtFontFlashState = EXT_FONT_FLASH_BUSY;
    return 1U;
}

void ExtFontFlashTask(void)
{
    uint8_t status[2];
    uint32_t elapsed;

    if (ExtFontFlashState != EXT_FONT_FLASH_BUSY)
    {
        return;
    }
    elapsed = (uint32_t)(ExtFontFlashTick() - ExtFontFlashStartTick);
    if (elapsed >= EXT_FONT_FLASH_TIMEOUT_MS)
    {
        /* Do not clear the trigger: the GUI might still own the command. */
        ExtFontFlashState = EXT_FONT_FLASH_ERROR;
        return;
    }

    read_dgus_vp(EXT_FONT_FLASH_COMMAND_VP, status, 1U);
    if (status[0] != 0U)
    {
        return;
    }

    if (ExtFontFlashPhase == EXT_FONT_FLASH_WAIT_IDLE)
    {
        /* Publish the trigger last so the GUI cannot see partial parameters. */
        write_dgus_vp(EXT_FONT_FLASH_COMMAND_VP + 1U,
                      &ExtFontFlashCommand[2], 5U);
        ExtFontFlashPhase = EXT_FONT_FLASH_WAIT_COMPLETE;
        write_dgus_vp(EXT_FONT_FLASH_COMMAND_VP, ExtFontFlashCommand, 1U);
        return;
    }

    read_dgus_vp(EXT_FONT_FLASH_BUFFER_VP, ExtFontFlashBuffer,
                 (uint8_t)(ExtFontFlashLength >> 1));
    ExtFontFlashState = EXT_FONT_FLASH_READY;
}

uint8_t ExtFontFlashStatus(void)
{
    return ExtFontFlashState;
}

uint8_t *ExtFontFlashData(void)
{
    return ExtFontFlashBuffer;
}

void ExtFontFlashRelease(void)
{
    if (ExtFontFlashState == EXT_FONT_FLASH_READY)
    {
        ExtFontFlashState = EXT_FONT_FLASH_IDLE;
        ExtFontFlashLength = 0U;
    }
}

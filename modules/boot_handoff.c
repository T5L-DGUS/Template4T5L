#include "boot_handoff.h"

#define BOOT_HANDOFF_FRAME_HEAD_0         0xABU
#define BOOT_HANDOFF_FRAME_HEAD_1         0xCDU
#define BOOT_HANDOFF_FILE_INFO_CMD        0x04U
#define BOOT_HANDOFF_DOWNLOAD_MAX         20U
#define BOOT_HANDOFF_CONTROL_ADDR         0x0020U
#define BOOT_HANDOFF_RESET_ADDR           0x0004U

static uint8_t BootHandoffResetPending;

static uint16_t BootHandoffReadBe16(const uint8_t *bytes)
{
    return (uint16_t)(((uint16_t)bytes[0] << 8) | bytes[1]);
}

static uint32_t BootHandoffReadLe32(const uint8_t *bytes)
{
    return ((uint32_t)bytes[3] << 24) |
           ((uint32_t)bytes[2] << 16) |
           ((uint32_t)bytes[1] << 8) |
           (uint32_t)bytes[0];
}

uint8_t BootHandoffIsUpgradeFrame(const uint8_t *frame, uint16_t len)
{
    uint16_t status_index;
    uint16_t crc_index;

    if((frame == NULL) || (len < 25U) ||
       (len > BOOT_HANDOFF_FILE_INFO_MAX) ||
       (frame[0] != BOOT_HANDOFF_FRAME_HEAD_0) ||
       (frame[1] != BOOT_HANDOFF_FRAME_HEAD_1) ||
       (BootHandoffReadBe16(&frame[2]) != (uint16_t)(len - 4U)) ||
       (frame[4] != BOOT_HANDOFF_FILE_INFO_CMD) ||
       (frame[5] == 0U) || (frame[5] > BOOT_HANDOFF_DOWNLOAD_MAX) ||
       (frame[6] != 0U))
    {
        return 0U;
    }

    status_index = (uint16_t)(20U + frame[19]);
    crc_index = (uint16_t)(status_index + 1U);
    if(((uint16_t)(crc_index + 4U) != len) ||
       (frame[status_index] != 0U) ||
       (BootHandoffReadLe32(&frame[7]) == 0UL) ||
       (BootHandoffReadLe32(&frame[15]) == 0UL))
    {
        return 0U;
    }
    return 1U;
}

static void BootHandoffPersistAndReset(uint8_t *control_word)
{
    uint8_t reset_word[4] = {0x55U, 0xAAU, 0x5AU, 0xA5U};

    if(BootHandoffResetPending != 0U)
    {
        return;
    }
    BootHandoffResetPending = 1U;
    /* Persist the accepted snapshot. DgusToFlash waits for Flash completion. */
    write_dgus_vp(BOOT_HANDOFF_CONTROL_ADDR, control_word, 2U);
    DgusToFlash(flashMAIN_BLOCK_ORDER,
                BOOT_HANDOFF_CONTROL_ADDR,
                BOOT_HANDOFF_CONTROL_ADDR,
                2U);

    SysEnterCritical();
    write_dgus_vp(BOOT_HANDOFF_RESET_ADDR, reset_word, 2U);
    SysExitCritical();
}

void BootHandoffInit(void)
{
    uint8_t control_word[4] = {0U, 0U, 0U, 0U};

    BootHandoffResetPending = 0U;
    /* BOOT leaves its load command in VP. Consume it without changing NOR,
     * otherwise the first APP poll would restart the same application again. */
    write_dgus_vp(BOOT_HANDOFF_CONTROL_ADDR, control_word, 2U);
}

void BootHandoffTask(void)
{
    uint8_t control_word[4];

    if(BootHandoffResetPending != 0U)
    {
        return;
    }
    read_dgus_vp(BOOT_HANDOFF_CONTROL_ADDR, control_word, 2U);
    if(((control_word[0] == 0x5AU) && (control_word[1] == 0xA5U) &&
        (control_word[2] == 0x5AU) && (control_word[3] == 0xA5U)) ||
       ((control_word[0] == 0xAAU) && (control_word[1] == 0x55U)))
    {
        BootHandoffPersistAndReset(control_word);
    }
}

void BootHandoffRequestUpgrade(void)
{
    uint8_t control_word[4] = {0x5AU, 0xA5U, 0x5AU, 0xA5U};

    BootHandoffPersistAndReset(control_word);
}

#include "boot_handoff.h"

#define BOOT_HANDOFF_CONTROL_ADDR         0x0020U
#define BOOT_HANDOFF_RESET_ADDR           0x0004U

static uint8_t BootHandoffResetPending;

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

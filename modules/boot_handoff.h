#ifndef BOOT_HANDOFF_H
#define BOOT_HANDOFF_H

#include "sys.h"

#define BOOT_HANDOFF_FILE_INFO_MAX       280U
#define BOOT_HANDOFF_TASK_INTERVAL       10U

/* Clear the consumed VP control at APP startup; preserve the NOR boot target. */
void BootHandoffInit(void);
/* Poll VP 0020: 5AA55AA5 upgrades, AA55xxxx loads a specified 4KB block. */
void BootHandoffTask(void);
uint8_t BootHandoffIsUpgradeFrame(const uint8_t *frame, uint16_t len);
void BootHandoffRequestUpgrade(void);

#endif /* BOOT_HANDOFF_H */

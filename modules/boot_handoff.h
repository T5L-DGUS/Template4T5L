#ifndef BOOT_HANDOFF_H
#define BOOT_HANDOFF_H

#include "sys.h"

#define BOOT_HANDOFF_TASK_INTERVAL       10U

/* Clear the consumed VP control at APP startup; preserve the NOR boot target. */
void BootHandoffInit(void);
/* Poll VP 0020: 5AA55AA5 upgrades, AA55xxxx loads a specified 4KB block. */
void BootHandoffTask(void);

#endif /* BOOT_HANDOFF_H */

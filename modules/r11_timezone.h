#ifndef R11_TIMEZONE_H
#define R11_TIMEZONE_H

#include "sys.h"
#include "uart.h"

#if sysADVERTISE_MODE_ENABLED
/* Page 110: UTC display, save/cancel/enter, east/west, integer hours and status. */
#define TIMEZONE_TEXT_ADDR       0x06F0U
#define TIMEZONE_COMMAND_ADDR    0x06F8U
#define TIMEZONE_SIGN_ADDR       0x06F9U
#define TIMEZONE_HOUR_ADDR       0x06FAU
#define TIMEZONE_STATUS_ADDR     0x06FBU
#define TIMEZONE_STORAGE_ADDR    0x06FCU
#define TIMEZONE_COMMAND_SAVE    1U
#define TIMEZONE_COMMAND_CANCEL  2U
#define TIMEZONE_COMMAND_ENTER   3U
#define TIMEZONE_STATUS_LOADED   0U
#define TIMEZONE_STATUS_PENDING  1U
#define TIMEZONE_STATUS_SYNCED   2U
#define TIMEZONE_STATUS_INVALID  3U
#define cmdSET_TIMEZONE          0xF4U

void R11TimezoneInit(void);
void R11TimezoneTask(void);
void R11TimezoneSync(void);
void UartR11UserTimezoneProtocol(UART_TYPE *uart, uint8_t *frame, uint16_t len);
#endif

#endif

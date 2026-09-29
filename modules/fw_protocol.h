#ifndef FW_PROTOCOL_H
#define FW_PROTOCOL_H
#ifndef FW_PROTOCOL_TEST
#include "sys.h"
#include "uart.h"
#endif
#define FW_MIRROR_VP       0x3600
#define FW_WRITE_ADDR_VP   0x3700
#define FW_WRITE_VALUE_VP  0x3701
#define FW_WRITE_TRIGGER_VP 0x3702
#define FW_WRITE_RESULT_VP 0x3703
#define FW_LINK_VP         0x3704
#define FW_ERROR_VP        0x3705
#define FW_EDIT_VP         0x3680
#define FW_UI_EVENT_VP     0x3710
#define FW_UI_RESULT_VP    0x3711
#define FW_UI_READY_VP     0x3712
#define FW_UI_ALARM_VP     0x3713
#define FW_UI_STATUS_VP    0x3714
#define FW_UI_MODE_VP      0x371C
#define FW_UI_PARTS_VP     0x371D
#define FW_UI_STATUS_ICON_VP 0x371E
#define FW_UI_LIVE_VP      0x3720
#define FW_UI_EDIT_TEXT_VP 0x3750
#define USB_VIDEO_VP       0x3780
#define USB_SOFTWARE_VP    0x3781
#define USB_COMPLETE_VP    0x3782
#define USB_VIDEO_GO_VP    0x3783
#define USB_SOFTWARE_GO_VP 0x3784
void FwProtocolInit(void);
void FwProtocolTask(void);
void FwProtocolFeed(uint8_t *bytes, uint16_t length);
void FwUsbProtocol(UART_TYPE *uart, uint8_t *frame, uint16_t length);
#endif

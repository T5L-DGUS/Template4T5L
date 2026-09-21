#ifndef FW_TEST_PLATFORM_H
#define FW_TEST_PLATFORM_H
#include <stdint.h>
#define code
#define xdata
#define sysBEAUTY_MODE_ENABLED 0
#define sysN5CAMERA_MODE_ENABLED 0
#define sysADVERTISE_MODE_ENABLED 1
typedef struct { uint8_t id; } UART_TYPE;
extern UART_TYPE Uart2, Uart_R11;
extern uint8_t ET0;
uint32_t GetSysTick(void);
uint16_t ReadPageId(void);
void write_dgus_vp(uint32_t address, uint8_t *bytes, uint16_t count);
void read_dgus_vp(uint32_t address, uint8_t *bytes, uint8_t count);
uint16_t crc_16(uint8_t *bytes, uint16_t length);
void SendModbusReadHoldingRegistersFrame(UART_TYPE *, uint8_t, uint16_t, uint16_t);
void SendModbusWriteSingleRegisterFrame(UART_TYPE *, uint8_t, uint16_t, uint16_t);
void UartSendData(UART_TYPE *, uint8_t *, uint16_t);
#include "../modules/fw_protocol.h"
#endif

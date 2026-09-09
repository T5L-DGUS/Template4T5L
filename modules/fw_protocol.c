#ifdef FW_PROTOCOL_TEST
#include "fw_test_platform.h"
#else
#include "fw_protocol.h"
#include "modbus.h"
#include "timer.h"
#endif

/* UART2 is a dedicated Modbus master. VP values preserve wire units. */
#define FW_ADDRESS 1
#define FW_TIMEOUT 500UL
#define FW_PERIOD 1000UL
#define FW_ERR_TIMEOUT 0x0100
#define FW_ERR_VALUE   0x0101
#define FW_ERR_BUSY    0x0102
typedef struct { uint8_t first; uint8_t count; } FwRange;
static code FwRange ranges[] = {
    {0,2}, {3,3}, {8,2}, {11,1}, {13,2}, {16,4}, {21,1},
    {30,8}, {39,7}, {50,4}, {69,15}, {85,1}
};
static uint8_t xdata rx[64];
static uint8_t rx_size;
static uint8_t command, range_index, video_ready, software_ready;
static uint16_t request_address, request_value, request_count;
static uint32_t sent_at, cycle_at, last_rx;

static uint32_t FwNow(void)
{
    uint32_t now;
    uint8_t enabled = ET0;
    ET0 = 0;
    now = GetSysTick();
    ET0 = enabled;
    return now;
}

static void FwSet(uint16_t address, uint16_t value)
{
    write_dgus_vp(address, (uint8_t *)&value, 1);
}

static uint16_t FwGet(uint16_t address)
{
    uint16_t value;
    read_dgus_vp(address, (uint8_t *)&value, 1);
    return value;
}

/* Table R/W cells are merged: 0..29 and 69..88; reserved cells are excluded. */
static uint8_t FwWritable(uint16_t address, uint16_t value)
{
    switch(address) {
        case 0: return value >= 3 && value <= 34;
        case 1: return value >= 3 && value <= 60;
        case 3: return value >= 10 && value <= 50;
        case 4: case 11: case 17: case 18:
        case 69: case 70: case 71: case 76: case 78: case 79: case 82:
            return value <= 1;
        case 5: return (int16_t)value >= -150 && (int16_t)value <= 150;
        case 8: return value >= 50 && value <= 999;
        case 9: return value <= 999;
        case 13: case 75: return 1;
        case 14: return value <= 99;
        case 16: return value >= 1 && value <= 99;
        case 19: return value >= 10 && value <= 60;
        case 21: return value <= 50;
        case 72: return value >= 800 && value <= 3000;
        case 73: return value >= 50 && value <= 150;
        case 74: return value >= 20 && value <= 100;
        case 77: return value <= 30000;
        case 80: return value >= 13 && value <= 34;
        case 81: return value >= 90 && value <= 110;
        case 83: return value >= 650 && value <= 750;
        case 85: return value >= 10 && value <= 60;
        default: return 0;
    }
}

static void FwFinish(uint16_t error)
{
    FwSet(FW_LINK_VP, error ? 2 : 1);
    FwSet(FW_ERROR_VP, error);
    if(command == 6) FwSet(FW_WRITE_RESULT_VP, error ? 3 : 2);
    command = 0;
}

static void FwReply(uint8_t *frame, uint8_t length)
{
    uint16_t i, value;
    if(!command || frame[0] != FW_ADDRESS) return;
    if(FwNow() - sent_at >= FW_TIMEOUT) return;
    if(frame[1] == (command | 0x80) && length == 5) {
        if(frame[2]) FwFinish(frame[2]);
        return;
    }
    if(frame[1] != command) return;
    if(command == 3 && frame[2] == request_count * 2 && length == frame[2] + 5) {
        for(i = 0; i < request_count; ++i) {
            value = ((uint16_t)frame[3 + i * 2] << 8) | frame[4 + i * 2];
            FwSet(FW_MIRROR_VP + request_address + i, value);
        }
        FwFinish(0);
    } else if(command == 6 && length == 8 &&
              (((uint16_t)frame[2] << 8) | frame[3]) == request_address &&
              (((uint16_t)frame[4] << 8) | frame[5]) == request_value) {
        FwSet(FW_MIRROR_VP + request_address, request_value);
        FwFinish(0);
    }
}

/* Persistent UART2 assembler; never interpret an 06 echo as an 03 response. */
void FwProtocolFeed(uint8_t *bytes, uint16_t length)
{
    uint16_t index, crc;
    uint8_t needed, drop, i;
    uint32_t now = FwNow();
    if(now - last_rx >= FW_TIMEOUT) rx_size = 0;
    last_rx = now;
    for(index = 0; index < length; ++index) {
        if(rx_size == sizeof(rx)) rx_size = 0;
        rx[rx_size++] = bytes[index];
        while(rx_size) {
            drop = 0;
            if(rx[0] != FW_ADDRESS) drop = 1;
            else if(rx_size < 2) break;
            else {
                if(rx[1] == 3) {
                    if(rx_size < 3) break;
                    needed = rx[2] + 5;
                    if(rx[2] == 0 || rx[2] > sizeof(rx) - 5 || (rx[2] & 1)) drop = 1;
                } else if(rx[1] == 6) needed = 8;
                else if(rx[1] == 0x83 || rx[1] == 0x86) needed = 5;
                else drop = 1;
                if(!drop) {
                    if(rx_size < needed) break;
                    crc = crc_16(rx, needed - 2);
                    if(rx[needed - 2] == (uint8_t)crc && rx[needed - 1] == (uint8_t)(crc >> 8)) {
                        FwReply(rx, needed);
                        drop = needed;
                    } else drop = 1;
                }
            }
            for(i = drop; i < rx_size; ++i) rx[i - drop] = rx[i];
            rx_size -= drop;
        }
    }
}

void FwUsbProtocol(UART_TYPE *uart, uint8_t *frame, uint16_t length)
{
#if sysBEAUTY_MODE_ENABLED || sysN5CAMERA_MODE_ENABLED || sysADVERTISE_MODE_ENABLED
    if(uart != &Uart_R11 || length != 6 || frame[0] != 0xAA || frame[1] != 0x55 ||
       frame[2] != 0 || frame[3] != 2 || frame[4] != 0xFB) return;
    switch(frame[5]) {
        case 0:
            video_ready = software_ready = 0;
            FwSet(USB_VIDEO_VP, 0); FwSet(USB_SOFTWARE_VP, 0);
            FwSet(USB_COMPLETE_VP, 0);
            FwSet(USB_VIDEO_GO_VP, 0); FwSet(USB_SOFTWARE_GO_VP, 0);
            break;
        case 1:
            if(!video_ready) FwSet(USB_VIDEO_GO_VP, 0);
            video_ready = 1; FwSet(USB_VIDEO_VP, 1); FwSet(USB_COMPLETE_VP, 0);
            break;
        case 2:
            if(!software_ready) FwSet(USB_SOFTWARE_GO_VP, 0);
            software_ready = 1; FwSet(USB_SOFTWARE_VP, 1);
            break;
        case 3:
            video_ready = 0;
            FwSet(USB_VIDEO_VP, 0); FwSet(USB_VIDEO_GO_VP, 0); FwSet(USB_COMPLETE_VP, 1);
            break;
        default: break;
    }
#endif
}

void FwProtocolInit(void)
{
    uint16_t i;
    command = rx_size = range_index = video_ready = software_ready = 0;
    for(i = FW_WRITE_ADDR_VP; i <= FW_ERROR_VP; ++i) FwSet(i, 0);
    for(i = USB_VIDEO_VP; i <= USB_SOFTWARE_GO_VP; ++i) FwSet(i, 0);
    for(i = 0; i <= 88; ++i) FwSet(FW_MIRROR_VP + i, 0);
    cycle_at = FwNow() - FW_PERIOD;
}

void FwProtocolTask(void)
{
    uint16_t trigger, address, value;
    uint32_t now = FwNow();
#if sysBEAUTY_MODE_ENABLED || sysN5CAMERA_MODE_ENABLED || sysADVERTISE_MODE_ENABLED
    uint8_t fb[6] = {0xAA, 0x55, 0, 2, 0xFB, 1};
    trigger = FwGet(USB_VIDEO_GO_VP);
    if(trigger) {
        FwSet(USB_VIDEO_GO_VP, 0);
        if(trigger == 1 && video_ready) UartSendData(&Uart_R11, fb, 6);
    }
    trigger = FwGet(USB_SOFTWARE_GO_VP);
    if(trigger) {
        FwSet(USB_SOFTWARE_GO_VP, 0);
        fb[5] = 2;
        if(trigger == 1 && software_ready) UartSendData(&Uart_R11, fb, 6);
    }
#endif
    if(command && now - sent_at >= FW_TIMEOUT) {
        FwFinish(FW_ERR_TIMEOUT);
        rx_size = 0;
        cycle_at = now;
        range_index = 0;
    }
    trigger = FwGet(FW_WRITE_TRIGGER_VP);
    if(trigger && command != 3) {
        FwSet(FW_WRITE_TRIGGER_VP, 0);
        address = FwGet(FW_WRITE_ADDR_VP);
        value = FwGet(FW_WRITE_VALUE_VP);
        if(command || trigger != 1 || !FwWritable(address, value)) {
            FwSet(FW_WRITE_RESULT_VP, 3);
            FwSet(FW_ERROR_VP, command ? FW_ERR_BUSY : FW_ERR_VALUE);
        } else {
            request_address = address; request_value = value;
            command = 6; sent_at = now; rx_size = 0;
            FwSet(FW_WRITE_RESULT_VP, 1);
            SendModbusWriteSingleRegisterFrame(&Uart2, FW_ADDRESS, address, value);
        }
        return;
    }
    if(command) return;
    if(range_index == 0) {
        if(now - cycle_at < FW_PERIOD) return;
        cycle_at = now;
    }
    request_address = ranges[range_index].first;
    request_count = ranges[range_index].count;
    if(++range_index == sizeof(ranges) / sizeof(ranges[0])) range_index = 0;
    command = 3; sent_at = now; rx_size = 0;
    SendModbusReadHoldingRegistersFrame(&Uart2, FW_ADDRESS, request_address, request_count);
}

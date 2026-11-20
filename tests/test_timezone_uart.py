"""Exercise production UART framing with V851 F4 replies and fragmented input."""
from pathlib import Path
import re
import subprocess
import unittest


ROOT = Path(__file__).resolve().parents[1]
BUILD = ROOT / 'tests/.build'


def function(source, name):
    match = re.search(r'^void ' + name + r'\([^\n]*\)\s*\{', source, re.M)
    assert match, name
    start = match.start()
    begin = source.index('{', start)
    depth = 1
    end = begin + 1
    while depth:
        depth += (source[end] == '{') - (source[end] == '}')
        end += 1
    return source[start:end]


class TimezoneUartTest(unittest.TestCase):
    def test_actual_uart_splitter(self):
        source = (ROOT / 'source/uart.c').read_text(encoding='utf-8-sig')
        prelude = r'''
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#define uartUART2_ENABLED 1
#define uartUART3_ENABLED 0
#define uartUART4_ENABLED 1
#define uartUART5_ENABLED 1
#define uartUART_COMMON_FRAME_SIZE 64U
#define uartUART2_RXBUF_SIZE 64U
#define uartUART4_RXBUF_SIZE 64U
#define uartUART5_RXBUF_SIZE 64U
#define sysBEAUTY_MODE_ENABLED 0
#define sysN5CAMERA_MODE_ENABLED 0
#define sysADVERTISE_MODE_ENABLED 1
#define R11_WIFI_ENABLED 1
#define uartMODBUS_PROTOCOL_ENABLED 1
#define uartTA_PROTOCOL_ENABLED 1
#define modbusSLAVE_ADDRESS 0x42U
#define UART_NON_REC 0U
#define UART_RECING 1U
typedef struct {
    uint16_t RxHead, RxTail;
    uint8_t RxFlag, RxTimeout;
} UART_TYPE;
static UART_TYPE Uart2, Uart4, Uart5;
#define Uart_R11 Uart5
static uint8_t Uart2RxBuffer[64], Uart4RxBuffer[64], Uart5RxBuffer[64];
static uint8_t captured[16][64];
static uint16_t captured_len[16], calls, dwin_calls, ta_calls, modbus_calls;
static void UartStandardDwin8283Protocal(UART_TYPE *u, uint8_t *f, uint16_t n) {
    (void)u; (void)f; assert(n >= 4U); dwin_calls++;
}
static void UartR11UserWifiProtocol(UART_TYPE *u, uint8_t *f, uint16_t n) {
    (void)u; (void)f; assert(n >= 5U);
}
static void UartR11UserVideoProtocol(UART_TYPE *u, uint8_t *f, uint16_t n) {
    (void)u; (void)f; assert(n >= 5U);
}
static void UartR11UserAdvertiseProtocol(UART_TYPE *u, uint8_t *f, uint16_t n) {
    (void)u; assert(calls < 16U && n <= 64U);
    memcpy(captured[calls], f, n); captured_len[calls++] = n;
}
static void UartStandardModbusRTUProtocal(UART_TYPE *u, uint8_t *f, uint16_t n) {
    (void)u; (void)f; assert(n >= 5U); modbus_calls++;
}
static void UartStandardTAProtocol(UART_TYPE *u, uint8_t *f, uint16_t n) {
    (void)u; (void)f; assert(n >= 6U); ta_calls++;
}
'''
        checks = r'''
static uint8_t reply[13] = {0xAA,0x55,0,4,0xF4,1,8,26,11,20,13,14,15};
static const uint8_t normal[6] = {0xAA,0x55,0,2,0x64,4};
static void reset(uint16_t start) {
    memset(&Uart2,0,sizeof(Uart2)); memset(&Uart4,0,sizeof(Uart4));
    memset(&Uart5,0,sizeof(Uart5));
    Uart5.RxHead=Uart5.RxTail=start;
    memset(captured,0,sizeof(captured)); memset(captured_len,0,sizeof(captured_len));
    calls=dwin_calls=ta_calls=modbus_calls=0;
}
static void feed(UART_TYPE *u, const uint8_t *bytes, uint16_t n) {
    uint8_t *buffer=(u==&Uart2)?Uart2RxBuffer:(u==&Uart4)?Uart4RxBuffer:Uart5RxBuffer;
    while(n--) {
        buffer[u->RxHead++]=*bytes++; u->RxHead%=64U; u->RxFlag=UART_RECING;
        assert(u->RxHead!=u->RxTail);
    }
    u->RxTimeout=0; UartReadFrame(u);
}
static void expect_reply(unsigned call) {
    assert(captured_len[call]==13 && !memcmp(captured[call],reply,13));
}
static void test_fragments(void) {
    unsigned canonical, split, start, j;
    for(canonical=0;canonical<2;canonical++) {
        reply[3]=canonical?9:4;
        for(start=0;start<64;start++) for(split=1;split<13;split++) {
            reset(start); feed(&Uart5,reply,split);
            assert(calls==0 && Uart5.RxTail==start && Uart5.RxFlag==UART_RECING);
            UartReadFrame(&Uart5); assert(calls==0 && Uart5.RxTail==start);
            feed(&Uart5,reply+split,13-split); assert(calls==1); expect_reply(0);
            assert(Uart5.RxTail==Uart5.RxHead && Uart5.RxFlag==UART_NON_REC);
        }
        reset(61);
        for(j=0;j<13;j++) {feed(&Uart5,reply+j,1); assert(calls==(j==12));}
        expect_reply(0);
    }
    puts("PASS both F4 lengths, every idle split and ring position, single-byte fragments");
}
static void test_concatenation(void) {
    uint8_t bytes[32]; unsigned split;
    reply[3]=4;
    for(split=1;split<13;split++) {
        reset(60); memcpy(bytes,normal,6); memcpy(bytes+6,reply,split);
        feed(&Uart5,bytes,6+split);
        assert(calls==1 && captured_len[0]==6 && Uart5.RxTail==2);
        memcpy(bytes,reply+split,13-split); memcpy(bytes+13-split,normal,6);
        feed(&Uart5,bytes,19-split); assert(calls==3); expect_reply(1);
        assert(captured_len[2]==6 && Uart5.RxTail==Uart5.RxHead);
    }
    reset(0); bytes[0]=0x23; bytes[1]=0x56; bytes[2]=0x22;
    memcpy(bytes+3,reply,13); memcpy(bytes+16,reply,13);
    feed(&Uart5,bytes,29); assert(calls==2); expect_reply(0); expect_reply(1);
    puts("PASS adjacent ordinary/F4 replies, trailing fragments, and discarded noise");
}
static void test_other_uart_and_short_headers(void) {
    static const uint8_t dwin[]={0x5A,0xA5,4,0x82,0,0x10,1};
    static const uint8_t invalid[][4]={{0x5A,0,0,0},{0x5A,0xA5,0,0},
        {0xAA,0xCC,0,0},{0x42,0,0,0}};
    uint8_t echo[7]={0xAA,0x55,0,3,0xF4,1,8};
    unsigned k,n;
    reset(0); feed(&Uart2,reply,13); assert(calls==1 && captured_len[0]==8);
    reset(0); feed(&Uart4,reply,13); assert(calls==1 && captured_len[0]==8);
    reset(0); feed(&Uart5,echo,7); assert(calls==1 && captured_len[0]==7);
    reset(0); feed(&Uart5,dwin,sizeof(dwin)); assert(dwin_calls==1 && calls==0);
    for(k=0;k<4;k++)for(n=1;n<=3;n++) {
        reset(0); feed(&Uart5,invalid[k],n);
        assert(!calls && !dwin_calls && !ta_calls && !modbus_calls);
    }
    reset(0); Uart5.RxFlag=UART_RECING; UartReadFrame(&Uart5); assert(calls==0);
    puts("PASS F4 quirk limited to UART5, short echoes, DGUS, empty and partial headers");
}
int main(void) {test_fragments();test_concatenation();test_other_uart_and_short_headers();return 0;}
'''
        BUILD.mkdir(exist_ok=True)
        path = BUILD / 'timezone_uart.c'
        exe = BUILD / 'timezone_uart.exe'
        path.write_text(prelude + function(source, 'UartReadFrame') + checks, encoding='utf-8')
        compiler = ROOT.parent / 'tools/llvm-mingw-20250709-ucrt-x86_64/bin/clang.exe'
        subprocess.run([str(compiler), '-std=c99', '-Wall', '-Wextra', '-Werror',
                        str(path), '-o', str(exe)], check=True)
        subprocess.run([str(exe)], check=True)


if __name__ == '__main__':
    unittest.main()

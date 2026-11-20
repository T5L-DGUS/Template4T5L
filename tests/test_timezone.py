"""Exercise production timezone persistence, UART payloads and the RX8130 RTC.

The harness replaces only hardware IO. DGUS and Flash are byte addressed so
8051 big-endian words remain explicit when this test runs on a little-endian PC.
"""
from pathlib import Path
import os
import re
import subprocess
import unittest

ROOT = Path(__file__).resolve().parents[1]
BUILD = ROOT / 'tests/.build'


def without_local_includes(source):
    return re.sub(r'^\s*#include\s+"[^\n]*$', '', source, flags=re.M)


HARDWARE = r'''
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#define sysADVERTISE_MODE_ENABLED 1
#define code
#define xdata
#define __NOP() ((void)0)
#define rtcRX_8130
#define GPIO_BYTE_SET_OUT(a,b) ((void)0)
#define flashMAIN_BLOCK_ORDER 0
#define flashREAD_FLAG 0x5a
#define flashWRITE_FLAG 0xa5
typedef struct { uint8_t id; } UART_TYPE;
static UART_TYPE Uart_R11, other_uart;
static uint8_t vp[0x20000], flash[0x20000], chip[256], tx[64];
static unsigned flash_reads, flash_writes, tx_calls, tx_len, rtc_writes;
static unsigned event_serial, last_flash_event, last_tx_event;
void read_dgus_vp(uint16_t address,uint8_t *out,uint16_t words) {
    assert((unsigned)address*2+words*2<=sizeof(vp));
    memcpy(out,vp+(unsigned)address*2,(unsigned)words*2);
}
void write_dgus_vp(uint16_t address,uint8_t *input,uint16_t words) {
    assert((unsigned)address*2+words*2<=sizeof(vp));
    memcpy(vp+(unsigned)address*2,input,(unsigned)words*2);
}
void T5lNorFlashRW(uint8_t mode,uint8_t block,uint16_t address,
                   uint16_t scratch,uint8_t *bytes,uint16_t words) {
    assert(block==0 && (address&1)==0 && (scratch&1)==0 && (words&1)==0);
    assert((unsigned)address*2+words*2<=sizeof(flash));
    if(mode==flashREAD_FLAG) {
        ++flash_reads;memcpy(vp+(unsigned)scratch*2,flash+(unsigned)address*2,words*2);
        if(bytes)memcpy(bytes,flash+(unsigned)address*2,words*2);
    } else {
        assert(mode==flashWRITE_FLAG);++flash_writes;last_flash_event=++event_serial;
        if(bytes)memcpy(vp+(unsigned)scratch*2,bytes,words*2);
        memcpy(flash+(unsigned)address*2,vp+(unsigned)scratch*2,words*2);
    }
}
#define FlashToDgusWithData(b,a,v,p,n) T5lNorFlashRW(flashREAD_FLAG,b,a,v,p,n)
#define DgusToFlashWithData(b,a,v,p,n) T5lNorFlashRW(flashWRITE_FLAG,b,a,v,p,n)
void UartSendData(UART_TYPE *uart,uint8_t *bytes,uint16_t length) {
    assert(uart==&Uart_R11 && length<=sizeof(tx));
    ++tx_calls;tx_len=length;memcpy(tx,bytes,length);last_tx_event=++event_serial;
}
void I2cWriteSingleByte(uint8_t address,uint8_t value) {chip[address]=value;}
uint8_t I2cReadSingleByte(uint8_t address) {return chip[address];}
void I2cWriteMultipleBytes(uint8_t address,uint8_t *bytes,uint8_t count) {
    if(address==0x10)++rtc_writes;
    while(count--)chip[address++]=*bytes++;
}
void I2cReadMultipleBytes(uint8_t address,uint8_t *bytes,uint8_t count) {
    while(count--)*bytes++=chip[address++];
}
'''


TESTS = r'''
static void put_word(uint16_t address,uint16_t value) {
    vp[(unsigned)address*2]=(uint8_t)(value>>8);
    vp[(unsigned)address*2+1]=(uint8_t)value;
}
static uint16_t get_word(uint16_t address) {
    return (uint16_t)((uint16_t)vp[(unsigned)address*2]<<8|vp[(unsigned)address*2+1]);
}
static void expect_text(const char *text) {
    assert(!memcmp(vp+(unsigned)TIMEZONE_TEXT_ADDR*2,text,9));
}
static void expect_tx(uint8_t sign,uint8_t hours) {
    uint8_t expected[]={0xaa,0x55,0,3,0xf4,sign,hours};
    assert(tx_len==sizeof(expected) && !memcmp(tx,expected,sizeof(expected)));
}
static void set_input(uint16_t sign,uint16_t hours) {
    put_word(TIMEZONE_SIGN_ADDR,sign);put_word(TIMEZONE_HOUR_ADDR,hours);
}
static void command(uint16_t value) {
    put_word(TIMEZONE_COMMAND_ADDR,value);R11TimezoneTask();
    assert(get_word(TIMEZONE_COMMAND_ADDR)==0);
}
static void factory_reset(void) {
    memset(vp,0,sizeof(vp));memset(flash,0xff,sizeof(flash));memset(chip,0,sizeof(chip));
    flash_reads=flash_writes=tx_calls=tx_len=rtc_writes=0;
    event_serial=last_flash_event=last_tx_event=0;R11TimezoneInit();
}
static void test_defaults_and_sync(void) {
    unsigned i;
    factory_reset();
    assert(flash_reads==1 && flash_writes==0 && tx_calls==0);
    assert(get_word(TIMEZONE_SIGN_ADDR)==0 && get_word(TIMEZONE_HOUR_ADDR)==8);
    assert(get_word(TIMEZONE_STATUS_ADDR)==0);expect_text("UTC+08:00");
    R11TimezoneTask();assert(tx_calls==1 && flash_writes==0);expect_tx(0,8);
    for(i=0;i<100;i++)R11TimezoneTask();
    assert(tx_calls==1 && flash_writes==0);
    R11TimezoneSync();assert(tx_calls==2 && flash_writes==0);expect_tx(0,8);
    factory_reset();R11TimezoneSync();assert(tx_calls==1);
    for(i=0;i<100;i++)R11TimezoneTask();
    assert(tx_calls==1 && flash_writes==0);
    puts("PASS silent initialization then one startup sync; explicit sync cancels startup duplication; exact seven-byte frame");
}
static void test_save_and_restore(void) {
    uint8_t record[8];unsigned writes,sends;
    factory_reset();set_input(1,8);command(1);
    assert(flash_writes==1 && tx_calls==1 && last_flash_event<last_tx_event);
    assert(get_word(TIMEZONE_STATUS_ADDR)==1);expect_text("UTC-08:00");expect_tx(1,8);
    memcpy(record,flash+(unsigned)TIMEZONE_STORAGE_ADDR*2,8);
    writes=flash_writes;sends=tx_calls;
    command(1);assert(flash_writes==writes && tx_calls==sends+1);expect_tx(1,8);
    assert(!memcmp(record,flash+(unsigned)TIMEZONE_STORAGE_ADDR*2,8));
    memset(vp,0x77,sizeof(vp));R11TimezoneInit();
    assert(flash_writes==writes && tx_calls==sends+1);
    assert(get_word(TIMEZONE_SIGN_ADDR)==1 && get_word(TIMEZONE_HOUR_ADDR)==8);
    expect_text("UTC-08:00");
    R11TimezoneSync();expect_tx(1,8);sends=tx_calls;
    set_input(0,14);command(2);
    assert(get_word(TIMEZONE_SIGN_ADDR)==1 && get_word(TIMEZONE_HOUR_ADDR)==8);
    assert(flash_writes==writes && tx_calls==sends);expect_text("UTC-08:00");
    puts("PASS save precedes UART, repeated save avoids wear, power cycle restores offset, cancel reloads saved value");
}
static void test_bounds(void) {
    static const uint16_t invalid[][2]={{2,8},{256,8},{0,15},{1,13},{0,0xffff},{0xffff,8}};
    unsigned i,writes,sends;uint8_t record[8];
    factory_reset();set_input(0,14);command(1);expect_tx(0,14);expect_text("UTC+14:00");
    set_input(1,12);command(1);expect_tx(1,12);expect_text("UTC-12:00");
    writes=flash_writes;sends=tx_calls;memcpy(record,flash+(unsigned)TIMEZONE_STORAGE_ADDR*2,8);
    for(i=0;i<sizeof(invalid)/sizeof(invalid[0]);i++) {
        set_input(invalid[i][0],invalid[i][1]);command(1);
        assert(get_word(TIMEZONE_STATUS_ADDR)==3 && flash_writes==writes && tx_calls==sends);
        assert(!memcmp(record,flash+(unsigned)TIMEZONE_STORAGE_ADDR*2,8));
    }
    set_input(1,0);command(1);expect_tx(0,0);expect_text("UTC+00:00");
    puts("PASS east +14/west -12 bounds; invalid words cannot truncate into a valid offset; zero is canonicalized");
}
static void valid_reply(uint8_t *reply,uint8_t length) {
    uint8_t frame[]={0xaa,0x55,0,length,0xf4,1,8,26,11,20,23,59,58};
    memcpy(reply,frame,sizeof(frame));
}
static void expect_rtc(void) {
    const uint8_t expected_chip[]={0x58,0x59,0x23,0x20,0x20,0x11,0x26};
    const uint8_t expected_vp[]={26,11,20,5,23,59,58,0};
    assert(!memcmp(chip+0x10,expected_chip,7));
    assert(!memcmp(vp+0x20,expected_vp,8));
}
static void test_reply(void) {
    uint8_t reply[14];unsigned writes;
    factory_reset();set_input(1,8);command(1);writes=flash_writes;
    valid_reply(reply,4);UartR11UserTimezoneProtocol(&Uart_R11,reply,13);
    assert(rtc_writes==1 && flash_writes==writes && get_word(TIMEZONE_STATUS_ADDR)==2);expect_rtc();
    valid_reply(reply,9);UartR11UserTimezoneProtocol(&Uart_R11,reply,13);
    assert(rtc_writes==2 && flash_writes==writes);expect_rtc();
    reply[7]=28;reply[8]=2;reply[9]=29;reply[10]=reply[11]=reply[12]=0;
    UartR11UserTimezoneProtocol(&Uart_R11,reply,13);
    assert(rtc_writes==3 && chip[0x13]==4 && chip[0x14]==0x29);
    assert(vp[0x20]==28 && vp[0x21]==2 && vp[0x22]==29 && vp[0x23]==2);
    puts("PASS literal 0004 and standard 0009 replies update real RTC BCD, weekday and DGUS; leap day accepted");
}
static void test_reject_bad_replies(void) {
    static const uint8_t bad[][2]={{0,0},{1,0},{2,1},{3,3},{4,0xf3},{5,2},{6,9},
                                  {7,100},{8,0},{8,13},{9,0},{9,31},{10,24},{11,60},{12,60}};
    uint8_t reply[14],before[256];unsigned i;
    factory_reset();set_input(1,8);command(1);memcpy(before,chip,sizeof(chip));
    for(i=0;i<13;i++) {
        valid_reply(reply,4);UartR11UserTimezoneProtocol(&Uart_R11,reply,(uint16_t)i);
        assert(rtc_writes==0 && !memcmp(chip,before,sizeof(chip)));
    }
    valid_reply(reply,4);UartR11UserTimezoneProtocol(&other_uart,reply,13);assert(rtc_writes==0);
    for(i=0;i<sizeof(bad)/sizeof(bad[0]);i++) {
        valid_reply(reply,4);reply[bad[i][0]]=bad[i][1];
        UartR11UserTimezoneProtocol(&Uart_R11,reply,13);assert(rtc_writes==0);
    }
    valid_reply(reply,4);reply[7]=26;reply[8]=2;reply[9]=29;
    UartR11UserTimezoneProtocol(&Uart_R11,reply,13);assert(rtc_writes==0);
    valid_reply(reply,4);reply[5]=0;
    UartR11UserTimezoneProtocol(&Uart_R11,reply,13);assert(rtc_writes==0);
    assert(flash_writes==1 && !memcmp(chip,before,sizeof(chip)));
    puts("PASS short frames, wrong UART/header/command/offset, impossible dates and out-of-range times leave RTC untouched");
}
static void test_reply_while_editing(void) {
    uint8_t reply[13];unsigned writes,sends;
    factory_reset();set_input(1,8);command(1);writes=flash_writes;sends=tx_calls;
    set_input(0,14);R11TimezoneTask();expect_text("UTC+14:00");
    valid_reply(reply,4);UartR11UserTimezoneProtocol(&Uart_R11,reply,13);
    expect_rtc();expect_text("UTC+14:00");
    assert(get_word(TIMEZONE_SIGN_ADDR)==0 && get_word(TIMEZONE_HOUR_ADDR)==14);
    assert(get_word(TIMEZONE_STATUS_ADDR)==0 && flash_writes==writes && tx_calls==sends);
    set_input(1,13);command(1);assert(get_word(TIMEZONE_STATUS_ADDR)==3);
    UartR11UserTimezoneProtocol(&Uart_R11,reply,13);
    assert(get_word(TIMEZONE_STATUS_ADDR)==3 && flash_writes==writes && tx_calls==sends);
    set_input(0,5);command(1);writes=rtc_writes;
    UartR11UserTimezoneProtocol(&Uart_R11,reply,13);
    assert(rtc_writes==writes && get_word(TIMEZONE_STATUS_ADDR)==1);expect_text("UTC+05:00");
    puts("PASS an asynchronous reply preserves unsaved edits/errors, and a superseded offset cannot reset the clock");
}
static void test_commands_and_zero(void) {
    static const uint16_t ignored[]={9,0x0101,0xff01,0x0102,0x0103,0xffff};
    unsigned i,writes,sends;uint8_t reply[13];
    factory_reset();set_input(0,0);command(1);expect_tx(0,0);
    valid_reply(reply,4);reply[5]=1;reply[6]=0;
    UartR11UserTimezoneProtocol(&Uart_R11,reply,13);
    assert(rtc_writes==1 && get_word(TIMEZONE_STATUS_ADDR)==2);expect_text("UTC+00:00");
    writes=flash_writes;sends=tx_calls;set_input(1,8);
    for(i=0;i<sizeof(ignored)/sizeof(ignored[0]);i++) {
        command(ignored[i]);assert(flash_writes==writes && tx_calls==sends);
    }
    R11TimezoneSync();expect_tx(0,0);
    puts("PASS zero-hour west reply is UTC+00:00; unknown and high-byte commands cannot save or send");
}
static void test_page_enter_sync(void) {
    unsigned i,sends,writes;
    factory_reset();set_input(1,12);command(3);
    assert(tx_calls==1 && flash_writes==0);expect_tx(0,8);expect_text("UTC+08:00");
    assert(get_word(TIMEZONE_SIGN_ADDR)==0 && get_word(TIMEZONE_HOUR_ADDR)==8);
    assert(get_word(TIMEZONE_STATUS_ADDR)==1);
    for(i=0;i<100;i++)R11TimezoneTask();
    assert(tx_calls==1 && flash_writes==0);
    set_input(1,8);command(1);writes=flash_writes;sends=tx_calls;
    set_input(0,14);R11TimezoneTask();expect_text("UTC+14:00");command(3);
    assert(tx_calls==sends+1 && flash_writes==writes);expect_tx(1,8);expect_text("UTC-08:00");
    assert(get_word(TIMEZONE_SIGN_ADDR)==1 && get_word(TIMEZONE_HOUR_ADDR)==8);
    for(i=0;i<100;i++)R11TimezoneTask();
    assert(tx_calls==sends+1 && flash_writes==writes);
    sends=tx_calls;R11TimezoneInit();set_input(2,99);command(3);
    assert(tx_calls==sends+1 && flash_writes==writes);expect_tx(1,8);expect_text("UTC-08:00");
    assert(get_word(TIMEZONE_SIGN_ADDR)==1 && get_word(TIMEZONE_HOUR_ADDR)==8);
    assert(get_word(TIMEZONE_STATUS_ADDR)==1);
    for(i=0;i<100;i++)R11TimezoneTask();
    assert(tx_calls==sends+1 && flash_writes==writes);
    puts("PASS page-enter command restores and sends the saved timezone once, including the first startup task");
}
static void test_corrupt_flash(void) {
    uint8_t record[8];unsigned i;
    factory_reset();set_input(1,8);command(1);
    memcpy(record,flash+(unsigned)TIMEZONE_STORAGE_ADDR*2,8);
    for(i=0;i<8;i++) {
        memcpy(flash+(unsigned)TIMEZONE_STORAGE_ADDR*2,record,8);
        flash[(unsigned)TIMEZONE_STORAGE_ADDR*2+i]^=0x80;
        R11TimezoneInit();expect_text("UTC+08:00");
        assert(get_word(TIMEZONE_SIGN_ADDR)==0 && get_word(TIMEZONE_HOUR_ADDR)==8);
    }
    assert(flash_writes==1);
    puts("PASS corrupt persistent records fall back safely without rewriting Flash on boot");
}
int main(void) {
    test_defaults_and_sync();test_save_and_restore();test_bounds();test_reply();
    test_reject_bad_replies();test_reply_while_editing();test_commands_and_zero();test_page_enter_sync();
    test_corrupt_flash();return 0;
}
'''


class TimezoneTest(unittest.TestCase):
    def test_production_timezone_and_rtc(self):
        header = (ROOT / 'modules/r11_timezone.h').read_text(encoding='utf-8-sig')
        source = (ROOT / 'modules/r11_timezone.c').read_text(encoding='utf-8-sig')
        rtc = (ROOT / 'modules/rtc.c').read_text(encoding='utf-8-sig')
        combined = (HARDWARE + without_local_includes(header)
                    + without_local_includes(rtc)
                    + without_local_includes(source) + TESTS)
        BUILD.mkdir(exist_ok=True)
        cfile, executable = BUILD / 'timezone.c', BUILD / 'timezone.exe'
        cfile.write_text(combined, encoding='utf-8')
        compiler = os.environ.get('CLANG', str(ROOT.parent / 'tools/llvm-mingw-20250709-ucrt-x86_64/bin/clang.exe'))
        subprocess.run([compiler, '-std=c99', '-Wall', '-Wextra', '-Werror',
                        str(cfile), '-o', str(executable)], check=True)
        subprocess.run([str(executable)], check=True)


if __name__ == '__main__':
    unittest.main()

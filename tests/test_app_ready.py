"""Production ready frame, UART5 transmission, and main-entry regression.

Only SFR/hardware and unrelated scheduled work are substituted. Generated
files remain in ignored tests/.build; --baseline-uart checks HEAD's old driver.
"""
from pathlib import Path
import argparse
import re
import subprocess

ROOT = Path(__file__).resolve().parents[1]
BUILD = ROOT / 'tests/.build'
BUILD.mkdir(exist_ok=True)
parser = argparse.ArgumentParser()
parser.add_argument('--baseline-uart', action='store_true')
args = parser.parse_args()


def read(path):
    return (ROOT / path).read_text(encoding='utf-8-sig')


def no_includes(text):
    return re.sub(r'^\s*#include[^\n]*$', '', text, flags=re.M)


def function(text, name):
    match = re.search(r'^(?:static\s+)?(?:void|uint16_t)\s+' + name + r'\s*\([^;]*?\)\s*\{', text, re.M)
    assert match, f'Cannot find production function {name}'
    start = match.start()
    pos = text.index('{', start)
    depth = 1
    for end in range(pos + 1, len(text)):
        depth += (text[end] == '{') - (text[end] == '}')
        if not depth:
            return text[start:end + 1] + '\n'
    raise AssertionError(f'Unclosed function {name}')


config = no_includes(read('include/T5L/T5LOSConfig.h'))
config = re.sub(r'^typedef[^\n]*$', '', config, flags=re.M)
config = re.sub(r'^\s*sbit[^\n]*$', '', config, flags=re.M)
config = re.sub(r'^#define NULL[^\n]*$', '', config, flags=re.M)
uart = (subprocess.check_output(['git', 'show', 'HEAD:source/uart.c'], cwd=ROOT).decode('utf-8')
        if args.baseline_uart else read('source/uart.c'))
uart = no_includes(uart[:uart.index('uint8_t prvDwin8283CrcCheck')])
uart = re.sub(r'\binterrupt\s+\d+', '', uart)
uart = re.sub(r'\bSBUF3_TX\s*=\s*([^;]+);', r'host_uart5_write(\1);', uart)
uart = uart.replace('void UartSendData(', 'static void production_uart_send(')

prefix = r'''
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <setjmp.h>
#define xdata
#define code
static uint8_t MUX_SEL,P0MDOUT,P0,ADCON,SCON0,PCON,SREL0H,SREL0L,ES0,EA;
static uint8_t RI0,TI0,SBUF0,SCON2T,SCON2R,BODE2_DIV_H,BODE2_DIV_L,ES2T,ES2R;
static uint8_t SBUF2_RX,SBUF2_TX,SCON3T,SCON3R,BODE3_DIV_H,BODE3_DIV_L,ES3T,ES3R;
static uint8_t SBUF3_RX,SBUF3_TX,EX0,EX1,data_write_f;
static uint8_t wire[2048], inflight;
static unsigned wire_len, notify_calls, crc_calls, send_calls, scheduler_steps;
static unsigned config_done,cpu_done,rtc_done,ota_done,task_mask,boot_mode;
static unsigned timezone_done,timezone_tasks;
static jmp_buf scheduler_exit;
static void host_uart5_write(uint8_t value) {
    assert((SCON3T&0x80U) && !inflight);
    assert(wire_len<sizeof(wire));
    if(boot_mode && wire_len==0) {
        assert(config_done && cpu_done && rtc_done && ota_done && timezone_done);
        assert(task_mask==((1U<<0)|(1U<<2)|(1U<<3)|(1U<<4)));
        assert(scheduler_steps==0 && notify_calls==1);
    }
    SBUF3_TX=value;inflight=1;wire[wire_len++]=value;
}
'''

driver_tests = r'''
static const uint8_t expected_ready[7]={0xAA,0x55,0x00,0x03,0xF3,0x6D,0x61};
void UartSendData(UART_TYPE *uart,uint8_t *bytes,uint16_t len) {
    assert(uart==&Uart5);++send_calls;production_uart_send(uart,bytes,len);
}
static void complete_one(void) {
    /* Only an actual SBUF write creates a hardware completion. */
    assert(inflight && ES3T && (SCON3T&0x80U));
    inflight=0;SCON3T|=1U;Uart5TxIsr();
}
static void drain(void) {
    unsigned limit=0;
    while(inflight){complete_one();assert(++limit<sizeof(wire));}
    assert(!Uart5.TxBusy && Uart5.TxHead==Uart5.TxTail && !(SCON3T&1U));
    assert(Uart2.TxHead==0 && !Uart2.TxBusy && Uart4.TxHead==0 && !Uart4.TxBusy);
    assert(SBUF0==0 && SBUF2_TX==0);
}
static void reset_driver(void) {
    wire_len=0;inflight=0;notify_calls=crc_calls=send_calls=0;boot_mode=0;
    SBUF0=SBUF2_TX=0;sys_2k_ratio=1;sysFOSC=sysFCLK=383385600UL;UartInit();
    assert(ES3T && ES3R && EA && SCON3T==0x80);
}
static void test_direct_kickoff(void) {
    uint8_t value=0xAA;
    reset_driver();UartSendData(&Uart5,&value,1);
    if(wire_len!=1 || !inflight || SBUF3_TX!=0xAA) {
        fprintf(stderr,"FAIL cold UART5 send never wrote SBUF3_TX: wire_len=%u TxBusy=%u SCON3T=%02X\n",
                wire_len,Uart5.TxBusy,SCON3T);
        exit(1);
    }
    drain();puts("PASS UART5 cold send directly writes first byte; hardware completion drains idle");
}
'''

main_harness = r'''
static uint16_t production_crc16(uint8_t *pBuf,uint16_t buf_len);
uint16_t crc_16(uint8_t *buf,uint16_t len) {
    assert(len==5 && !memcmp(buf,expected_ready,5));++crc_calls;
    return production_crc16(buf,len);
}
static void production_notify(void);
void R11AdvertiseNotifyReady(void) {++notify_calls;production_notify();}
static void (*tasks[sysMAX_TASK_NUM])(void);
#define RTC_INTERVAL 500
#define R11_TASK_INTERVAL 100
#define OTA_TASK_INTERVAL 100
static struct {uint16_t restart_flag;} r11_state={UINT16_PORT_MAX};
static unsigned video_init_process;
#define VIDEO_PROCESS_UNINIT 0
static void R11RestartInit(void) {assert(!"unexpected V851 handshake");}
static void R11VideoPlayerProcess(void) {assert(!"unexpected V851 handshake");}
static void R11ValueScanTask(void) {assert(!"unexpected V851 handshake");}
void R11ConfigInitFormLib(void) {assert(!config_done && !wire_len);config_done=1;}
void T5LCpuInit(void) {assert(config_done && !wire_len);UartInit();cpu_done=1;}
void RtcInit(void) {assert(cpu_done && !wire_len);rtc_done=1;}
void R11TimezoneInit(void) {assert(rtc_done && !wire_len && !timezone_done);timezone_done=1;}
void R11TimezoneTask(void) {assert(timezone_done);++timezone_tasks;}
void OTAInit(void) {assert(cpu_done && !wire_len);ota_done=1;}
void RtcTask(void) {}
void UartProtocalHandleTask(void) {}
void OTATask(void) {}
void EX1_Start(void) {assert(0);}
void R11AdvertiseTask(void);
void SysTaskAdd(uint8_t id,uint16_t interval,void (*task)(void)) {
    assert(id<sysMAX_TASK_NUM && interval && !wire_len && !tasks[id]);
    tasks[id]=task;task_mask|=1U<<id;
}
void SysTaskRun(void) {
    unsigned i;
    ++scheduler_steps;
    assert(notify_calls==1 && crc_calls==1 && send_calls==1);
    assert(Uart5.RxHead==0 && Uart5.RxTail==0 && r11_state.restart_flag==UINT16_PORT_MAX);
    for(i=0;i<sysMAX_TASK_NUM;++i)if(tasks[i])tasks[i]();
    if(inflight)complete_one();
    if(scheduler_steps==100)longjmp(scheduler_exit,1);
}
'''

full_tests = r'''
static void assert_frame(unsigned offset) {
    assert(wire_len==offset+7 && !memcmp(wire+offset,expected_ready,7));
}
static void test_ready(void) {
    uint8_t lead[3]={0x10,0x20,0x30},tail[2]={0x40,0x50};
    unsigned first;
    reset_driver();R11AdvertiseNotifyReady();
    assert(wire_len==1 && inflight && SBUF3_TX==0xAA && Uart5.TxBusy);
    assert(crc_calls==1 && send_calls==1);drain();assert_frame(0);
    first=wire_len;R11AdvertiseNotifyReady();assert(wire_len==first+1);drain();assert_frame(first);
    puts("PASS exact AA550003F36D61 frame, production CRC over first five bytes, idle restart, UART5 only");
    reset_driver();Uart5.TxHead=Uart5.TxTail=uartUART5_TXBUF_SIZE-3;
    R11AdvertiseNotifyReady();drain();assert_frame(0);assert(Uart5.TxHead==4 && Uart5.TxTail==4);
    puts("PASS ready frame wraps UART5 transmit ring");
    reset_driver();UartSendData(&Uart5,lead,3);assert(wire_len==1 && inflight);
    R11AdvertiseNotifyReady();assert(wire_len==1 && inflight);
    UartSendData(&Uart5,tail,2);assert(wire_len==1 && inflight);drain();
    assert(wire_len==12 && !memcmp(wire,lead,3) && !memcmp(wire+3,expected_ready,7) && !memcmp(wire+10,tail,2));
    puts("PASS appending ready frame and other data preserves in-flight byte and queued order");
}
static void test_main_entry(void) {
    reset_driver();boot_mode=1;scheduler_steps=0;config_done=cpu_done=rtc_done=ota_done=task_mask=0;
    timezone_done=timezone_tasks=0;
    memset(tasks,0,sizeof(tasks));data_write_f=0;r11_state.restart_flag=UINT16_PORT_MAX;
    if(!setjmp(scheduler_exit))app_main();
    assert(scheduler_steps==100 && notify_calls==1 && crc_calls==1 && send_calls==1);
    assert(timezone_done==1 && timezone_tasks==100);
    assert(!inflight && !Uart5.TxBusy);assert_frame(0);drain();
    puts("PASS actual main initializes timezone before F3; 100 timezone task iterations run without V851 handshake");
}
int main(void) {test_direct_kickoff();test_ready();test_main_entry();return 0;}
'''

combined = '#include <stdlib.h>\n' + prefix + config + no_includes(read('source/uart.h')) + uart + driver_tests
if args.baseline_uart:
    combined += 'int main(void){test_direct_kickoff();return 0;}\n'
else:
    advertise = read('modules/r11_advertise.c')
    assert re.search(r'void\s+R11AdvertiseNotifyReady\s*\(void\)\s*;', read('modules/r11_advertise.h'))
    notify = function(advertise, 'R11AdvertiseNotifyReady').replace('void R11AdvertiseNotifyReady(', 'static void production_notify(')
    crc = function(read('include/sys.c'), 'crc_16').replace('uint16_t crc_16', 'static uint16_t production_crc16')
    combined += main_harness + crc + notify + function(advertise, 'R11AdvertiseTask')
    combined += no_includes(read('user/main.c')).replace('void main(void)', 'void app_main(void)') + full_tests

name = 'app_ready_uart_baseline' if args.baseline_uart else 'app_ready'
path = BUILD / (name + '.c')
path.write_text(combined, encoding='utf-8')
exe = BUILD / (name + '.exe')
compiler = ROOT.parent / 'tools/llvm-mingw-20250709-ucrt-x86_64/bin/clang.exe'
subprocess.run([str(compiler), '-std=c99', '-Wall', '-Wextra', '-Werror', '-Wno-unused-function',
                '-Wno-unused-variable', str(path), '-o', str(exe)], check=True)
subprocess.run([str(exe)], check=True)

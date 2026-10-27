"""Run production UART parsers and manual video controls with simulated IO.

Requires CLANG, LLVM_LINK and LLI (or the LLVM tools on PATH).
"""
import os
import re
from pathlib import Path
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]


def function(source, name):
    start = source.index(name + '(')
    start = source.rfind('\n', 0, start) + 1
    body = source.index('{', start)
    depth = 1
    end = body + 1
    while depth:
        depth += (source[end] == '{') - (source[end] == '}')
        end += 1
    return source[start:end]


class UartRoutingTest(unittest.TestCase):
    def test_split_dgus_and_independent_modbus(self):
        source = (ROOT / 'source/uart.c').read_text(encoding='utf-8')
        harness = r'''
#include <stdint.h>
#define xdata
#define uartUART2_ENABLED 1
#define uartUART4_ENABLED 1
#define uartUART2_RXBUF_SIZE 256
#define uartUART4_RXBUF_SIZE 256
#define uartUART_COMMON_FRAME_SIZE 512
#define UART_NON_REC 0
#define UART_RECING 1
#define sysDGUS_SYSTEM_CONFIG 0
typedef struct { uint16_t RxHead, RxTail, RxTimeout; uint8_t RxFlag; } UART_TYPE;
static UART_TYPE Uart2, Uart4;
static uint8_t Uart2RxBuffer[256], Uart4RxBuffer[256];
static uint8_t uart2_partial[258], uart4_partial[258];
static uint16_t uart2_partial_size, uart4_partial_size;
static uint32_t uart2_last_rx, uart4_last_rx, tick;
static uint8_t ET0=1, vp[2], reply[256];
static uint16_t replies, modbus;
static UART_TYPE *reply_uart;
uint32_t GetSysTick(void) { return tick; }
uint16_t crc_16(uint8_t *p, uint16_t n) {
    uint16_t c=0xffff, i;
    while(n--) { c ^= *p++; for(i=0;i<8;i++) c=(c&1)?(c>>1)^0xa001:c>>1; }
    return c;
}
void read_dgus_vp(uint32_t a,uint8_t *b,uint8_t n) {
    (void)n; b[0]=a?vp[0]:0; b[1]=a?vp[1]:0;
}
void write_dgus_vp(uint32_t a,uint8_t *b,uint16_t n) {
    (void)a; (void)n; vp[0]=b[0]; vp[1]=b[1];
}
void UartSendData(UART_TYPE *u,uint8_t *b,uint16_t n) {
    uint16_t i; reply_uart=u; replies++; for(i=0;i<n;i++) reply[i]=b[i];
}
void FwProtocolFeed(uint8_t *b,uint16_t n) { (void)b; (void)n; modbus++; }
void FwUsbProtocol(UART_TYPE *u,uint8_t *b,uint16_t n) { (void)u;(void)b;(void)n; }
uint8_t AppOtaControl(UART_TYPE *u,uint8_t *b,uint16_t n) { (void)u;(void)b;(void)n;return 0; }
'''
        for name in ['prvDwin8283CrcCheck', 'UartStandardDwin8283Protocal',
                     'Uart2ProtocolFeed', 'Uart4ProtocolFeed', 'UartReadFrame']:
            harness += '\n' + function(source, name)
        harness += r'''
static void receive(UART_TYPE *u,uint8_t *b,uint16_t n) {
    uint8_t *ring=(u==&Uart2)?Uart2RxBuffer:Uart4RxBuffer;
    while(n--) { ring[u->RxHead++]=*b++; u->RxHead%=256; }
    u->RxFlag=UART_RECING; u->RxTimeout=0; tick+=10; UartReadFrame(u);
}
#define CHECK(c) do { if(!(c)) return __LINE__; } while(0)
int main(void) {
    uint8_t w[]={0x5a,0xa5,5,0x82,0x10,0,0x12,0x34};
    uint8_t r[]={0x5a,0xa5,4,0x83,0x10,0,1};
    uint8_t m[]={1,3,2,0,7,0,0};
    uint16_t c=crc_16(m,5), split, i;
    m[5]=(uint8_t)c; m[6]=(uint8_t)(c>>8);
    for(split=1;split<sizeof(w);split++) {
        vp[0]=vp[1]=0;
        receive(&Uart2,w,split); CHECK(vp[0]==0);
        receive(&Uart4,m,3);
        receive(&Uart2,w+split,sizeof(w)-split);
        CHECK(vp[0]==0x12 && vp[1]==0x34);
        receive(&Uart4,m+3,4); CHECK(modbus==split);
    }
    for(split=1;split<sizeof(r);split++) {
        replies=0; receive(&Uart2,r,split); CHECK(replies==0);
        receive(&Uart2,r+split,sizeof(r)-split);
        CHECK(replies==1 && reply_uart==&Uart2);
        CHECK(reply[3]==0x83 && reply[7]==0x12 && reply[8]==0x34);
    }
    replies=0;
    for(i=0;i<sizeof(r);i++) receive(&Uart2,r+i,1);
    CHECK(replies==1 && reply_uart==&Uart2);
    receive(&Uart2,m,sizeof(m)); CHECK(modbus==7);
    receive(&Uart2,w,4); tick+=501; receive(&Uart2,r,sizeof(r));
    CHECK(replies==2 && reply_uart==&Uart2);
    CHECK(ET0==1);
    return 0;
}
'''
        compiler = os.environ.get('CLANG', 'clang')
        suffix = '.exe' if os.name == 'nt' else ''
        sibling = lambda name: str(Path(compiler).with_name(name + suffix))
        with tempfile.TemporaryDirectory(prefix='uart-routing-') as directory:
            path = Path(directory)
            (path / 'test.c').write_text(harness, encoding='utf-8')
            subprocess.run([compiler, '-std=c99', '-Wall', '-Wextra', '-Werror',
                            '-ffreestanding', '-fno-builtin', '-emit-llvm', '-c',
                            str(path / 'test.c'), '-o', str(path / 'test.bc')], check=True)
            subprocess.run([os.environ.get('LLI', sibling('lli')),
                            '--force-interpreter=true', str(path / 'test.bc')], check=True)


class ManualVideoTest(unittest.TestCase):
    def test_no_idle_playback_and_manual_controls(self):
        source = (ROOT / 'modules/r11_common.c').read_text(encoding='utf-8')
        header = (ROOT / 'modules/r11_common.h').read_text(encoding='utf-8')
        defines = '\n'.join(line for line in header.splitlines()
                            if re.match(r'#define (keyMP4_|cmdMP4_|VIDEO_UI_STATUS_ADDR)', line))
        harness = '#include <stdint.h>\n' + defines + r'''
static uint16_t page=42, video_full_origin=0xffff, video_ui_pending;
static uint8_t video_touch_down,video_wake_guard,video_ui_enter,video_next_due,video_ui_known,video_ui_query;
static uint32_t tick,video_wake_at;
static struct { uint8_t state; uint16_t now_play_serial; } r11_player;
static uint16_t count=2, last_command, last_file;
static unsigned sends,switches;
static uint32_t VideoTime(void) {return tick;}
static uint16_t ReadPageId(void) {return page;}
static void SwitchPageById(uint16_t p) {page=p;++switches;}
static void VideoVp(uint16_t a,uint16_t v) {(void)a;(void)v;}
static uint16_t VideoCount(void) {return count;}
static void VideoSend(uint8_t c,uint16_t v) {(void)v;last_command=c;++sends;}
static void VideoPlayFileKey(uint16_t k) {last_file=k;++sends;}
'''
        for name in ('R11VideoIdleActive', 'R11VideoIdleReturnPage', 'R11VideoWakeBlocked',
                     'VideoIdleDismiss', 'R11VideoTouchTask', 'VideoFullFailure', 'VideoUiKey'):
            harness += '\n' + function(source, name) + '\n'
        harness += r'''
#define CHECK(c) do {if(!(c))return __LINE__;}while(0)
int main(void) {
 uint16_t pages[]={1,2,27,42,9,120,121},i;
 for(i=0;i<7;i++) {
  page=pages[i];tick+=30000;R11VideoTouchTask(0);
  tick+=60000;R11VideoTouchTask(0);
  CHECK(page==pages[i] && sends==0 && switches==0 && !R11VideoIdleActive());
 }
 CHECK(R11VideoIdleReturnPage()==42);
 page=120;VideoUiKey(keyMP4_IMG_SET_BIG);CHECK(page==121 && video_full_origin==120);
 R11VideoTouchTask(0x5a01);R11VideoTouchTask(0x5a02);CHECK(page==121);
 VideoUiKey(keyMP4_IMG_SET_SMALL);CHECK(page==120 && sends==0);
 r11_player.state=1;VideoUiKey(keyMP4_TOGGLE_PAUSE);CHECK(last_command==cmdMP4_PAUSE);
 r11_player.state=4;VideoUiKey(keyMP4_TOGGLE_PAUSE);CHECK(last_command==cmdMP4_REPLAY);
 video_ui_known=1;r11_player.now_play_serial=0;VideoUiKey(keyMP4_NEXT_VIDEO);CHECK(last_file==keyMP4_1FILE+1);
 VideoUiKey(keyMP4_1FILE);CHECK(last_file==keyMP4_1FILE);
 count=0;VideoUiKey(keyMP4_IMG_SET_BIG);CHECK(page==120);
 page=121;video_full_origin=0xffff;VideoFullFailure();CHECK(page==42 && last_command==cmdMP4_PAUSE);
 CHECK(R11VideoWakeBlocked());R11VideoTouchTask(0x5a01);tick+=1000;R11VideoTouchTask(0x5a03);
 CHECK(R11VideoWakeBlocked());R11VideoTouchTask(0x5a02);tick+=200;R11VideoTouchTask(0);
 CHECK(!R11VideoWakeBlocked());
 return 0;
}
'''
        compiler = os.environ.get('CLANG', 'clang')
        sibling = str(Path(compiler).with_name('lli.exe' if os.name == 'nt' else 'lli'))
        with tempfile.TemporaryDirectory(prefix='manual-video-') as directory:
            path = Path(directory)
            (path / 'test.c').write_text(harness, encoding='utf-8')
            subprocess.run([compiler, '-std=c99', '-Wall', '-Wextra', '-Werror',
                            '-ffreestanding', '-fno-builtin', '-emit-llvm', '-c',
                            str(path / 'test.c'), '-o', str(path / 'test.bc')], check=True)
            subprocess.run([os.environ.get('LLI', sibling), '--force-interpreter=true',
                            str(path / 'test.bc')], check=True)


if __name__ == '__main__':
    unittest.main()

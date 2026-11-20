"""Run the production idle timer and initialization state machine on a host."""
from pathlib import Path
import os
import re
import subprocess
import unittest
from test_advertise_protocol import function

ROOT = Path(__file__).resolve().parents[1]
BUILD = ROOT / 'tests/.build'

PRELUDE = r'''
#include <stdint.h>
#include <string.h>
#include <assert.h>
#include <stdio.h>
#define code
#define data
#define __NOP() ((void)0)
#define sysADVERTISE_MODE_ENABLED 1
#define sysBEAUTY_MODE_ENABLED 0
#define sysN5CAMERA_MODE_ENABLED 0
#define R11_WIFI_ENABLED 0
#define sysDGUS_SYSTEM_CONFIG 0x81
#define sysDGUS_PIC_NOW 0x14
#define sysDGUS_TP_STATUS 0x16
#define sysWAE_PLAY_ADDR 0xa0
#define flashMAIN_BLOCK_ORDER 0
typedef struct { unsigned id; } UART_TYPE;
'''

STUBS = r'''
PAGE_S page_st;
PLAYER_T r11_player;
MAINVIEW_S mainview;
SCREEN_S screen_opt;
R11_STATE r11_state;
VIDEO_INIT_PROCESS video_init_process;
uint8_t first_in_flag;
static uint16_t idle_touch_count, idle_return_page;
static uint16_t current_page, key, color_red=0xf800, size_width, size_height;
static uint8_t touch[4], rotation, Big_Small_Flag;
uint8_t mp4_name[5][MAX_MP3_NAME_LEN];
uint16_t mp4_name_len[5];
uint16_t pixels_arr_h2[5]={1920,1024,800,800,1024};
uint16_t pixels_arr_l2[5]={1080,600,600,480,768};
static unsigned commands[256], page_switches;
static uint16_t loop_mode;
void read_dgus_vp(uint32_t addr,uint8_t *buf,uint8_t len) {
    uint16_t saved[6]={37,0,0,0,0,0};
    memset(buf,0,len*2);
    if(addr==sysDGUS_PIC_NOW) memcpy(buf,&current_page,2);
    if(addr==R11_SCAN_ADDRESS) memcpy(buf,&key,2);
    if(addr==sysDGUS_TP_STATUS) memcpy(buf,touch,4);
    if(addr==sysDGUS_SYSTEM_CONFIG) buf[1]=rotation;
    if(addr==VOLUME_SET_ADDR) memcpy(buf,saved,len*2);
}
void write_dgus_vp(uint32_t addr,uint8_t *buf,uint16_t len) {
    (void)len;
    if(addr==R11_SCAN_ADDRESS) memcpy(&key,buf,2);
    if(addr==sysDGUS_TP_STATUS) memcpy(touch,buf,4);
}
void T5lSendUartDataToR11(uint8_t cmd,uint8_t *buf) {
    commands[cmd]++;
    if(cmd==cmdMP4_UPDATEFILE) {
        r11_player.page_mp4_nums=1;r11_player.total_serial=1;
        memcpy(mp4_name[0],"1.mp4",6);mp4_name_len[0]=5;
    }
    if(cmd==cmdMP4_LOOP_MODE_SET) memcpy(&loop_mode,buf,2);
}
void R11ChangePictureLocate(uint16_t x,uint16_t y,uint16_t w,uint16_t h,uint8_t full) {
    (void)x;(void)y;size_width=w;size_height=h;Big_Small_Flag=full;
}
void SwitchPageById(uint16_t page) {current_page=page;page_switches++;}
void FlashToDgus(uint16_t a,uint16_t b,uint16_t c,uint16_t d) {(void)a;(void)b;(void)c;(void)d;}
void T5lJpegInit(void) {}
void R11PageInitChange(void) {}
static void R11StartPowerInit(void) {}
void R11TimezoneSync(void) {}
void R11TimezoneTask(void) {}
void delay_ms(uint16_t ms) {(void)ms;}
static void R11ValueScanTask(void) {}
'''

CHECKS = r'''
static void ticks(unsigned n) {while(n--) R11AdvertiseTask();}
static void reset(void) {
    memset(&r11_player,0,sizeof(r11_player));memset(&page_st,0,sizeof(page_st));
    memset(commands,0,sizeof(commands));memset(touch,0,sizeof(touch));
    page_st.fullvideo_flag=page_st.video_flag=0x5a;
    page_st.fullvideo_page=19;page_st.video_page=18;
    key=0;rotation=0;screen_opt.screen_ratio=0;current_page=7;page_switches=0;
    r11_state.restart_flag=1;R11AdvertiseTask();
}
int main(void) {
    unsigned i, before;
    reset();ticks(599);
    assert(!first_in_flag && !commands[cmdMP4_PLAY] && !page_switches);
    ticks(1);assert(first_in_flag && idle_return_page==7);
    ticks(8);
    assert(commands[cmdMP4_PLAY]==1 && current_page==19 && loop_mode==0x0102);
    assert(video_init_process==VIDEO_PROCESS_COMPLETE && r11_player.r11_volunme==37);
    puts("PASS no early boot playback; 600 idle ticks start full-screen all-loop playback and retain volume");

    key=keyMP4_IDLE_RETURN;ticks(1);
    assert(current_page==7 && key==0 && commands[cmdMP4_PAUSE]==1);
    before=commands[cmdMP4_REPLAY];ticks(599);assert(commands[cmdMP4_REPLAY]==before);
    rotation=1;ticks(1);
    assert(commands[cmdMP4_REPLAY]==before+1 && current_page==19);
    assert(size_width==1080 && size_height==1920 && Big_Small_Flag==1);
    key=keyMP4_IDLE_RESTART;ticks(1);
    assert(!first_in_flag && current_page==7 && video_init_process==VIDEO_PROCESS_VOLUME);
    before=commands[cmdMP4_PLAY];ticks(599);assert(commands[cmdMP4_PLAY]==before);
    rotation=0;ticks(9);assert(commands[cmdMP4_PLAY]==before+1);
    puts("PASS first return page, resume after a fresh minute, rotated full-screen size, restart after 0202");

    reset();ticks(599);touch[0]=0x5a;touch[1]=2;ticks(1);
    assert(touch[0]==0 && !first_in_flag);ticks(599);assert(!first_in_flag);
    ticks(1);assert(first_in_flag);
    reset();touch[1]=3;ticks(1200);assert(!first_in_flag);
    touch[0]=0x5a;touch[1]=2;ticks(1);ticks(599);assert(!first_in_flag);
    ticks(1);assert(first_in_flag);
    puts("PASS a touch resets the whole minute; a held finger cannot trigger idle playback");

    for(i=18;i<=20;i++) {reset();current_page=i;ticks(1200);assert(!first_in_flag && !page_switches);}
    reset();page_st.fullvideo_flag=0;ticks(1200);assert(!first_in_flag);
    reset();ticks(300);r11_state.restart_flag=1;ticks(1);ticks(599);assert(!first_in_flag);
    ticks(1);assert(first_in_flag);
    puts("PASS first-time exclusions for video/full-screen/media-disconnect pages; disabled config and module restart");

    reset();ticks(600);assert(video_init_process!=VIDEO_PROCESS_COMPLETE);
    key=keyMP4_IDLE_RETURN;ticks(1);assert(!first_in_flag && current_page==7);
    ticks(599);assert(!commands[cmdMP4_PLAY]);ticks(9);assert(commands[cmdMP4_PLAY]==1);
    key=keyMP4_IDLE_LIST;ticks(1);assert(current_page==18 && key==0 && loop_mode==0x0102);
    puts("PASS exiting during initial setup can play on next timeout; 0201 selects the looping video list");
    return 0;
}
'''


class AdvertiseIdleTest(unittest.TestCase):
    def test_external_page_switch_guard(self):
        common = (ROOT / 'modules/r11_common.h').read_text(encoding='utf-8-sig')
        header = re.sub(r'^#include[^\n]*', '', common, flags=re.M)
        uart = (ROOT / 'source/uart.c').read_text(encoding='utf-8-sig')
        stubs = r'''
#define uartUART_82CMD_RETURN 1
PAGE_S page_st;
static uint16_t current_page, written_addr;
static unsigned writes, replies;
static UART_TYPE port;
void read_dgus_vp(uint32_t addr,uint8_t *buf,uint8_t len) {
    assert(addr==sysDGUS_PIC_NOW && len==1);memcpy(buf,&current_page,2);
}
void write_dgus_vp(uint32_t addr,uint8_t *buf,uint16_t len) {
    (void)buf;assert(len==2);written_addr=(uint16_t)addr;writes++;
}
uint8_t prvDwin8283CrcCheck(uint8_t *buf,uint16_t len,uint16_t *flag) {
    (void)buf;(void)len;*flag=0;return 1;
}
uint16_t crc_16(uint8_t *buf,uint16_t len) {(void)buf;(void)len;return 0;}
void UartSendData(UART_TYPE *uart,uint8_t *buf,uint16_t len) {
    assert(uart==&port && len==6 && !memcmp(buf,"\x5a\xa5\x03\x82\x4f\x4b",6));replies++;
}
'''
        checks = r'''
int main(void) {
    uint8_t frame[]={0x5a,0xa5,7,0x82,0,0x84,0x5a,1,0,7};
    page_st.fullvideo_flag=0x5a;page_st.fullvideo_page=19;current_page=19;
    UartStandardDwin8283Protocal(&port,frame,sizeof(frame));assert(writes==0 && replies==1);
    current_page=20;UartStandardDwin8283Protocal(&port,frame,sizeof(frame));assert(writes==0 && replies==2);
    current_page=7;UartStandardDwin8283Protocal(&port,frame,sizeof(frame));assert(writes==1 && written_addr==0x84);
    current_page=19;frame[5]=0x90;
    UartStandardDwin8283Protocal(&port,frame,sizeof(frame));assert(writes==2 && written_addr==0x90);
    frame[5]=0x84;page_st.fullvideo_page=47;
    UartStandardDwin8283Protocal(&port,frame,sizeof(frame));assert(writes==3);
    current_page=47;UartStandardDwin8283Protocal(&port,frame,sizeof(frame));assert(writes==3 && replies==6);
    puts("PASS configured full-screen/media page guards preserve normal VP writes and 82 acknowledgements");
    return 0;
}
'''
        BUILD.mkdir(exist_ok=True)
        cfile, exe = BUILD / 'advertise_page_guard.c', BUILD / 'advertise_page_guard.exe'
        cfile.write_text(PRELUDE + header + stubs + function(uart, 'UartStandardDwin8283Protocal') + checks, encoding='utf-8')
        compiler = os.environ.get('CLANG', str(ROOT.parent / 'tools/llvm-mingw-20250709-ucrt-x86_64/bin/clang.exe'))
        subprocess.run([compiler, '-std=c99', '-Wall', '-Wextra', '-Werror', str(cfile), '-o', str(exe)], check=True)
        subprocess.run([str(exe)], check=True)

    def test_production_idle_and_player(self):
        def read(path):
            return (ROOT / path).read_text(encoding='utf-8-sig')
        headers = ''.join(re.sub(r'^#include[^\n]*', '', read(path), flags=re.M)
                          for path in ('modules/r11_common.h', 'modules/r11_advertise.h'))
        advertise, common = read('modules/r11_advertise.c'), read('modules/r11_common.c')
        funcs = function(common, 'R11VideoPlayerProcess')
        funcs += ''.join(function(advertise, name) for name in (
            'R11RestartInit', 'R11NoneTouchRecoveryPlayVideo', 'R11AdvertiseTask'))
        BUILD.mkdir(exist_ok=True)
        cfile, exe = BUILD / 'advertise_idle.c', BUILD / 'advertise_idle.exe'
        cfile.write_text(PRELUDE + headers + STUBS + funcs + CHECKS, encoding='utf-8')
        compiler = os.environ.get('CLANG', str(ROOT.parent / 'tools/llvm-mingw-20250709-ucrt-x86_64/bin/clang.exe'))
        subprocess.run([compiler, '-std=c99', '-Wall', '-Wextra', '-Werror', str(cfile), '-o', str(exe)], check=True)
        subprocess.run([str(exe)], check=True)


if __name__ == '__main__':
    unittest.main()

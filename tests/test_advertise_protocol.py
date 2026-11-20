"""Exercise production V851 framing and loop highlighting with captured UART/VP writes."""
from pathlib import Path
import os
import re
import subprocess
import unittest

ROOT = Path(__file__).resolve().parents[1]
BUILD = ROOT / 'tests/.build'


def function(source, name):
    match = re.search(r'^(?:static\s+)?void\s+' + name + r'\s*\([^;]*?\)\s*\{', source, re.M)
    assert match, name
    brace = source.index('{', match.start())
    depth = 1
    for end in range(brace + 1, len(source)):
        depth += (source[end] == '{') - (source[end] == '}')
        if not depth:
            return source[match.start():end + 1] + '\n'
    raise AssertionError(name)


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
#define flashMAIN_BLOCK_ORDER 0
#define TERNARY_CODE_ADDR 0x410
#define WEBSOCKET_ADDR 0x680
typedef struct { unsigned id; } UART_TYPE;
static UART_TYPE Uart_R11, Uart2;
'''

STUBS = r'''
PLAYER_T r11_player;
uint8_t mp4_name[5][MAX_MP3_NAME_LEN];
uint16_t mp4_name_len[5];
uint16_t Icon_Overlay_SP_X[10], Icon_Overlay_SP_Y[10];
uint16_t Icon_Overlay_SP_L[10], Icon_Overlay_SP_H[10];
uint16_t pixels_arr_h2[5]={1920,1024,800,800,1024};
uint16_t pixels_arr_l2[5]={1080,600,600,480,768};
static uint8_t Big_Small_Flag;
static uint16_t color_white=0xffff, color_red=0xf800;
static struct { uint16_t video_high,video_weight,video_x_point,video_y_point; } mainview;
static struct { uint16_t screen_ratio; } screen_opt;
static uint8_t wire[256], rotation;
static uint16_t wire_len, colors[5], current_page;
static unsigned commands[256], color_writes;
void read_dgus_vp(uint32_t addr,uint8_t *buf,uint8_t len) {
    memset(buf,0,len*2);
    if(addr==sysDGUS_SYSTEM_CONFIG) buf[1]=rotation;
}
void write_dgus_vp(uint32_t addr,uint8_t *buf,uint16_t len) {
    unsigned i;
    (void)len;
    for(i=0;i<5;i++) if(addr==MP4_FILENAME_SP_LIST1+i*0x20+3) {
        memcpy(&colors[i],buf,2);color_writes++;
    }
}
void UartSendData(UART_TYPE *uart,uint8_t *buf,uint16_t len) {
    if(uart==&Uart_R11) {memcpy(wire,buf,len);wire_len=len;commands[buf[4]]++;}
}
uint16_t crc_16(uint8_t *buf,uint16_t len) {(void)buf;(void)len;return 0x1234;}
void DgusToFlash(uint16_t a,uint16_t b,uint16_t c,uint16_t d) {(void)a;(void)b;(void)c;(void)d;}
void SwitchPageById(uint16_t page) {current_page=page;}
static void ExtractFilenamesFromProtocol(uint8_t *frame,uint16_t len) {(void)frame;(void)len;}
'''

CHECKS = r'''
static unsigned word(unsigned i) {return (wire[i]<<8)|wire[i+1];}
static void expect_red(int row) {
    unsigned i;for(i=0;i<5;i++) assert(colors[i]==(i==(unsigned)row?0xf800:0xffff));
}
static void report(uint8_t serial) {
    uint8_t frame[]={0xaa,0x55,0,2,0xb1,0};frame[5]=serial;
    UartR11UserVideoProtocol(&Uart_R11,frame,sizeof(frame));
}
int main(void) {
    unsigned before, i, j;
    uint8_t loop[]={0xaa,0x55,0,3,0x90,1,2};
    uint8_t stop_loop[]={0xaa,0x55,0,2,0x90,0};
    uint8_t ack[]={0xaa,0x55,0,9,0x66,1,0,1,3,0xe0,1,0xc0,1};
    r11_player.store_type=exUDISK;r11_player.now_play_page=1;r11_player.total_serial=12;
    report(0);expect_red(0);assert(commands[0x61]==0);
    report(0);expect_red(0);assert(commands[0x61]==0);
    report(4);expect_red(4);assert(commands[0x62]==0);
    report(5);expect_red(0);assert(commands[0x62]==1 && r11_player.now_play_page==2);
    report(6);expect_red(1);assert(commands[0x62]==1);
    report(0);expect_red(0);assert(commands[0x61]==1 && r11_player.now_play_page==1);
    report(0);expect_red(0);assert(commands[0x61]==1);
    report(255);expect_red(-1);
    puts("PASS first/repeated item, cross-page loop, wrap to first page and invalid index colors");
    r11_player.now_play_serial=0;before=color_writes;
    UartR11UserVideoProtocol(&Uart_R11,loop,sizeof(loop));
    assert(color_writes==before && r11_player.now_play_serial==0);
    UartR11UserVideoProtocol(&Uart_R11,stop_loop,sizeof(stop_loop));
    assert(color_writes==before && r11_player.now_play_serial==0);
    puts("PASS loop mode replies cannot fall through as playback-index reports");

    mainview.video_high=992;mainview.video_weight=448;
    r11_player.serial=0;r11_player.state=0;r11_player.store_type=exUDISK;
    memcpy(mp4_name[0],"1.mp4",5);mp4_name_len[0]=5;
    T5lSendUartDataToR11(cmdMP4_PLAY,mp4_name[0]);
    assert(wire[4]==0x66 && wire_len==37 && word(2)==33);
    assert(word(7)==992 && word(9)==448 && word(11)==24);
    assert(!memcmp(wire+13,"/mnt/exUDISK/VIDEO/1.mp4",24));
    UartR11UserVideoProtocol(&Uart_R11,ack,sizeof(ack));
    T5lSendUartDataToR11(cmdMP4_PLAY,mp4_name[0]);assert(wire[4]==0x76);
    T5lSendUartDataToR11(cmdMP4_PAUSE,NULL);
    assert(wire_len==5 && !memcmp(wire,"\xaa\x55\x00\x01\x65",5));
    T5lSendUartDataToR11(cmdMP4_REPLAY,NULL);
    assert(wire_len==5 && !memcmp(wire,"\xaa\x55\x00\x01\x78",5));
    for(i=0;i<5;i++) for(j=0;j<4;j++) {
        screen_opt.screen_ratio=i;rotation=j;
        R11ChangePictureLocate(0,0,1920,1080,1);
        assert(wire_len==11 && wire[4]==0x68 && word(2)==7);
        assert(word(7)==((j&1)?pixels_arr_l2[i]:pixels_arr_h2[i]));
        assert(word(9)==((j&1)?pixels_arr_h2[i]:pixels_arr_l2[i]));
    }
    puts("PASS V851 0x66/0x76 paths, command-only 0x65/0x78, 0x68 sizes for every resolution/rotation");
    return 0;
}
'''


class AdvertiseProtocolTest(unittest.TestCase):
    def test_production_framing_and_highlight(self):
        header = (ROOT / 'modules/r11_common.h').read_text(encoding='utf-8-sig')
        header = re.sub(r'^#include[^\n]*', '', header, flags=re.M)
        source = (ROOT / 'modules/r11_common.c').read_text(encoding='utf-8-sig')
        funcs = ''.join(function(source, name) for name in (
            'R11FillVideoDisplayParams', 'T5lSendUartDataToR11',
            'R11ChangePictureLocate', 'R11PlayHighlightVideo', 'UartR11UserVideoProtocol'))
        BUILD.mkdir(exist_ok=True)
        cfile, exe = BUILD / 'advertise_protocol.c', BUILD / 'advertise_protocol.exe'
        cfile.write_text(PRELUDE + header + STUBS + funcs + CHECKS, encoding='utf-8')
        compiler = os.environ.get('CLANG', str(ROOT.parent / 'tools/llvm-mingw-20250709-ucrt-x86_64/bin/clang.exe'))
        subprocess.run([compiler, '-std=c99', '-Wall', '-Wextra', '-Werror',
                        '-Wno-unused-parameter', str(cfile), '-o', str(exe)], check=True)
        subprocess.run([str(exe)], check=True)


if __name__ == '__main__':
    unittest.main()

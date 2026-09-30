
/*
 * 文件名: r11_common.c
 * 说明: R11模块通用实现文件，包含视频播放、WiFi等相关功能实现。
 */
#include "r11_common.h"
#if sysBEAUTY_MODE_ENABLED
#include "r11_netskinAnalyze.h"
#endif /* sysBEAUTY_MODE_ENABLED */
#if sysN5CAMERA_MODE_ENABLED
#include "r11_n5camera.h"
#endif /* sysN5CAMERA_MODE_ENABLED */
#if sysADVERTISE_MODE_ENABLED
#include "r11_advertise.h"
#endif /* sysADVERTISE_MODE_ENABLED */
#include "sys.h"
#include "T5LOSConfig.h"
#include "uart.h"
#include "timer.h"
#include <string.h>

#if sysBEAUTY_MODE_ENABLED || sysN5CAMERA_MODE_ENABLED || sysADVERTISE_MODE_ENABLED
/**
 * @brief 调试数据定义区域
 * @note 所有变量均有物理地址映射，便于调试和数据追踪。
 */
volatile uint8_t    xdata       buf_jpeg[0x8000]            _at_    0x8000;
volatile uint8_t    data        EX1_Len                     _at_    0x20;
volatile uint8_t    data        Picture_Count               _at_    0x21;
volatile uint16_t   data        buf_tail                    _at_    0x22;
volatile uint16_t   data        buf_os_tail                 _at_    0x24;
volatile uint8_t    data        Packet_Count                _at_    0x26;
volatile uint8_t    data        Packet_TotalCount           _at_    0x27;
volatile uint32_t   data        JpegLaber_OS_ADR            _at_    0x28;
volatile uint32_t   data        JpegLaber_DGUSII_VP         _at_    0x2C;

volatile uint8_t    data        Other_Cmd_Count             _at_    0x30;
volatile uint8_t    data        Miss_Align_16KB_Count       _at_    0x31;
volatile uint8_t    data        NotDeal_OverBuffer_Count    _at_    0x32;
volatile uint8_t    data        Over_Max_16KB_Count         _at_    0x33;
volatile uint8_t    data        CRC_ERR_Count               _at_    0x34;
volatile uint8_t    data        data_write_f                _at_    0x35;
volatile uint8_t    data        state                       _at_    0x36;
volatile uint8_t    data        Kind                        _at_    0x37;

volatile uint8_t    data        Icon_Num                    _at_    0x38;
volatile uint8_t    data        Max_16KB_Count              _at_    0x39;
volatile uint8_t    data        Big_Small_Flag              _at_    0x3A;
volatile uint8_t    data        Rx3_Len                     _at_    0x3B;
volatile uint8_t    data        END_CRC                     _at_    0x3C;
volatile uint8_t    data        Dgus_Need_Restart           _at_    0x3D;
volatile uint8_t    data        Restart_Dgus_Time           _at_    0x3E;
volatile uint8_t    data        TimeOut_Count               _at_    0x3F;

volatile uint8_t    data        TimeOut                     _at_    0x40;
volatile uint16_t   data        Jpeg_Word_Len               _at_    0x41;
volatile uint8_t    data        Pick_Picture                _at_    0x43;
volatile uint8_t    data        Picture_Flag                _at_    0x44;
volatile uint8_t    data        pic_time_cnt                _at_    0x45;
volatile uint8_t    data        Bit8_16_Flag                _at_    0x46;
volatile uint16_t   data        write_f_time                _at_    0x47;
volatile uint8_t    idata       Pic_Count[10]               _at_    0xb0;


code uint16_t Icon_Overlay_SP[11] = {0x0700,0x0700,0x0700,0x0700,0x0710,0x0720,0x0730,0x0740,0x0750,0x0760,0x770};
uint32_t Icon_Overlay_SP_VP[11] = {0x07000,0x1d000,0x07000,0x1d000,0x33000,0x37000,0x3b000,0x3f000,0x3f000,0x3f000,0x3F000};
uint16_t Icon_Overlay_SP_X[10];  
uint16_t Icon_Overlay_SP_Y[10];
uint16_t Icon_Overlay_SP_L[10];
uint16_t Icon_Overlay_SP_H[10]; 
uint16_t Locate_arr[10]; 
PAGE_S page_st;

uint8_t mp4_name[5][MAX_MP3_NAME_LEN]=0;
uint16_t mp4_name_len[5];
PLAYER_T r11_player;

VIDEO_INIT_PROCESS video_init_process = VIDEO_PROCESS_UNINIT;
uint8_t wifi_now_offset = 0;

/*用于视频播放列表高亮显示的颜色*/
uint16_t color_white = 0xFFFF;
uint16_t color_red = 0xF800;

/** @note 适用于全屏播放的分辨率参数，由于最高只能显示1280*720，所以在1920*1080的分辨率上面进行特殊处理 */
// uint16_t pixels_arr_h[5]={1280,1024,800,800,1024};
// uint16_t pixels_arr_l[5]={720,600,600,480,768};
uint16_t pixels_arr_h[5]={1920,1024,800,800,1024};
uint16_t pixels_arr_l[5]={1080,600,600,480,768};

uint16_t pixels_arr_h2[5]={1920,1024,800,800,1024};
uint16_t pixels_arr_l2[5]={1080,600,600,480,768};

void T5lJpegInit(void)
{
    P1MDOUT         =   0x00;
    P2MDOUT         =   0x00;
    P3MDOUT         &= ~0x03;
    IT0             =   1;
    IT1             =   1;
    EX0             =   0;
    EX1             =   0;
    data_write_f    =   0;
    memset(buf_jpeg, 0, sizeof(buf_jpeg));
    EX1_Start();
}


void R11ChangePictureLocate(uint16_t x_point,uint16_t y_point,uint16_t high,uint16_t weight,uint8_t big_small_flag)
{
    uint8_t write_param[4];
    Icon_Overlay_SP_X[0] = Icon_Overlay_SP_X[1] = x_point;
    Icon_Overlay_SP_Y[0] = Icon_Overlay_SP_Y[1] = y_point;
    Icon_Overlay_SP_L[0] = Icon_Overlay_SP_L[1] = high;
    Icon_Overlay_SP_H[0] = Icon_Overlay_SP_H[1] = weight;
    if(big_small_flag == 0x00 || big_small_flag == 0x01)
    {
        write_param[0] = Icon_Overlay_SP_L[0] >> 8;
        write_param[1] = Icon_Overlay_SP_L[0];
        write_param[2] = Icon_Overlay_SP_H[0] >> 8;
        write_param[3] = Icon_Overlay_SP_H[0];
        T5lSendUartDataToR11(cmdMP4_IMG_SET+big_small_flag, write_param);
    }else
    {
        __NOP();
    }
}


/** 遍历len长度的数组，遇到0x00，或者0xff，或者'\0',将剩下的数据替换成0x00
 * 如果前几个字符不是/mnt/UDISK/和/mnt/SDCARD/和/mnt/exUDISK/,则替换为/mnt/UDISK/tmp
 */
void FormatArrayToFullPath(uint8_t *buf, uint8_t len)
{
    uint8_t i;
    if (memcmp(buf, "/mnt/", 5) != 0)
    {
        memcpy(buf, "/mnt/UDISK/tmp", 14);
        buf[14] = 0x00;
    }

    for (i = 0; i < len; i++)
    {
        if (buf[i] == 0x00 || buf[i] == 0xff || buf[i] == '\0')
        {
            memset(&buf[i], 0x00, len - i);
            break;
        }
    }
}



void T5lSendUartDataToR11( uint8_t cmd, uint8_t *buf)
{
    #define BUF_MAX_LEN  256
    uint8_t 	i,len,now_len,Temp_Buf_uint8_t[10];
    uint8_t     r11_buf[BUF_MAX_LEN];
    uint16_t    CRC16;
    r11_buf[0] = 0xAA;
    r11_buf[1] = 0x55;
    r11_buf[4] = cmd;
    switch (cmd)
    {
        case cmdMP4_UPDATEFILE: 
            /* A list query must not overwrite actual playback identity. */
            r11_player.now_play_page = 1;
            /* Shared two-byte payload. */
        case cmdMP4_ROTATE_ANGLE: 
        case cmdMP4_LOOP_MODE_SET:
            r11_buf[2] = 0x00;
            r11_buf[3] = 0x03;
            r11_buf[5] = buf[0];
            r11_buf[6] = buf[1];
            break;
        case cmdMP4_AUX_SET:
            r11_buf[2] = 0x00;
            r11_buf[3] = 0x02;
            r11_buf[5] = buf[0];
            break;
        case cmdMP4_PLAY_BY_NUM:
            r11_buf[2] = 0x00;
            r11_buf[3] = 0x04;
            r11_buf[5] = buf[0];
            r11_buf[6] = buf[1];
            r11_buf[7] = buf[2];
            break;
        case cmdMP4_PREVFILE:
        case cmdMP4_NEXTFILE: 
        case cmdMP4_PAUSE: 
        case cmdMP4_REPLAY: 
        case cmdMP4_STOP: 
            r11_buf[2] = 0x00;
            r11_buf[3] = 0x02;
            r11_buf[5] = 0x00;
            break;
        case cmdMP4_PLAY: 
            len = mp4_name_len[r11_player.serial];
            r11_buf[2] = 0x00;
            r11_buf[3] = 0x01;
            if (len > MAX_MP3_NAME_LEN)
            {
                return;
            }
            now_len = 5;

            switch (r11_player.store_type)
            {
                case 1:
                case 2:
                case 3:
                    Temp_Buf_uint8_t[0] = strlen ( DirPath ( r11_player.store_type ) );
                    memcpy ( &r11_buf[5], DirPath ( r11_player.store_type ), Temp_Buf_uint8_t[0] );
                    now_len += Temp_Buf_uint8_t[0];
                    break;
                default:
                    r11_player.store_type = exUDISK;
                    Temp_Buf_uint8_t[0] = strlen ( DirPath ( r11_player.store_type ) ) ;
                    memcpy ( &r11_buf[5], DirPath ( r11_player.store_type ), Temp_Buf_uint8_t[0] );
                    now_len += Temp_Buf_uint8_t[0];
                    break;
            }
            memcpy ( &r11_buf[now_len], buf, len );
            len += now_len;
            r11_buf[len++] = 0x00;
            r11_buf[3] += len - 5;
            break;
        case cmdMP4_IMG_SET: 
        case (cmdMP4_IMG_SET+1): 
            r11_buf[2] = 0x00;
            r11_buf[3] = 0x05;
            r11_buf[5] = buf[0];
            r11_buf[6] = buf[1];
            r11_buf[7] = buf[2];
            r11_buf[8] = buf[3];
            break;
        case cmdCHECK_STATUS_NET:
        case cmdCHECK_STATUS_DEVICE: 
            r11_buf[2] = buf[2];
            r11_buf[3] = buf[3];
            r11_buf[4] = buf[4];
            r11_buf[5] = buf[5];
            r11_buf[6] = buf[6];
            break;
        #if sysADVERTISE_MODE_ENABLED
        /** 适用于广告屏，用来进行三元码的注册和websocket连接设置 */
        case cmdSET_TERNARY_CODE:
            r11_buf[2] = 0x00;
            r11_buf[3] = 12;
            read_dgus_vp(TERNARY_CODE_ADDR+0x01, ( uint8_t * ) &r11_buf[5],5 );
            r11_buf[15] = '\0';
            break;
        case cmdSET_WEBSOCKET:
            read_dgus_vp(WEBSOCKET_ADDR, ( uint8_t * ) &r11_buf[5],32 );
            for ( i=0; i<64; i++ )
            {
                if ( r11_buf[5+i] == 0xFF )
                {
                    r11_buf[5+i] = '\0';
                    i++;
                    break;
                }
            }
            r11_buf[2] = 0x00;
            r11_buf[3] = i+1;
            break;
        #endif /* sysADVERTISE_MODE_ENABLED */
        #if sysN5CAMERA_MODE_ENABLED
        case cmdN5_CAMERA_AV1_OPEN:
        case cmdN5_CAMERA_AV2_OPEN:
        case cmdN5_CAMERA_CLOSE:
        case cmdN5_CAMERA_MODE:
            r11_buf[2] = buf[2];
            r11_buf[3] = buf[3];
            r11_buf[4] = buf[4];
            r11_buf[5] = buf[5];
            break;
        #endif /* sysN5CAMERA_MODE_ENABLED */
        default:
            return;
            break;
    }
    CRC16 = crc_16 ( &r11_buf[4], r11_buf[3] );
    r11_buf[r11_buf[3]+4] = CRC16 >> 8;
    r11_buf[r11_buf[3]+5] = CRC16 & 0xFF;
    r11_buf[3] += 2;
    UartSendData ( &Uart_R11, r11_buf, r11_buf[3]+4 );
    return;
}


void R11DebugValueHandle(uint16_t dgus_value)
{
    #define DEBUG_UART_ORDER         Uart2
    uint32_t i;
    uint8_t r11_send_buf[6];
    if(dgus_value == 0x111)
    {
        for ( i=0; i<16384*3; i++ )
        {
            read_dgus_vp ( Icon_Overlay_SP_VP[0]+2*i, ( uint8_t * ) &r11_send_buf[0],2 );
            UartSendData ( &DEBUG_UART_ORDER,&r11_send_buf[3], 1);
            UartSendData ( &DEBUG_UART_ORDER,&r11_send_buf[2], 1);
            UartSendData ( &DEBUG_UART_ORDER,&r11_send_buf[1], 1);
            UartSendData ( &DEBUG_UART_ORDER,&r11_send_buf[0], 1);
            delay_ms ( 1 );
        }
    }else if(dgus_value == 0x11a)
    {
        /** 将设置项写入Flash */
        DgusToFlash(flashMAIN_BLOCK_ORDER, PIXELS_SET_ADDR, PIXELS_SET_ADDR, 0x48);
        #if sysSET_FROM_LIB
        R11ConfigInitFormLib();
        #if sysBEAUTY_MODE_ENABLED
        R11PageInitChange();
        #endif /* sysBEAUTY_MODE_ENABLED */
        #endif /* sysSET_FROM_LIB */
    }
    #if sysBEAUTY_MODE_ENABLED
    else if(dgus_value == 0x11c)
    {
        /** 查询摄像头支持格式 */
        UartSendData ( &Uart_R11,"\xAA\x55\x00\x02\xB4\x01", 6);
    }
    #endif /* sysBEAUTY_MODE_ENABLED */
    else if(dgus_value == 0x11d)
    {
        if(data_write_f > 2)
        {
            EX0 = 0;
            EX1 = 0;
            data_write_f = 0;
            EX1_Start();
        }
    }
}


void R11ClearPicture(uint8_t clear_type)
{
    uint16_t write_param[2] = {0x5b5b,0x5b5b};
    #if sysBEAUTY_MODE_ENABLED
    uint16_t i;
    #else
    (void)clear_type;
    #endif

    write_dgus_vp(Icon_Overlay_SP_VP[0],(uint8_t*)write_param,2);
    write_dgus_vp(Icon_Overlay_SP_VP[1],(uint8_t*)write_param,2);
    #if sysBEAUTY_MODE_ENABLED
    if(clear_type)
    {
        for(i=0;i<screen_opt.thumbnail_num;i++)
        {
            write_dgus_vp(Icon_Overlay_SP_VP[4+i],(uint8_t*)write_param,2);
            camera_magnifier.camera_num[i] = 0;
        }
        r11_state.now_choose_pic = 0;
        write_dgus_vp(cameraNOW_NUM_ADDR,(uint8_t*)&r11_state.now_choose_pic,1);
    }
    #endif /* sysBEAUTY_MODE_ENABLED */
}


void R11VideoPlayerProcess(void)
{
    static uint16_t search_retry_count = 10;
    static uint16_t read_param[6] = 0;     /**@note 需要改为静态变量，因为会多次进入此函数 */
    uint16_t rotate_angle;
    uint8_t r11_send_buf[6];
    if(video_init_process == VIDEO_PROCESS_UNINIT)
    {
	    T5lJpegInit();
        r11_send_buf[0] = 0x00;
        r11_send_buf[1] = 20;
        write_dgus_vp(sysWAE_PLAY_ADDR, r11_send_buf, 1);
        /* 初始化视频当前页和当前播放的编号和总量*/
        r11_player.now_play_page = 1;
        r11_player.now_play_serial = 0;
        r11_player.total_serial = 0;
        /** 0.读取音量和循环信息 */
        FlashToDgus(flashMAIN_BLOCK_ORDER, VOLUME_SET_ADDR, VOLUME_SET_ADDR,0x06);
        read_dgus_vp(VOLUME_SET_ADDR, (uint8_t *)&read_param[0], 6);
        video_init_process = VIDEO_PROCESS_VOLUME;
    }else if(video_init_process == VIDEO_PROCESS_VOLUME)
    {
        if(read_param[0] > MAX_VOLUME)
        {
            read_param[0] = MAX_VOLUME;
            write_dgus_vp(VOLUME_SET_ADDR,(uint8_t*)&read_param[0],1);
        }
        r11_player.r11_volunme = (uint8_t)read_param[0];
        T5lSendUartDataToR11(cmdMP4_AUX_SET, &r11_player.r11_volunme);
        video_init_process = VIDEO_PROCESS_SEARCH_LOOP;
    }else if(video_init_process == VIDEO_PROCESS_SEARCH_LOOP)
    {
        #if sysADVERTISE_MODE_ENABLED
        /* The local UI sets the loop only after the first file starts. */
        video_init_process = VIDEO_PROCESS_COMPLETE;
        #else
        /** 2.检查循环播放设置,如果开启循环，则认为已经开始自动播放，0xxx00为不循环，0xxx01循环当前，0x0002循环云端的视频发给r11不更新规则，0x0102循环所有并且发给r11更新规则 */
        if((read_param[2]&0x00ff) != 0x0000)
        {
            video_init_process = VIDEO_PROCESS_SIZE;
        }else
        {
            video_init_process = VIDEO_PROCESS_COMPLETE;
        }
        #endif
    }else if(video_init_process == VIDEO_PROCESS_SIZE)
    {
        /** 3.设置视频显示初始大小 */
        if(read_param[5] == 0x0001)
        {
            Big_Small_Flag = 0x01;
            if(page_st.fullvideo_flag == 0x5a)
            {
                SwitchPageById((uint16_t)page_st.fullvideo_page); 
            }
            read_dgus_vp(sysDGUS_SYSTEM_CONFIG, (uint8_t *)&rotate_angle, 1);
            if((rotate_angle & 0x0003) == 0x00 || (rotate_angle & 0x0003) == 0x02)
            {
                #if sysBEAUTY_MODE_ENABLED
                R11ChangePictureLocate((pixels_arr_h2[screen_opt.screen_ratio]-pixels_arr_h[screen_opt.screen_ratio])/2,
                (pixels_arr_l2[screen_opt.screen_ratio]-pixels_arr_l[screen_opt.screen_ratio])/2,
                pixels_arr_h[screen_opt.screen_ratio],
                pixels_arr_l[screen_opt.screen_ratio],0x01);
                #endif /* sysBEAUTY_MODE_ENABLED */
                #if sysADVERTISE_MODE_ENABLED
                R11ChangePictureLocate(0,0,pixels_arr_h2[screen_opt.screen_ratio],pixels_arr_l2[screen_opt.screen_ratio],0x01);
                #endif /* sysADVERTISE_MODE_ENABLED */

            }else if((rotate_angle & 0x0003) == 0x01 || (rotate_angle & 0x0003) == 0x03)
            {
                #if sysBEAUTY_MODE_ENABLED
                R11ChangePictureLocate((pixels_arr_l2[screen_opt.screen_ratio]-pixels_arr_l[screen_opt.screen_ratio])/2,
                (pixels_arr_h2[screen_opt.screen_ratio]-pixels_arr_h[screen_opt.screen_ratio])/2,
                pixels_arr_l[screen_opt.screen_ratio],
                pixels_arr_h[screen_opt.screen_ratio],0x01);
                #endif /* sysBEAUTY_MODE_ENABLED */
                #if sysADVERTISE_MODE_ENABLED
                R11ChangePictureLocate(0,0,pixels_arr_l2[screen_opt.screen_ratio],pixels_arr_h2[screen_opt.screen_ratio],0x01);
                #endif /* sysADVERTISE_MODE_ENABLED */
            }
        }else if(read_param[5] == 0x0000)
        {
            Big_Small_Flag = 0x00;
            if(page_st.video_flag == 0x5a)
            {
                SwitchPageById((uint16_t)page_st.video_page); 
            }

            R11ChangePictureLocate(mainview.video_x_point,mainview.video_y_point,mainview.video_high,mainview.video_weight,0x00);
        }
        video_init_process = VIDEO_PROCESS_QUERY;
    }else if(video_init_process == VIDEO_PROCESS_QUERY)
    {
        #if sysBEAUTY_MODE_ENABLED
        r11_send_buf[0] = r11_player.store_type = SDCARD;
        #elif sysADVERTISE_MODE_ENABLED
        r11_send_buf[0] = r11_player.store_type = UDISK;
        #else
        r11_send_buf[0] = r11_player.store_type = SDCARD;
        #endif /* sysADVERTISE_MODE_ENABLED END */
        r11_send_buf[1] = MP4;
        T5lSendUartDataToR11(cmdMP4_UPDATEFILE, r11_send_buf);
        if((read_param[2]&0xff00) != 0x0000)
        {
            video_init_process = VIDEO_PROCESS_PLAY;
        }else
        {
            /* 采用老规则播放时不再自动播放*/
            video_init_process = VIDEO_PROCESS_SET_LOOP;
        }
    }else if(video_init_process == VIDEO_PROCESS_PLAY)
    {
        r11_send_buf[0] = 0;
        if (r11_send_buf[0] < r11_player.page_mp4_nums) {
            r11_player.serial = r11_send_buf[0];
            write_dgus_vp(MP4_NOW_PLAY_NAME_ADDR,(uint8_t*)&mp4_name[r11_player.serial][0], MAX_MP3_NAME_LEN>>1);
            T5lSendUartDataToR11(cmdMP4_PLAY, mp4_name[r11_player.serial]);
            video_init_process = VIDEO_PROCESS_SET_LOOP;
            /*对第一个结果进行高亮显示*/
            write_dgus_vp(MP4_FILENAME_SP_LIST1 + (r11_player.now_play_serial % 5) * 0x20 + 0x03,(uint8_t*)&color_red,1);
        }else{
            search_retry_count --;
            if(search_retry_count == 0)
            {
                video_init_process = VIDEO_PROCESS_SET_LOOP; /* 设置为查询视频状态 */
            }else{
                video_init_process = VIDEO_PROCESS_PLAY;
            }
        }
    }else if(video_init_process == VIDEO_PROCESS_SET_LOOP)
    {
        if((read_param[2]&0xff00) == 0x0000)
        {
            /*采用老规则，并且循环播放*/
            read_param[2] = 0x0100;
        }
        T5lSendUartDataToR11(cmdMP4_LOOP_MODE_SET,(uint8_t *)&read_param[2]);
        video_init_process = VIDEO_PROCESS_COMPLETE;
    }else if(video_init_process == VIDEO_PROCESS_COMPLETE){
        __NOP();
    }
}


/* Local video UI and touch-inactivity advertising. No FW page/command writes. */
static uint16_t xdata video_ui_page=0xffff, video_ui_pending, video_ui_target;
static uint8_t xdata video_ui_enter, video_ui_known, video_ui_query, video_title_dirty, video_resume_fallback, video_next_due;
static uint32_t xdata video_ui_sent;
/* Query: 0 not sent, 1 waiting, 2 received, 3 timed out (late reply allowed).
 * R11 does not acknowledge a T5L 0x90 loop setting: track transmission only. */
static uint8_t xdata video_loop_sent;
static uint16_t xdata video_idle_return=42, video_idle_seen=0xffff;
/* Entry ownership survives page-readback delay and R11 restart. Only an
 * explicit fullscreen key on page 120 may use 120 as its return target. */
static uint16_t xdata video_full_origin=0xffff;
static uint8_t xdata video_idle_entering;
static uint8_t xdata video_idle_active, video_idle_started, video_touch_down, video_wake_pending, video_wake_guard;
static uint32_t xdata video_idle_at, video_wake_at;
static uint32_t VideoTime(void)
{
    uint32_t t; uint8_t enabled=ET0; ET0=0; t=GetSysTick(); ET0=enabled; return t;
}
static void VideoVp(uint16_t a,uint16_t v) { write_dgus_vp(a,(uint8_t*)&v,1); }
static uint16_t VideoGet(uint16_t a) { uint16_t v; read_dgus_vp(a,(uint8_t*)&v,1); return v; }
static uint16_t VideoCount(void)
{
    uint16_t count=r11_player.total_serial;
    if(count>VIDEO_UI_MAX_FILES) count=VIDEO_UI_MAX_FILES;
    if(count>r11_player.page_mp4_nums) count=r11_player.page_mp4_nums;
    return count;
}
static void VideoSend(uint8_t cmd,uint16_t serial)
{
    uint8_t buf[3]; buf[0]=MP4; buf[1]=(uint8_t)(serial>>8); buf[2]=(uint8_t)serial;
    T5lSendUartDataToR11(cmd,buf);
    video_ui_pending=cmd; video_ui_target=serial; video_ui_sent=VideoTime();
    VideoVp(VIDEO_UI_STATUS_ADDR,1);
}
/* The existing 0x21..0x25 keys select filename slots, using R11 command 0x64.
 * Do not send these UI key values as R11 wire opcodes or use 0x74 here. */
static void VideoPlayFileKey(uint16_t key)
{
    uint16_t slot=key-keyMP4_1FILE;
    if(key<keyMP4_1FILE || key>keyMP4_5FILE || slot>=VideoCount()) return;
    if(!mp4_name_len[slot]) { VideoVp(VIDEO_UI_STATUS_ADDR,2); return; }
    r11_player.store_type=UDISK; r11_player.serial=(uint8_t)slot;
    T5lSendUartDataToR11(cmdMP4_PLAY,mp4_name[slot]);
    video_ui_pending=cmdMP4_PLAY; video_ui_target=slot; video_ui_sent=VideoTime();
    video_next_due=0; VideoVp(VIDEO_UI_STATUS_ADDR,1);
}
static uint8_t VideoIdlePage(uint16_t p)
{
    return p==1 || p==2 || p==27 || p==42;
}
uint8_t R11VideoIdleActive(void)
{
    return (video_idle_active || VideoIdlePage(video_full_origin)) && ReadPageId()==121;
}
uint16_t R11VideoIdleReturnPage(void) { return video_idle_return; }
uint8_t R11VideoWakeBlocked(void) { return video_wake_guard; }
static void VideoIdleDismiss(void)
{
    uint16_t target=VideoIdlePage(video_full_origin)?video_full_origin:video_idle_return;
    if(!VideoIdlePage(target)) target=42;
    video_full_origin=0xffff; video_idle_entering=0;
    video_idle_active=0; video_wake_pending=0; video_wake_guard=1;
    video_idle_at=video_wake_at=VideoTime();
    video_ui_enter=0; video_next_due=0;
    /* Wake always pauses and returns directly to the originating business page. */
    VideoSend(cmdMP4_PAUSE,0);
    VideoVp(0x0600,0); VideoVp(0x3710,0);
    video_idle_seen=target;
    SwitchPageById(target);
}
/* Called at 10 ms from the FW task, independent of the 100 ms R11 scanner.
 * DGUS 0x0016: 5A01 press, 5A03 held, 5A02 release; clear update flag after read. */
void R11VideoTouchTask(uint8_t allow_start)
{
    uint32_t now=VideoTime();
    uint16_t touch=VideoGet(sysDGUS_TP_STATUS),p=ReadPageId();
    uint8_t updated=(touch>>8)==0x5a, state=(uint8_t)touch;
    if(!video_idle_started) { video_idle_started=1; video_idle_at=now; }
    if(p==121 && VideoIdlePage(video_full_origin)) {
        video_idle_active=1; video_idle_entering=0;
    }
    if(video_idle_active && p!=121) {
        /* The switch command may complete before the current-page VP changes.
         * Do not discard the first entry's return context during that gap. */
        if(video_idle_entering && p==video_idle_return && now-video_idle_at<1000UL) return;
        video_idle_active=video_idle_entering=0; video_full_origin=0xffff;
        video_wake_pending=0; video_idle_at=now;
    }
    if(p!=video_idle_seen) { video_idle_seen=p; video_idle_at=now; }
    if(updated) {
        VideoVp(sysDGUS_TP_STATUS,0);
        if(state<=3) {
            video_idle_at=now;
            if(state==1 || state==3) video_touch_down=1;
            else video_touch_down=0;
            if(video_idle_active) {
                video_wake_pending=1;
                /* Return on release: the wake touch cannot hit the underlying page. */
                if(!video_touch_down) { VideoIdleDismiss(); return; }
            }
        }
    }
    if(video_wake_guard) {
        VideoVp(0x0600,0); VideoVp(0x3710,0);
        if(video_touch_down) video_idle_at=video_wake_at=now;
        else if(now-video_wake_at>=200UL) video_wake_guard=0;
        return;
    }
    if(video_touch_down) { video_idle_at=now; return; }
    if(VideoGet(0x3710) || VideoGet(0x0600)) video_idle_at=now;
    if(video_idle_active) return;
    /* Only these business/settings pages participate in idle advertising. */
    if(!allow_start || !VideoIdlePage(p) || video_init_process!=VIDEO_PROCESS_COMPLETE) { video_idle_at=now; return; }
    if(now-video_idle_at>=VIDEO_IDLE_TIMEOUT_MS) {
        video_idle_return=p; video_idle_active=1; video_wake_pending=0;
        video_full_origin=p; video_idle_entering=1; video_idle_at=now;
        video_ui_enter=1; video_ui_query=0; video_resume_fallback=0;
        video_idle_seen=121;
        SwitchPageById(121);
    }
}
static void VideoFullFailure(void)
{
    if(R11VideoIdleActive()) VideoIdleDismiss();
    else if(ReadPageId()==121) {
        if(video_full_origin==120) { video_full_origin=0xffff; SwitchPageById(120); }
        else VideoIdleDismiss();
    }
}
void R11VideoUiReset(void)
{
    uint8_t empty[MAX_MP3_NAME_LEN];
    /* Preserve idle return context across an R11 restart. */
    video_ui_page=0xffff; video_ui_pending=video_ui_enter=video_ui_known=video_ui_query=0;
    video_loop_sent=0;
    VideoVp(VIDEO_UI_STATUS_ADDR,2); VideoVp(VIDEO_UI_COUNT_ADDR,0); VideoVp(VIDEO_UI_BUTTON_ADDR,2);
    r11_player.state=0; r11_player.total_serial=r11_player.page_mp4_nums=0;
    r11_player.store_type=UDISK;
    VideoVp(PLAY_STATUS_ADDR,0); memset(empty,0,sizeof(empty));
    write_dgus_vp(MP4_NOW_PLAY_NAME_ADDR,empty,MAX_MP3_NAME_LEN>>1);
    video_title_dirty=1; video_resume_fallback=video_next_due=0;
    page_st.video_flag=page_st.fullvideo_flag=0x5a; page_st.video_page=120; page_st.fullvideo_page=121;
    mainview.video_x_point=16; mainview.video_y_point=64; mainview.video_high=992; mainview.video_weight=448;
}
void R11VideoUiTask(void)
{
    uint16_t p=ReadPageId(),count=VideoCount();
    uint8_t active=(p==120 || p==121),was=(video_ui_page==120 || video_ui_page==121);
    uint8_t query[2];
    VideoVp(VIDEO_UI_COUNT_ADDR,count);
    VideoVp(VIDEO_UI_BUTTON_ADDR,video_ui_pending?2:(r11_player.state==1?0:(r11_player.state==4?1:2)));
    if(p!=video_ui_page) {
        if(active && !was) {
            video_ui_enter=1; video_ui_query=0; video_resume_fallback=0;
        }
        if(!active && was) {
            video_ui_enter=0; video_next_due=0;
            if(video_ui_pending!=cmdMP4_PAUSE && r11_player.state!=4) VideoSend(cmdMP4_PAUSE,0);
        }
        if(p==121) { R11ChangePictureLocate(0,0,1024,600,1); Big_Small_Flag=1; }
        else if(p==120) { R11ChangePictureLocate(16,64,992,448,0); Big_Small_Flag=0; }
        else if(p==9) R11ChangePictureLocate(16,116,562,434,0);
        else R11ChangePictureLocate(1024,600,1,1,0);
        VideoVp(BIG_SMALL_FLAG_ADDR,p==121); R11ClearPicture(0); video_ui_page=p;
    }
    if(video_init_process!=VIDEO_PROCESS_COMPLETE) return;
    if(video_ui_pending && VideoTime()-video_ui_sent>=(video_ui_pending==cmdMP4_UPDATEFILE?5000UL:1500UL)) {
        if(video_ui_pending==cmdMP4_REPLAY && !video_resume_fallback && active) {
            video_ui_known=0; video_ui_query=0; video_ui_enter=1; video_resume_fallback=1; video_ui_pending=0;
        } else if(video_ui_pending==cmdMP4_PAUSE && video_ui_enter && active) {
            /* A missing exit-pause reply must not cancel a new entry. */
            video_ui_pending=0; video_ui_known=0;
        } else {
            if(video_ui_pending==cmdMP4_UPDATEFILE) video_ui_query=3;
            video_ui_pending=0; video_ui_enter=0; video_title_dirty=0;
            VideoVp(VIDEO_UI_STATUS_ADDR,2); VideoFullFailure(); return;
        }
    }
    if(video_ui_enter && !video_ui_pending && !R11VideoWakeBlocked()) {
        if(!video_ui_query) {
            query[0]=r11_player.store_type=UDISK; query[1]=MP4;
            T5lSendUartDataToR11(cmdMP4_UPDATEFILE,query);
            video_ui_query=1; video_ui_pending=cmdMP4_UPDATEFILE; video_ui_sent=VideoTime();
            VideoVp(VIDEO_UI_STATUS_ADDR,1);
        } else if(video_ui_query!=2) {
            /* Do not mistake cached names for the outstanding query's reply. */
        } else if(count) {
            video_ui_enter=0;
            if(video_ui_known && r11_player.now_play_serial<count && r11_player.state==4) VideoSend(cmdMP4_REPLAY,0);
            else if(video_ui_known && r11_player.now_play_serial<count && r11_player.state==1) VideoVp(VIDEO_UI_STATUS_ADDR,0);
            else R11VideoValueHandle(keyMP4_1FILE);
        } else { video_ui_enter=0; VideoVp(VIDEO_UI_STATUS_ADDR,3); VideoFullFailure(); return; }
    }
    if(active && !video_ui_enter && !video_ui_pending && video_next_due && count) {
        video_next_due=0;
        VideoPlayFileKey(keyMP4_1FILE+(video_ui_known?(r11_player.now_play_serial+1)%count:0));
    }
    if(active && video_ui_known && r11_player.state==1 && !video_ui_enter && !video_ui_pending && !video_loop_sent) {
        /* Configure only after real filename-play confirmation. More than five
         * files use the bounded local loop rather than R11's unbounded ALL. */
        query[0]=r11_player.total_serial>VIDEO_UI_MAX_FILES?0:1;
        query[1]=query[0]?2:0;
        T5lSendUartDataToR11(cmdMP4_LOOP_MODE_SET,query);
        video_loop_sent=1;
    }
    if(active && video_ui_known && !video_ui_enter && !video_ui_pending && video_title_dirty && count) {
        if(r11_player.now_play_serial<count) write_dgus_vp(MP4_NOW_PLAY_NAME_ADDR,mp4_name[r11_player.now_play_serial],MAX_MP3_NAME_LEN>>1);
        video_title_dirty=0;
    }
}
static void VideoFilesReported(uint8_t cmd)
{
    /* The R11 0x61 refresh returns its first list with opcode 0x63.
     * The receive handler validates the first-list index before calling us. */
    if((cmd==cmdMP4_UPDATEFILE || cmd==cmdMP4_PREVFILE) &&
       (ReadPageId()==120 || ReadPageId()==121) && r11_player.store_type==UDISK &&
       (video_ui_pending==cmdMP4_UPDATEFILE || (!video_ui_pending && video_ui_query==3))) {
        /* A delayed UDISK scan still completes this entry, but never revives
         * playback after the user has left the player. */
        video_ui_pending=0; video_ui_query=2; video_ui_enter=1;
        VideoVp(VIDEO_UI_STATUS_ADDR,VideoCount()?1:3);
    }
    video_title_dirty=1;
    if(!VideoCount() && ReadPageId()==121) { video_ui_enter=0; VideoFullFailure(); }
}
static void VideoLoopReported(uint8_t update,uint8_t mode)
{
    uint16_t value=((uint16_t)update<<8)|mode;
    /* Unsolicited cloud-mode notification, not an ACK to our loop command.
     * It must not complete or fail a pending play/pause/list request. */
    VideoVp(LOOP_MODE_ADDR,value);
}
static void VideoStateReported(uint8_t state,uint8_t reply_cmd)
{
    uint8_t old=r11_player.state;
    r11_player.state=state; VideoVp(PLAY_STATUS_ADDR,state);
    if(video_ui_pending==cmdMP4_PLAY && reply_cmd==cmdMP4_PLAY && state==1) {
        /* R11 has confirmed filename playback, so the requested slot is now current. */
        r11_player.now_play_serial=video_ui_target; video_ui_known=1; video_title_dirty=1;
        VideoVp(NP4_NOW_NUM_ADDR,video_ui_target);
        video_ui_pending=0; VideoVp(VIDEO_UI_STATUS_ADDR,0);
    } else if((video_ui_pending==cmdMP4_PAUSE && state==4) || (video_ui_pending==cmdMP4_REPLAY && state==1)) {
        video_ui_pending=0; VideoVp(VIDEO_UI_STATUS_ADDR,0);
    }
    if(state==1 && ReadPageId()!=120 && ReadPageId()!=121 && video_ui_pending!=cmdMP4_PAUSE) VideoSend(cmdMP4_PAUSE,0);
    if(old==1 && state==3 && !video_ui_pending && video_loop_sent &&
       r11_player.total_serial>VIDEO_UI_MAX_FILES && (ReadPageId()==120 || ReadPageId()==121)) video_next_due=1;
}
static void VideoSerialReported(uint16_t serial)
{
    if(serial>=VIDEO_UI_MAX_FILES || serial>=VideoCount()) {
        video_ui_known=0; video_next_due=1; VideoVp(VIDEO_UI_STATUS_ADDR,2); return;
    }
    r11_player.now_play_serial=serial; video_ui_known=1; video_title_dirty=1; VideoVp(NP4_NOW_NUM_ADDR,serial);
}
static void VideoMediaChanged(void)
{
    uint8_t empty[MAX_MP3_NAME_LEN]; memset(empty,0,sizeof(empty));
    write_dgus_vp(MP4_NOW_PLAY_NAME_ADDR,empty,MAX_MP3_NAME_LEN>>1);
    video_ui_known=0; r11_player.total_serial=r11_player.page_mp4_nums=0; r11_player.state=0;
    r11_player.store_type=UDISK;
    video_ui_pending=0; video_ui_query=0; video_title_dirty=1; video_next_due=0;
    video_loop_sent=0;
    video_ui_enter=(ReadPageId()==120 || ReadPageId()==121);
    VideoVp(PLAY_STATUS_ADDR,0); VideoVp(VIDEO_UI_STATUS_ADDR,2);
}
static uint8_t VideoUiKey(uint16_t key)
{
    uint16_t count=VideoCount(),next,touch;
    uint8_t filekey=(key>=keyMP4_1FILE && key<=keyMP4_5FILE);
    if(key!=keyMP4_PREVIOUS_VIDEO && key!=keyMP4_NEXT_VIDEO && key!=keyMP4_TOGGLE_PAUSE &&
       key!=keyMP4_IMG_SET_BIG && key!=keyMP4_IMG_SET_SMALL && key!=keyMP4_PAUSE && key!=keyMP4_REPLAY && !filekey) return 0;
    if(R11VideoWakeBlocked()) return 1;
    if(key==keyMP4_IMG_SET_SMALL) {
        if(R11VideoIdleActive()) {
            /* The R11 scan may run just before the 10 ms touch task. */
            touch=VideoGet(sysDGUS_TP_STATUS);
            if((touch>>8)==0x5a && ((uint8_t)touch==1 || (uint8_t)touch==3)) video_touch_down=1;
            video_wake_pending=1; if(!video_touch_down) VideoIdleDismiss();
        }
        else VideoFullFailure();
        return 1;
    }
    if(ReadPageId()!=120 && ReadPageId()!=121) return 1;
    if(key==keyMP4_IMG_SET_BIG) {
        if(count && ReadPageId()==120) {
            video_full_origin=120; video_idle_active=video_idle_entering=0;
            SwitchPageById(121);
        }
        return 1;
    }
    if(video_ui_pending || video_ui_enter) return 1;
    if(!count) { VideoVp(VIDEO_UI_STATUS_ADDR,3); return 1; }
    if(filekey) VideoPlayFileKey(key);
    else if(key==keyMP4_PREVIOUS_VIDEO || key==keyMP4_NEXT_VIDEO) {
        if(count<2 || !video_ui_known || r11_player.now_play_serial>=count) return 1;
        next=key==keyMP4_NEXT_VIDEO?(r11_player.now_play_serial+1)%count:(r11_player.now_play_serial?r11_player.now_play_serial-1:count-1);
        VideoPlayFileKey(keyMP4_1FILE+next);
    } else if(key==keyMP4_TOGGLE_PAUSE) {
        if(r11_player.state==1) VideoSend(cmdMP4_PAUSE,0);
        else if(r11_player.state==4) VideoSend(cmdMP4_REPLAY,0);
        else { video_ui_enter=1; video_ui_query=0; }
    } else VideoSend(key==keyMP4_PAUSE?cmdMP4_PAUSE:cmdMP4_REPLAY,0);
    return 1;
}

void R11VideoValueHandle(uint16_t dgus_value)
{
    #define MAX_MP4_NUMBER        256
    uint8_t r11_send_buf[6];
    if(R11VideoWakeBlocked()) return;
    if(VideoUiKey(dgus_value)) return;
#if sysADVERTISE_MODE_ENABLED
    /* This screen uses only first-page filename keys 0x21..0x25. */
    if(dgus_value>=keyMP4_PLAY_BY_NUM && dgus_value<keyMP4_PLAY_BY_NUM+MAX_MP4_NUMBER) {
        if(dgus_value-keyMP4_PLAY_BY_NUM<VIDEO_UI_MAX_FILES)
            VideoUiKey(keyMP4_1FILE+dgus_value-keyMP4_PLAY_BY_NUM);
        return;
    }
    if(dgus_value==keyMP4_NEXTFILE || dgus_value==keyMP4_PREVFILE) return;
#endif

    if(dgus_value >= keyMP4_PLAY_BY_NUM && dgus_value < keyMP4_PLAY_BY_NUM + MAX_MP4_NUMBER)
    {
        if(r11_player.total_serial > 0)
        {
            r11_player.serial = dgus_value - keyMP4_PLAY_BY_NUM;
            if(r11_player.serial < r11_player.total_serial)
            {
                write_dgus_vp(NP4_NOW_NUM_ADDR,(uint8_t*)&r11_player.serial,1);
                r11_send_buf[0] = MP4;
                r11_send_buf[1] = 0x00;
                r11_send_buf[2] = r11_player.serial;
                T5lSendUartDataToR11(cmdMP4_PLAY_BY_NUM, r11_send_buf);
            }
        }
    }else if(dgus_value == keyMP4_REPLAY)
    {
        r11_send_buf[0] = 0x00;
        T5lSendUartDataToR11(cmdMP4_REPLAY, r11_send_buf);
    }else if(dgus_value == keyMP4_STOP)
    {
        /*将当前标白，去掉高亮显示*/
        write_dgus_vp(MP4_FILENAME_SP_LIST1 + (r11_player.now_play_serial % 5) * 0x20 + 0x03,(uint8_t*)&color_white,1);
        r11_send_buf[0] = 0x00;
        T5lSendUartDataToR11(cmdMP4_STOP, r11_send_buf);
    }else if(dgus_value == keyMP4_EXIT)
    {
        /*将当前标白，去掉高亮显示*/
        write_dgus_vp(MP4_FILENAME_SP_LIST1 + (r11_player.now_play_serial % 5) * 0x20 + 0x03,(uint8_t*)&color_white,1);
        r11_send_buf[0] = 0x00;
        T5lSendUartDataToR11(cmdMP4_STOP, r11_send_buf);
        /** 退出循环播放模式 */
        r11_send_buf[0] = 0x00;
        r11_send_buf[1] = 0x00;
        T5lSendUartDataToR11(cmdMP4_LOOP_MODE_SET, r11_send_buf);
        write_dgus_vp(LOOP_MODE_ADDR, (uint8_t *)&r11_send_buf[0], 0x01);
        DgusToFlash(flashMAIN_BLOCK_ORDER, LOOP_MODE_ADDR, LOOP_MODE_ADDR, 0x02);
    }else if(dgus_value == keyMP4_PAUSE)
    {
        r11_send_buf[0] = 0x00;
        T5lSendUartDataToR11(cmdMP4_PAUSE, r11_send_buf);
    }else if(dgus_value == keyMP4_AUX_CLOSE)
    {
        if(r11_player.r11_volunme > 0)
        {
            r11_player.r11_volunme_Bak = r11_player.r11_volunme;
            r11_player.r11_volunme = 0;
            T5lSendUartDataToR11(cmdMP4_AUX_SET, &r11_player.r11_volunme);
        }else{
            r11_player.r11_volunme = r11_player.r11_volunme_Bak;
            T5lSendUartDataToR11(cmdMP4_AUX_SET, &r11_player.r11_volunme);
        }
        r11_send_buf[0] = 0x00;
        r11_send_buf[1] = r11_player.r11_volunme;
        write_dgus_vp(VOLUME_SET_ADDR, (uint8_t *)&r11_send_buf[0], 0x01);
        DgusToFlash(flashMAIN_BLOCK_ORDER, VOLUME_SET_ADDR, VOLUME_SET_ADDR, 0x02);
    }else if(dgus_value == keyMP4_AUX_ADD)
    {
        if(r11_player.r11_volunme < MAX_VOLUME)
        {
            r11_player.r11_volunme++;
            T5lSendUartDataToR11(cmdMP4_AUX_SET, &r11_player.r11_volunme);
            r11_send_buf[0] = 0x00;
            r11_send_buf[1] = r11_player.r11_volunme;
            write_dgus_vp(VOLUME_SET_ADDR, (uint8_t *)&r11_send_buf[0], 0x01);
            DgusToFlash(flashMAIN_BLOCK_ORDER, VOLUME_SET_ADDR, VOLUME_SET_ADDR, 0x02);
        }
    }else if(dgus_value == keyMP4_AUX_SUB)
    {
        if(r11_player.r11_volunme > 0)
        {
            r11_player.r11_volunme--;
            T5lSendUartDataToR11(cmdMP4_AUX_SET, &r11_player.r11_volunme);
            r11_send_buf[0] = 0x00;
            r11_send_buf[1] = r11_player.r11_volunme;
            write_dgus_vp(VOLUME_SET_ADDR, (uint8_t *)&r11_send_buf[0], 0x01);
            DgusToFlash(flashMAIN_BLOCK_ORDER, VOLUME_SET_ADDR, VOLUME_SET_ADDR, 0x02);
        }
    }else if(dgus_value == keyMP4_AUX_SET)
    {
        read_dgus_vp(VOLUME_SET_ADDR, (uint8_t *)&r11_send_buf[0], 1);
        if(r11_send_buf[1] > MAX_VOLUME)
        {
            r11_send_buf[1] = MAX_VOLUME;
        }
        r11_player.r11_volunme = r11_send_buf[1];
        T5lSendUartDataToR11(cmdMP4_AUX_SET, &r11_player.r11_volunme);
        write_dgus_vp(VOLUME_SET_ADDR, (uint8_t *)&r11_send_buf[0], 0x01);
        DgusToFlash(flashMAIN_BLOCK_ORDER, VOLUME_SET_ADDR, VOLUME_SET_ADDR, 0x02);
    }else if(dgus_value == keyMP4_1FILE || dgus_value == keyMP4_2FILE || dgus_value == keyMP4_3FILE || dgus_value == keyMP4_4FILE || dgus_value == keyMP4_5FILE)
    {
        r11_send_buf[0] = (uint8_t)(dgus_value - keyMP4_1FILE);
        /*先将当前播放的标白，之后将当前播放改为点击的，再将点击的标红*/
        write_dgus_vp(MP4_FILENAME_SP_LIST1 + (r11_player.now_play_serial % 5) * 0x20 + 0x03,(uint8_t*)&color_white,1);
        r11_player.now_play_serial = r11_send_buf[0] + (r11_player.now_play_page-1)*5;
        write_dgus_vp(MP4_FILENAME_SP_LIST1 + (r11_player.now_play_serial % 5) * 0x20 + 0x03,(uint8_t*)&color_red,1);
        if (r11_send_buf[0] < r11_player.page_mp4_nums) {
            r11_player.serial = r11_send_buf[0];
            write_dgus_vp(MP4_NOW_PLAY_NAME_ADDR,(uint8_t*)&mp4_name[r11_player.serial][0], MAX_MP3_NAME_LEN>>1);
            T5lSendUartDataToR11(cmdMP4_PLAY, mp4_name[r11_player.serial]);
        }
    }else if(dgus_value == keyMP4_UPDATE_SDCARD || dgus_value == keyMP4_UPDATE_exUDISK || dgus_value == keyMP4_UPDATE_UDISK)
    {
        if(dgus_value == keyMP4_UPDATE_SDCARD)
        {
            r11_player.store_type = SDCARD;
        }else if(dgus_value == keyMP4_UPDATE_exUDISK)
        {
            r11_player.store_type = exUDISK;
        }else if(dgus_value == keyMP4_UPDATE_UDISK)
        {
            r11_player.store_type = UDISK;
        }else{
            r11_player.store_type = SDCARD;
        }

        R11ChangePictureLocate(mainview.video_x_point,mainview.video_y_point,mainview.video_high,mainview.video_weight,0x00);
#if sysADVERTISE_MODE_ENABLED
        r11_player.store_type=UDISK;
#endif
        r11_send_buf[0] = r11_player.store_type;
        r11_send_buf[1] = r11_player.Document_type = MP4;
        T5lSendUartDataToR11(cmdMP4_UPDATEFILE, r11_send_buf);
        memset((uint8_t *)mp4_name, 0, sizeof(mp4_name));
    }else if(dgus_value == keyMP4_NEXTFILE)
    {
        if(r11_player.now_play_page < (r11_player.total_serial + 4) / 5)
        {
            r11_player.now_play_page++;
            /*如果当前播放的视频在这一页，就标红，如果不在，就标白*/
            if(r11_player.now_play_serial >= (r11_player.now_play_page-1) * 5 && r11_player.now_play_serial < r11_player.now_play_page*5)
            {
                write_dgus_vp(MP4_FILENAME_SP_LIST1 + (r11_player.now_play_serial % 5) * 0x20 + 0x03,(uint8_t*)&color_red,1);
            }else
            {
                write_dgus_vp(MP4_FILENAME_SP_LIST1 + (r11_player.now_play_serial % 5) * 0x20 + 0x03,(uint8_t*)&color_white,1);
            }
        }
        r11_send_buf[0] = 0x00;
        T5lSendUartDataToR11(cmdMP4_NEXTFILE, r11_send_buf);
        memset((uint8_t *)mp4_name, 0, sizeof(mp4_name));
    }else if(dgus_value == keyMP4_PREVFILE)
    {
        if(r11_player.now_play_page > 1)
        {
            r11_player.now_play_page--;
            /*如果当前播放的视频在这一页，就标红，如果不在，就标白*/
            if(r11_player.now_play_serial >= (r11_player.now_play_page-1) * 5 && r11_player.now_play_serial < r11_player.now_play_page*5)
            {
                write_dgus_vp(MP4_FILENAME_SP_LIST1 + (r11_player.now_play_serial % 5) * 0x20 + 0x03,(uint8_t*)&color_red,1);
            }else
            {
                write_dgus_vp(MP4_FILENAME_SP_LIST1 + (r11_player.now_play_serial % 5) * 0x20 + 0x03,(uint8_t*)&color_white,1);
            }
        }
        r11_send_buf[0] = 0x00;
        T5lSendUartDataToR11(cmdMP4_PREVFILE, r11_send_buf);
        memset((uint8_t *)mp4_name, 0, sizeof(mp4_name));
        
    }else if(dgus_value == keyMP4_IMG_SET_BIG)
    {
        read_dgus_vp(sysDGUS_SYSTEM_CONFIG, (uint8_t *)&r11_send_buf[0], 1);
        if((r11_send_buf[1] & 0x03) == 0x00 || (r11_send_buf[1] & 0x03) == 0x02)
        {
            #if sysBEAUTY_MODE_ENABLED
            R11ChangePictureLocate((pixels_arr_h2[screen_opt.screen_ratio]-pixels_arr_h[screen_opt.screen_ratio])/2,
            (pixels_arr_l2[screen_opt.screen_ratio]-pixels_arr_l[screen_opt.screen_ratio])/2,
            pixels_arr_h[screen_opt.screen_ratio],
            pixels_arr_l[screen_opt.screen_ratio],0x01);
            #endif /* sysBEAUTY_MODE_ENABLED */
            #if sysADVERTISE_MODE_ENABLED
            R11ChangePictureLocate(0,0,pixels_arr_h2[screen_opt.screen_ratio],pixels_arr_l2[screen_opt.screen_ratio],0x01);
            #endif /* sysADVERTISE_MODE_ENABLED */
        }else if((r11_send_buf[1] & 0x03) == 0x01 || (r11_send_buf[1] & 0x03) == 0x03)
        {
            #if sysBEAUTY_MODE_ENABLED
            R11ChangePictureLocate((pixels_arr_l2[screen_opt.screen_ratio]-pixels_arr_l[screen_opt.screen_ratio])/2,
            (pixels_arr_h2[screen_opt.screen_ratio]-pixels_arr_h[screen_opt.screen_ratio])/2,
            pixels_arr_l[screen_opt.screen_ratio],
            pixels_arr_h[screen_opt.screen_ratio],0x01);
            #endif /* sysBEAUTY_MODE_ENABLED */
            #if sysADVERTISE_MODE_ENABLED
            R11ChangePictureLocate(0,0,pixels_arr_l2[screen_opt.screen_ratio],pixels_arr_h2[screen_opt.screen_ratio],0x01);
            #endif /* sysADVERTISE_MODE_ENABLED */
        }
        R11ClearPicture(0);
        Big_Small_Flag = 0x01;
        r11_send_buf[0] = 0x00;
        r11_send_buf[1] = 0x01;
        write_dgus_vp(BIG_SMALL_FLAG_ADDR,(uint8_t*)&r11_send_buf[0], 1);
        DgusToFlash(flashMAIN_BLOCK_ORDER, BIG_SMALL_FLAG_ADDR-1, BIG_SMALL_FLAG_ADDR-1, 0x02);
    }else if(dgus_value == keyMP4_IMG_SET_SMALL)
    {
        R11ChangePictureLocate(mainview.video_x_point,mainview.video_y_point,mainview.video_high,mainview.video_weight,0x00);
        R11ClearPicture(0);
        Big_Small_Flag = 0x00;
        r11_send_buf[0] = 0x00;
        r11_send_buf[1] = 0x00;
        write_dgus_vp(BIG_SMALL_FLAG_ADDR,(uint8_t*)&r11_send_buf[0], 1);
        DgusToFlash(flashMAIN_BLOCK_ORDER, BIG_SMALL_FLAG_ADDR-1, BIG_SMALL_FLAG_ADDR-1, 0x02);
    }else if(dgus_value == keyMP4_ROTATE_ANGLE)
    {
        if(r11_player.rotate == 0)
        {
            r11_send_buf[0] = 0x00;
            r11_send_buf[1] = 90;
            r11_player.rotate = 1;
        }else if(r11_player.rotate == 1)
        {
            r11_send_buf[0] = 0x00;
            r11_send_buf[1] = 180;
            r11_player.rotate = 2;
        }else if(r11_player.rotate == 2)
        {
            r11_send_buf[0] = 0x01;
            r11_send_buf[1] = 0x0e;
            r11_player.rotate = 3;
        }else if(r11_player.rotate == 3)
        {
            r11_send_buf[0] = 0;
            r11_send_buf[1] = 0;
            r11_player.rotate = 0;
        }
        T5lSendUartDataToR11(cmdMP4_ROTATE_ANGLE, r11_send_buf);
    }else if(dgus_value == keyMP4_NONE_LOOP_MODE)
    {
        r11_send_buf[0] = 0x00;
        r11_send_buf[1] = 0x00;
        T5lSendUartDataToR11(cmdMP4_LOOP_MODE_SET, r11_send_buf);
        write_dgus_vp(LOOP_MODE_ADDR, (uint8_t *)&r11_send_buf[0], 0x01);
        DgusToFlash(flashMAIN_BLOCK_ORDER, LOOP_MODE_ADDR, LOOP_MODE_ADDR, 0x02);
    }else if(dgus_value == keyMP4_SINGLE_LOOP_MODE)
    {
        r11_send_buf[0] = 0x01;
        r11_send_buf[1] = 0x01;
        T5lSendUartDataToR11(cmdMP4_LOOP_MODE_SET, r11_send_buf);
        write_dgus_vp(LOOP_MODE_ADDR, (uint8_t *)&r11_send_buf[0], 0x01);
        DgusToFlash(flashMAIN_BLOCK_ORDER, LOOP_MODE_ADDR, LOOP_MODE_ADDR, 0x02);
    }else if(dgus_value == keyMP4_ALL_LOOP_MODE)
    {
        r11_send_buf[0] = 0x01;
        r11_send_buf[1] = 0x02;
        T5lSendUartDataToR11(cmdMP4_LOOP_MODE_SET, r11_send_buf);
        write_dgus_vp(LOOP_MODE_ADDR, (uint8_t *)&r11_send_buf[0], 0x01);
        DgusToFlash(flashMAIN_BLOCK_ORDER, LOOP_MODE_ADDR, LOOP_MODE_ADDR, 0x02);
    }else if(dgus_value == keyMP4_CLEAR_PAGE)
    {
        R11ClearPicture(0);
    }
}


#if R11_WIFI_ENABLED
/*
 * @brief WiFi扫描结果处理。
 */
static void R11WifiScanResultHandle(uint8_t *frame)
{
	uint16_t write_param[4],i,j,zero_arr[32] = 0;
	if(frame[6] == 0x0a)
	{
		if(wifi_now_offset > 0)
		{
			wifi_now_offset--;
		}
		if(wifi_page.scan_flag == 0x5a)
		{
			SwitchPageById((uint16_t)wifi_page.scan_page); 
		}
	}else
	{
		write_param[0] = (frame[2]<<8|frame[3]) + 4;
		write_param[2] = 0;
		for ( i=6,j=6; i<write_param[0]; i++ )
		{
			//每次间隔两个0x0a
			if ( frame[i] == '\n' )
			{
				write_param[1] = i-j;
				if ( write_param[1]>32 )
				{
					write_param[1] = 32;
					write_dgus_vp ( wifi_page.wifi_start_addr+write_param[2]*0x10,&frame[j],write_param[1]/2 );
				}
				else
				{
					if ( write_param[1] %2 !=0 )
					{
						frame[i] = 0x00;
						write_dgus_vp ( wifi_page.wifi_start_addr+write_param[2]*0x10,&frame[j], ( write_param[1]+1 ) /2 );
						write_dgus_vp ( wifi_page.wifi_start_addr+write_param[2]*0x10+ ( write_param[1]+1 ) /2, ( uint8_t * ) zero_arr, ( MAX_WIFI_NAME_LEN-write_param[1]-1 ) /2 );
					}
					else
					{
						write_dgus_vp ( wifi_page.wifi_start_addr+write_param[2]*0x10,&frame[j],write_param[1]/2 );
						write_dgus_vp ( wifi_page.wifi_start_addr+write_param[2]*0x10+write_param[1]/2, ( uint8_t * ) zero_arr, ( MAX_WIFI_NAME_LEN-write_param[1] ) /2 );
					}
				}
				write_param[2]++;

				i=i+2;
				j=i;
			}
			if ( write_param[2]>=5 )
			{
				break;
			}
		}
		if ( write_param[2]<5 )
		{
			for(i=write_param[2];i<5;i++)
			{
				write_dgus_vp ( wifi_page.wifi_start_addr+i*0x10, ( uint8_t * ) zero_arr, MAX_WIFI_NAME_LEN/2 );
			}
		}
		if(wifi_page.scan_flag == 0x5a)
		{
			SwitchPageById((uint16_t)wifi_page.scan_page); 
		}
	}
}


/*
 * @brief 连接WiFi，组装并发送WiFi连接协议帧。
 */
static void R11ConnectWifi(void)
{
    uint8_t now_len = 0,now_len2;
	uint8_t r11_send_buf[80],read_param[32];

    r11_send_buf[0] = 0xaa;
    r11_send_buf[1] = 0x55;
    r11_send_buf[2] = 0x00;
    r11_send_buf[3] = 0x02;
    r11_send_buf[4] = cmdWIFI_CONNECT;
    r11_send_buf[5] = 0x01;

    now_len = 8;
    read_dgus_vp(WIFI_SSID_ADDR, read_param, 16);
    now_len = CopyAsciiString(r11_send_buf, read_param, now_len);
    r11_send_buf[6] = 0x00;
    r11_send_buf[7] = now_len - 8;

    read_dgus_vp(WIFI_PASSWD_ADDR, read_param, 16);
    now_len2 = CopyAsciiString(r11_send_buf, read_param, now_len+2);
    r11_send_buf[now_len] = 0x00;
    r11_send_buf[now_len+1] = now_len2 - now_len - 2;

    r11_send_buf[3] = now_len2 - 4;
    UartSendData(&Uart_R11,r11_send_buf,now_len2 + 4);
}


/*
 * @brief 扫描WiFi，组装并发送WiFi扫描协议帧。
 */
static void R11ScanWifi(uint8_t scan_offset)
{
    uint8_t r11_send_buf[7];
    r11_send_buf[0]= 0xaa;
    r11_send_buf[1]= 0x55;
    r11_send_buf[2]= 0x00;
    r11_send_buf[3]= 0x03;
    r11_send_buf[4]= cmdWIFI_SCAN;
    r11_send_buf[5]= scan_offset*5;
    r11_send_buf[6]= 0x05;
    UartSendData(&Uart_R11,r11_send_buf,7);
}


void UartR11UserWifiProtocol(UART_TYPE *uart,uint8_t *frame, uint16_t len)
{
    uint16_t write_param[2] = 0;
    if(uart!=&Uart_R11 || len<6) return;
    if(frame[0] == 0xAA && frame[1] == 0x55)
    {
        if(len < 6 || len < ((frame[2]<<8|frame[3])+4))
        {
            return;
        }
        switch (frame[4])
        {
			case cmdWIFI_SCAN:
				if(frame[5] != R11_RECV_OK)
				{
					if(wifi_page.scan_flag == 0x5a)
					{
						SwitchPageById((uint16_t)wifi_page.scan_page); 
					}
					return;
				}else
				{
					R11WifiScanResultHandle(frame);
				}
				break;
			case cmdWIFI_CONNECT:
				if(frame[5] == R11_RECV_OK)
				{
                    #if sysBEAUTY_MODE_ENABLED
					net_connected_state = NET_CPUINFO_QUERY;
                    #endif /* sysBEAUTY_MODE_ENABLED */
                    #if sysADVERTISE_MODE_ENABLED
                    net_connected_state = NET_DISCONNECTED;
                    #endif /* sysADVERTISE_MODE_ENABLED */
				}
				break;
        }
    }
}


void R11WifiValueHandle(uint16_t dgus_value)
{
    uint8_t r11_send_buf[15];
    uint16_t read_param[20];
    uint16_t write_zero_param[80]=0;
    const uint16_t uint16_port_zero = 0;
    if(dgus_value == keyWIFI_LIST1 || dgus_value == keyWIFI_LIST2 || dgus_value == keyWIFI_LIST3 
        || dgus_value == keyWIFI_LIST4 || dgus_value == keyWIFI_LIST5)
    {
        read_dgus_vp ( wifi_page.wifi_start_addr+ ( dgus_value-keyWIFI_LIST1 ) *0x10, ( uint8_t * ) read_param,MAX_WIFI_NAME_LEN/2 );
        if ( read_param[0] !=0x0000 && read_param[0] !=0xffff )
        {
            write_dgus_vp ( 0x4b0, ( uint8_t * ) read_param,MAX_WIFI_NAME_LEN/2 );
        }
        write_dgus_vp ( wifi_page.wifi_start_addr, ( uint8_t * ) write_zero_param,(MAX_WIFI_NAME_LEN/2)*5 ); 
        if(wifi_page.detail_flag == 0x5a)
        {
            SwitchPageById((uint16_t)wifi_page.detail_page); 
        }
    }else if(dgus_value == keyWIFI_CONNECT)
    {
        R11ConnectWifi();
        if(wifi_page.tr_detail_flag == 0x5a)
        {
            SwitchPageById((uint16_t)wifi_page.tr_detail_page); 
        }
    }
    else if(dgus_value == keyWIFI_DISCONNECT)
    {
        __NOP();
    }else if(dgus_value == keyWIFI_SCAN)
    {
        if(wifi_page.tr_scan_flag == 0x5a)
        {
            SwitchPageById((uint16_t)wifi_page.tr_scan_page); 
        }
        wifi_now_offset = 0;
        R11ScanWifi(wifi_now_offset);
    }else if(dgus_value == keyWIFI_IP_QUERY)
    {
        r11_send_buf[0] = 0xaa;
        r11_send_buf[1] = 0x55;
        r11_send_buf[2] = 0x00;
        r11_send_buf[3] = 0x08;
        r11_send_buf[4] = 0xc2;
        r11_send_buf[5] = 0x00;
        r11_send_buf[6] = 0x05;
        memcpy(&r11_send_buf[7], "wlan0", 6); 
        UartSendData(&Uart_R11, r11_send_buf, 13);
    }else if(dgus_value == keyWIFI_IP_RETURN)
    {
        if(page_st.menu_flag == 0x5a)
        {
            SwitchPageById((uint16_t)page_st.menu_page); 
        }
    }else if(dgus_value == keyWIFI_NEXT_LIST)
    {
        if(wifi_page.tr_scan_flag == 0x5a)
        {
            SwitchPageById((uint16_t)wifi_page.tr_scan_page); 
        }
        wifi_now_offset++;
        R11ScanWifi(wifi_now_offset);
    }else if(dgus_value == keyWIFI_PREV_LIST)
    {
        if(wifi_page.tr_scan_flag == 0x5a)
        {
            SwitchPageById((uint16_t)wifi_page.tr_scan_page); 
        }
        if(wifi_now_offset > 0)
        {
            wifi_now_offset--;
        }
        R11ScanWifi(wifi_now_offset);
    }else if(dgus_value == keyCHECK_STATUS_WIFI || dgus_value == keyCHECK_STATUS_ETH0 || dgus_value == keyCHECK_STATUS_4G)
    {
        r11_send_buf[0] = 0xaa;
        r11_send_buf[1] = 0x55;
        r11_send_buf[2] = 0x00;
        r11_send_buf[3] = 0x02;
        r11_send_buf[4] = cmdCHECK_STATUS_NET;
        r11_send_buf[5] = (uint8_t)(dgus_value - keyCHECK_STATUS_WIFI + 1);
        T5lSendUartDataToR11(cmdCHECK_STATUS_NET, r11_send_buf);
    }else if(dgus_value == keyCHECK_STATUS_exUDISK || dgus_value == keyCHECK_STATUS_SDCARD)
    {
        r11_send_buf[0] = 0xaa;
        r11_send_buf[1] = 0x55;
        r11_send_buf[2] = 0x00;
        r11_send_buf[3] = 0x02;
        r11_send_buf[4] = cmdCHECK_STATUS_DEVICE;
        r11_send_buf[5] = (uint8_t)(dgus_value - keyCHECK_STATUS_exUDISK + 1);
        T5lSendUartDataToR11(cmdCHECK_STATUS_DEVICE, r11_send_buf);
    }
}
#endif /** R11_WIFI_ENABLED */


/**
 * @brief 从串口协议中提取以双0x23字符分隔的文件名
 * @param frame 协议帧数据
 * @param len 协议帧长度
 * @note 协议格式：AA 55 ... 23 23 filename1 23 23 filename2 23 23 filename3 ...
 *  eg.AA 55 00 28 63 00 06 00 00 23 23 31 2E 6D 70 34 23 23 32 2E 6D 70 34 23 23 33 2E 6D 70 34 23 23 34 2E 6D 70 34 23 23 35 2E 6D 70 34
 *       每个文件名之间用两个0x23字符（##）分隔
 */
static void ExtractFilenamesFromProtocol(uint8_t *frame, uint16_t len)
{
    uint16_t i;
    uint8_t separator_count = 0;  /* 连续0x23的计数器 */
    uint8_t current_file_index = 0;  /* 当前文件索引 */
    
    /* 清空文件名数组和长度数组 */
    memset((uint8_t *)mp4_name, 0, sizeof(mp4_name));
    memset((uint8_t *)mp4_name_len, 0, sizeof(mp4_name_len));
    r11_player.page_mp4_nums = 0;
    r11_player.total_serial = frame[5]<<8 | frame[6];
    
    /* 从协议数据部分开始解析（跳过协议头） */
    for(i = 9; i < len && current_file_index < 5; i++)
    {
        if(frame[i] == 0x23)
        {
            separator_count++;
            /* 检测到两个连续的0x23，表示分隔符 */
            if(separator_count == 2)
            {
                /* 如果当前文件名长度大于0，则切换到下一个文件 */
                if(current_file_index<5 && mp4_name_len[current_file_index] > 0)
                {
                    current_file_index++;
                    if(current_file_index >= 5)
                    {
                        break;  /* 超过最大文件数量 */
                    }
                }
                separator_count = 0;  /* 重置分隔符计数器 */
            }
        }
        else
        {
            /* 非0x23字符，重置分隔符计数器 */
            separator_count = 0;
            
            /* 将字符添加到当前文件名 */
            if(mp4_name_len[current_file_index] < MAX_MP3_NAME_LEN - 1)
            {
                mp4_name[current_file_index][mp4_name_len[current_file_index]] = frame[i];
                mp4_name_len[current_file_index]++;
            }
        }
    }
    
    /* 如果最后一个文件名有内容，增加文件计数 */
    if(current_file_index<5 && mp4_name_len[current_file_index] > 0)
    {
        current_file_index++;
    }
    
    /* 设置实际的文件数量 */
    r11_player.page_mp4_nums = current_file_index;
    
    /* 更新显示缓冲区 */
    for(i = 0; i < 5; i++)
    {
        write_dgus_vp(MP4_FILENAME_VP_LIST1 + i * (MAX_MP3_NAME_LEN >> 1), 
                     (uint8_t*)&mp4_name[i][0], 
                     MAX_MP3_NAME_LEN >> 1);
    }
}


void UartR11UserVideoProtocol(UART_TYPE *uart,uint8_t *frame, uint16_t len)
{
    #if !sysADVERTISE_MODE_ENABLED
    uint16_t write_param[10];
    #endif
    if(uart!=&Uart_R11 || len<6) return;
    if(frame[0] == 0xAA && frame[1] == 0x55)
    {
        if(len < 6 || len < ((frame[2]<<8|frame[3])+4))
        {
            return;
        }
        /** @note 广告屏含有crc校验 */
        #if sysADVERTISE_MODE_ENABLED
        if((frame[len-1]<<8 |frame[len-2]) != crc_16(&frame[4], len-6))
        {
            return;
        }else{
            len -= 2;
            if(len<6) return;
        }
        #endif /* sysADVERTISE_MODE_ENABLED */
        switch (frame[4])
        {
        case cmdMP4_UPDATEFILE:
        case cmdMP4_PREVFILE:
        case cmdMP4_NEXTFILE:
            /* 提取以0x23 0x23(##)为分隔符的文件名 */            
            if(len<9) break;
            #if sysADVERTISE_MODE_ENABLED
            /* Only the first five UDISK files belong to this UI. Reject other
             * list pages before they overwrite the filenames used by 0x21. */
            if((frame[4]!=cmdMP4_UPDATEFILE && frame[4]!=cmdMP4_PREVFILE) ||
               frame[7]!=0 || frame[8]!=0 || r11_player.store_type!=UDISK) break;
            #endif
            ExtractFilenamesFromProtocol(frame, len);
            VideoFilesReported(frame[4]);
            break;
        case cmdMP4_PAUSE:
        case cmdMP4_REPLAY:
        case cmdMP4_STOP:
        case cmdMP4_PLAY:     /* 播放状态反馈*/
            if(len<6 || frame[5]>7) break;
            VideoStateReported(frame[5],frame[4]);
            break;
        case cmdMP4_PLAY_BY_NUM:
            if(len>=6 && video_ui_pending==cmdMP4_PLAY_BY_NUM && frame[5]!=1) {
                video_ui_pending=0; VideoVp(VIDEO_UI_STATUS_ADDR,2);
            }
            break;
        case cmdCHECK_STATUS_NET:
            if(len<7) break;
            write_dgus_vp(CHECK_NET_STATUS_ADDR,&frame[5],1);
            break;
        case cmdCHECK_STATUS_DEVICE:
            if(len<7) break;
            write_dgus_vp(CHECK_DEVICE_STATUS_ADDR,&frame[5],1);
            break;
        case cmdMP4_LOOP_MODE_SET:
            if(len<7) break;
            #if sysADVERTISE_MODE_ENABLED
            VideoLoopReported(frame[5],frame[6]);
            #else
            write_param[0]=0;
            /**
             * 处理云端下发的循环播放指令
             * t5l：
             * LOOP_MODE_ADDR高位对应是否发送循环指令：0x00不更新play_json，即不发送，0x01更新playsjson
             * LOOP_MODE_ADDR低位对应循环播放状态：0x00不循环，，0x01单曲循环，0x02全部循环
             * 云端三种状态
             * 1.下载完不播放，对应停止循环，此时t5l存储0x0000，r11发送0x0000
             * 2.播放下载的视频，对应全部循环，此时t5l存储0x0002，r11发送0x0100
             * 3.播放全部视频，对应全部循环，此时t5l存储0x0102，r11发送0x0102
             */
            if(frame[5] == 0x00)    /*下载完不播放，停止循环*/
            {
                write_param[0] = 0x0000;
            }else if(frame[5] == 0x01  )   /*单曲循环*/
            {
                if(frame[6] == 0x00)
                {
                    write_param[0] = 0x0002;
                }else if(frame[6] == 0x02)
                {
                    write_param[0] = 0x0102;
                }
            }
            write_dgus_vp(LOOP_MODE_ADDR, (uint8_t *)&write_param[0], 0x01);
            DgusToFlash(flashMAIN_BLOCK_ORDER, LOOP_MODE_ADDR, LOOP_MODE_ADDR, 0x02);
            /*设置完成后进行一次查询*/
            #if sysBEAUTY_MODE_ENABLED
            r11_player.store_type = SDCARD;
            #elif sysADVERTISE_MODE_ENABLED
            r11_player.store_type = UDISK;
            #else
            r11_player.store_type = SDCARD;
            #endif /* sysADVERTISE_MODE_ENABLED END */
            write_param[0] = r11_player.store_type<<8|MP4;
            T5lSendUartDataToR11(cmdMP4_UPDATEFILE, (uint8_t *)&write_param[0]);
            #endif
            break;
        case cmdMP4_NOW_PLAY_NUMBER:
            VideoSerialReported(frame[5]);
            break;
        case cmdMP4_STORAGE_MEDIUM_STATUS:
            VideoMediaChanged();
            #if STORAGE_DISCONN_PAGE
            SwitchPageById(STORAGE_DISCONN_PAGE);
            #endif /* STORAGE_DISCONN_PAGE */
            break;
        default:
            break;
        }
    }
}


void inter_extern1_1_fun_C ( void ) interrupt 2
{
    uint8_t idata Temp,Index, ADR_H_Bak,ADR_M_Bak,ADR_L_Bak,ADR_INC_Bak,DATA3_Bak,DATA2_Bak,DATA1_Bak,DATA0_Bak,RAMMODE_Bak;
	uint16_t zero_value = 0;
    state = P1;
    if ( RAMMODE )
    {
        while ( APP_EN );
    }
    RAMMODE_Bak = RAMMODE;
    ADR_H_Bak = ADR_H;
    ADR_M_Bak = ADR_M;
    ADR_L_Bak = ADR_L;
    ADR_INC_Bak = ADR_INC;
    DATA3_Bak = DATA3;
    DATA2_Bak = DATA2;
    DATA1_Bak = DATA1;
    DATA0_Bak = DATA0;
    RAMMODE = 0x00;
    EX1_Len++;
    Temp = state & 0xE0;
    /** debug
    * Display_Debug_Message();
    */
   Display_Debug_Message();
    switch ( Temp )
    {
        case 0x80:
        {
            if ( data_write_f == 0x00 )
            {
                Kind = state & 0x1F;
                switch ( Kind )
                {
                    case 0:
                    case 1:
                    case 2:
                    case 3:
                    case 4:
                    case 10:
                    {
                        Max_16KB_Count = ( Icon_Overlay_SP_VP[1] - Icon_Overlay_SP_VP[0] ) >>13;
                        Icon_Num = ( Pic_Count[0]%2 ) + 1;
                        Index = Icon_Num - 1;
                        JpegLaber_DGUSII_VP = Icon_Overlay_SP_VP[Index];
                        JpegLaber_OS_ADR = JpegLaber_DGUSII_VP >> 1;
                        Init_Jpeg_Parament();
                        data_write_f = 1;
                        EX0_Start();
                        break;
                    }
                    default:
                    {
                        Other_Cmd_Count++;
                        data_write_f = 7;
                        EX0 = 0;
                        EX1 = 0;
                        break;
                    }
                }
            }
            else
            {
                NotDeal_OverBuffer_Count++;
                data_write_f = 4;
                EX0 = 0;
                EX1 = 0;
            }
            break;
        }
        case 0x40:
        {
            if ( data_write_f == 0x01 )
            {
                if ( 1 | ( Packet_Count < 1 ) )
                {
                    Temp = ( state & 0xC0 ) | ( Packet_TotalCount + 1 );
                    if ( Temp == state )
                    {
                        if ( Packet_TotalCount + 1 < Max_16KB_Count )
                        {
                            Packet_Count++;
                            Packet_TotalCount++;
                            if ( Packet_TotalCount & 0x01 )
                            {
                                if ( buf_tail == 0xC000 )
                                {
                                    Judge_Packet_Count();
                                }
                                else
                                {
                                    Miss_Align_16KB_Count++;
                                    data_write_f = 0x03;
                                    EX0 = 0;
                                    EX1 = 0;
                                }
                            }
                            else
                            {
                                if ( buf_tail == 0x8000 )
                                {
                                    Judge_Packet_Count();
                                }
                                else
                                {
                                    Miss_Align_16KB_Count++;
                                    data_write_f = 0x03;
                                    EX0 = 0;
                                    EX1 = 0;
                                }
                            }
                        }
                        else
                        {
                            Over_Max_16KB_Count++;
                            data_write_f = 9;
                            EX0 = 0;
                            EX1 = 0;
                        }
                    }
                    else
                    {
                        Other_Cmd_Count++;
                        data_write_f = 7;
                        EX0 = 0;
                        EX1 = 0;
                    }
                }
                else
                {
                    NotDeal_OverBuffer_Count++;
                    data_write_f = 4;
                    EX0 = 0;
                    EX1 = 0;
                }
            }
            else
            {
                NotDeal_OverBuffer_Count++;
                data_write_f = 4;
                EX0 = 0;
                EX1 = 0;
            }
            break;
        }
        case 0xA0:
        {
            if ( data_write_f == 0x00 )
            {

                Kind = state & 0x1F;
                Icon_Num = 5 + Kind;
                Max_16KB_Count = ( Icon_Overlay_SP_VP[Icon_Num] - Icon_Overlay_SP_VP[Icon_Num-1] ) >>13;
                JpegLaber_DGUSII_VP = Icon_Overlay_SP_VP[Icon_Num-1];
                JpegLaber_OS_ADR = JpegLaber_DGUSII_VP >> 1;
                Index = Icon_Num - 1;
                Init_Jpeg_Parament();
                data_write_f = 1;
                EX0_Start();
            }
            else
            {
                NotDeal_OverBuffer_Count++;
                data_write_f = 4;
                EX0 = 0;
                EX1 = 0;
            }
            break;
        }
        case 0xC0:
        {
            EX0 = 0;
            if ( data_write_f == 0x01 )
            {
                if ( 1 | ( Packet_Count < 1 ) )
                {
                    Temp = ( state & 0xC0 ) | ( Packet_TotalCount + 1 );
                    if ( Temp == state )
                    {
                        if ( Packet_TotalCount <  Max_16KB_Count )
                        {
                            if ( 1 || END_CRC == 0 )
                            {
                                Packet_Count++;
                                Packet_TotalCount++;
                                if ( Packet_TotalCount & 0x01 )
                                {

                                    if ( buf_tail == 0xC000 )
                                    {
                                        data_write_f = 2;
                                        {
                                            Judge_Packet_Count();
                                            if ( END_CRC != 0 )
                                            {
                                                CRC_ERR_Count++;
                                                data_write_f = 8;
                                                EX0 = 0;
                                                EX1 = 0;
                                            }
                                            else
                                            {

                                                //在这里查询一次，如果GUI在忙就等待1ms再次查询
                                                // #define Judge_GUI_BUSY
                                                #ifdef Judge_GUI_BUSY
                                                // 查询 GUI 忙状态
                                                ADR_H = 0x00;
                                                ADR_M = 0x00 >> 9;
                                                ADR_L = 0x15 >> 1;
                                                ADR_INC = 0x00;
                                                RAMMODE = 0xAF;
                                                while ( !APP_ACK );
                                                APP_EN = 1;
                                                while ( APP_EN );
                                                //RAMMODE = 0x00;
                                                while ( DATA1 == 0x00 && DATA0 == 0x01 )
                                                {
                                                    RAMMODE = 0x00;
                                                    delay_us ( 100 );
                                                    RAMMODE = 0xAF;
                                                    while ( !APP_ACK );
                                                    APP_EN = 1;
                                                    while ( APP_EN );
                                                }
                                                #endif
												ADR_H = JpegLaber_DGUSII_VP >> 17;
												ADR_M = JpegLaber_DGUSII_VP >> 9;
												ADR_L = JpegLaber_DGUSII_VP >> 1;
												ADR_INC = 0x00;
												RAMMODE = 0xAF;
												while ( !APP_ACK );
												APP_EN = 1;
												while ( APP_EN );
												RAMMODE = 0x8F;
												DATA1 = DATA2;
												DATA3 = 0x5A;
												DATA2 = 0xA5;
												//DATA1 = 0xFF;
												DATA0 = 0xFE;
												APP_EN = 1;
												while ( APP_EN );
												RAMMODE = 0x00;
                                                
                                                ADR_H = Icon_Overlay_SP[Index] >> 17;
                                                ADR_M = Icon_Overlay_SP[Index] >> 9;
                                                ADR_L = Icon_Overlay_SP[Index] >> 1;
                                                ADR_INC = 0x01;
                                                RAMMODE = 0x8F;
                                                while ( !APP_ACK );
                                                DATA3 = JpegLaber_DGUSII_VP >> 8;
                                                DATA2 = JpegLaber_DGUSII_VP >> 0;
                                                DATA1 = Icon_Overlay_SP_X[Index] >> 8;
                                                DATA0 = Icon_Overlay_SP_X[Index] >> 0;
                                                APP_EN = 1;
                                                while ( APP_EN );
                                                DATA3 = Icon_Overlay_SP_Y[Index] >> 8;
                                                DATA2 = Icon_Overlay_SP_Y[Index] >> 0;
                                                DATA1 = Icon_Overlay_SP_L[Index] >> 8;
                                                DATA0 = Icon_Overlay_SP_L[Index] >> 0;
                                                APP_EN = 1;
                                                while ( APP_EN );
                                                DATA3 = Icon_Overlay_SP_H[Index] >> 8;
                                                DATA2 = Icon_Overlay_SP_H[Index] >> 0;
                                                DATA1 = Icon_Overlay_SP_Mode >> 8;
                                                DATA0 = Icon_Overlay_SP_Mode >> 0;
                                                APP_EN = 1;
                                                while ( APP_EN );
                                                DATA3 = 0x00;
                                                DATA2 = 0x80 | ( JpegLaber_DGUSII_VP >> 16 );
                                                DATA1 = 00;
                                                DATA0 = 00;
                                                APP_EN = 1;
                                                while ( APP_EN );
                                                RAMMODE = 0x00;

               


                                                if ( Icon_Num == 1 ||Icon_Num == 2 )
                                                {
                                                    Pic_Count[ ( Icon_Num-1 ) /2]++;
                                                }
                                                #if sysBEAUTY_MODE_ENABLED
                                                else
                                                {
													
													if(Icon_Num == r11_state.now_choose_pic+5)
													{
														r11_state.pic_capture_flag = 1;
													}
                                                    Pic_Count[ ( Icon_Num-1 )]++;
                                                }
                                                #endif /* sysBEAUTY_MODE_ENABLED */
                                                data_write_f = 0;
                                                EX1_Start();
                                            }
                                            // }
                                        }
                                        //break;
                                    }
                                    else
                                    {
                                        Miss_Align_16KB_Count++;
                                        data_write_f = 0x03;
                                        EX0 = 0;
                                        EX1 = 0;
                                    }
                                }
                                else
                                {
                                    if ( buf_tail == 0x8000 )
                                    {
                                        data_write_f = 2;
                                        {
                                            Judge_Packet_Count();
                                            if ( END_CRC != 0 )
                                            {
                                                CRC_ERR_Count++;
                                                data_write_f = 8;
                                                EX0 = 0;
                                                EX1 = 0;
                                            }
                                            else
                                            {

                                                //在这里查询一次，如果GUI在忙就等待1ms再次查询
                                                // #define Judge_GUI_BUSY
                                                    #ifdef Judge_GUI_BUSY
                                                    // 查询 GUI 忙状态
                                                    ADR_H = 0x00;
                                                    ADR_M = 0x00 >> 9;
                                                    ADR_L = 0x15 >> 1;
                                                    ADR_INC = 0x00;
                                                    RAMMODE = 0xAF;
                                                    while ( !APP_ACK );
                                                    APP_EN = 1;
                                                    while ( APP_EN );
                                                    //RAMMODE = 0x00;
                                                    while ( DATA1 == 0x00 && DATA0 == 0x01 )
                                                    {
                                                        RAMMODE = 0x00;
                                                        delay_us ( 100 );
                                                        RAMMODE = 0xAF;
                                                        while ( !APP_ACK );
                                                        APP_EN = 1;
                                                        while ( APP_EN );
                                                    }
                                                    #endif
                                                    ADR_H = JpegLaber_DGUSII_VP >> 17;
                                                    ADR_M = JpegLaber_DGUSII_VP >> 9;
                                                    ADR_L = JpegLaber_DGUSII_VP >> 1;
                                                    ADR_INC = 0x00;
                                                    
                                                    RAMMODE = 0xAF;
                                                    while ( !APP_ACK );
                                                    APP_EN = 1;
                                                    while ( APP_EN );
                                                    RAMMODE = 0x8F;
                                                    DATA1 = DATA2;
                                                    DATA3 = 0x5A;
                                                    DATA2 = 0xA5;
                                                    //DATA1 = 0xFF;
                                                    DATA0 = 0xFE;
                                                    APP_EN = 1;
                                                    while ( APP_EN );
                                                    RAMMODE = 0x00;

                                                    
                                                

                                                    ADR_H = Icon_Overlay_SP[Index] >> 17;
                                                    ADR_M = Icon_Overlay_SP[Index] >> 9;
                                                    ADR_L = Icon_Overlay_SP[Index] >> 1;
                                                    ADR_INC = 0x01;
                                                    RAMMODE = 0x8F;
                                                    while ( !APP_ACK );
                                                    DATA3 = JpegLaber_DGUSII_VP >> 8;
                                                    DATA2 = JpegLaber_DGUSII_VP >> 0;
                                                    DATA1 = Icon_Overlay_SP_X[Index] >> 8;
                                                    DATA0 = Icon_Overlay_SP_X[Index] >> 0;
                                                    APP_EN = 1;
                                                    while ( APP_EN );
                                                    DATA3 = Icon_Overlay_SP_Y[Index] >> 8;
                                                    DATA2 = Icon_Overlay_SP_Y[Index] >> 0;
                                                    DATA1 = Icon_Overlay_SP_L[Index] >> 8;
                                                    DATA0 = Icon_Overlay_SP_L[Index] >> 0;
                                                    APP_EN = 1;
                                                    while ( APP_EN );
                                                    DATA3 = Icon_Overlay_SP_H[Index] >> 8;
                                                    DATA2 = Icon_Overlay_SP_H[Index] >> 0;
                                                    DATA1 = Icon_Overlay_SP_Mode >> 8;
                                                    DATA0 = Icon_Overlay_SP_Mode >> 0;
                                                    APP_EN = 1;
                                                    while ( APP_EN );
                                                    DATA3 = 0x00;
                                                    DATA2 = 0x80 | ( JpegLaber_DGUSII_VP >> 16 );
                                                    DATA1 = 00;
                                                    DATA0 = 00;
                                                    APP_EN = 1;
                                                    while ( APP_EN );
                                                    RAMMODE = 0x00;





                                                    if ( Icon_Num == 1 ||Icon_Num == 2 )
                                                    {
                                                        Pic_Count[ ( Icon_Num-1 ) /2]++;
                                                    }
                                                    #if sysBEAUTY_MODE_ENABLED
                                                    else
                                                    {
                                                        if(Icon_Num == r11_state.now_choose_pic+5)
                                                        {
                                                            r11_state.pic_capture_flag = 1;
                                                        }
                                                        Pic_Count[ ( Icon_Num-1 )]++;
                                                    }
                                                    #endif /* sysBEAUTY_MODE_ENABLED */
                                                    data_write_f = 0;
                                                    EX1_Start();
                                                // }
                                            }
                                        }
                                    }
                                    else
                                    {
                                        Miss_Align_16KB_Count++;
                                        data_write_f = 0x03;
                                        EX0 = 0;
                                        EX1 = 0;
                                    }
                                }
                            }
                            else
                            {
                                CRC_ERR_Count++;
                                data_write_f = 8;
                                EX0 = 0;
                                EX1 = 0;
                            }
                        }
                        else
                        {
                            Over_Max_16KB_Count++;
                            data_write_f = 9;
                            EX0 = 0;
                            EX1 = 0;
                        }
                    }
                    else
                    {
                        Other_Cmd_Count++;
                        data_write_f = 7;
                        EX0 = 0;
                        EX1 = 0;
                    }
                }
                else
                {
                    NotDeal_OverBuffer_Count++;
                    data_write_f = 4;
                    EX0 = 0;
                    EX1 = 0;
                }
            }
            break;
        }
        default:
        {
            Other_Cmd_Count++;
            data_write_f = 7;
            EX0 = 0;
            EX1 = 0;
            break;
        }
    }
    DATA0 = DATA0_Bak;
    DATA1 = DATA1_Bak;
    DATA2 = DATA2_Bak;
    DATA3 = DATA3_Bak;
    ADR_INC = ADR_INC_Bak;
    ADR_L = ADR_L_Bak;
    ADR_M = ADR_M_Bak;
    ADR_H = ADR_H_Bak;
    RAMMODE = RAMMODE_Bak;
    if ( RAMMODE )
    {
        while ( ~APP_ACK );
    }
}

#endif /* sysBEAUTY_MODE_ENABLED || sysN5CAMERA_MODE_ENABLED || sysADVERTISE_MODE_ENABLED */

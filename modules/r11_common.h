#ifndef R11_COMMON_H
#define R11_COMMON_H
/*
 * 文件名: r11_common.h
 * 说明: R11模块通用头文件，包含视频播放、WiFi等相关宏定义和结构体定义。
 * 遵循MISRA C规范，所有接口均有详细注释。
 */
#include "sys.h"
#include "uart.h"
#include "T5LOSConfig.h"
#include "core_json.h"             

#if sysBEAUTY_MODE_ENABLED ||sysN5CAMERA_MODE_ENABLED || sysADVERTISE_MODE_ENABLED


/**
 * @struct PAGE_S
 * @brief 页面状态结构体，包含各类页面的标志和编号。
 * @note 广告屏部分页面标志和编号设置为0。
 */
typedef struct 
{
	uint8_t menu_flag;           /**< 菜单页面标志 */
	uint8_t menu_page;           /**< 菜单页面编号 */
	uint8_t main_flag;           /**< 主页面标志，广告屏不包含，设置为0 */
	uint8_t main_page;           /**< 主页面编号，广告屏不包含，设置为0 */
	uint8_t detail_flag;         /**< 详细页面标志，广告屏不包含，设置为0 */
	uint8_t detail_page;         /**< 详细页面编号，广告屏不包含，设置为0 */
	uint8_t hotplug_flag;        /**< 热插拔页面标志，广告屏不包含，设置为0 */
	uint8_t hotplug_page;        /**< 热插拔页面编号，广告屏不包含，设置为0 */
	uint8_t crop_flag;           /**< 裁剪页面标志，广告屏不包含，设置为0 */
	uint8_t crop_page;           /**< 裁剪页面编号，广告屏不包含，设置为0 */
	uint8_t video_flag;          /**< 视频页面标志 */
	uint8_t video_page;          /**< 视频页面编号 */
	uint8_t upload_flag;         /**< 上传页面标志，广告屏不包含，设置为0 */
	uint8_t upload_page;         /**< 上传页面编号，广告屏不包含，设置为0 */
	uint8_t fullvideo_flag;      /**< 全屏视频页面标志 */
	uint8_t fullvideo_page;      /**< 全屏视频页面编号 */
	uint8_t folder_flag;		 /**< 文件夹页面标志，广告屏不包含，设置为0 */
	uint8_t folder_page;		 /**< 文件夹页面编号，广告屏不包含，设置为0 */
} PAGE_S;
extern PAGE_S page_st;

/** 图标叠加相关控件定义区域 */
#define Icon_Overlay_SP_Mode 0xFF00
extern code uint16_t Icon_Overlay_SP[11];
extern uint32_t Icon_Overlay_SP_VP[11];
extern uint16_t Icon_Overlay_SP_X[10];  
extern uint16_t Icon_Overlay_SP_Y[10];
extern uint16_t Icon_Overlay_SP_L[10];
extern uint16_t Icon_Overlay_SP_H[10]; 
extern uint16_t Locate_arr[10]; 
extern volatile uint8_t data data_write_f;
extern volatile uint8_t data Bit8_16_Flag;

/**
 * @enum VIDEO_INIT_PROCESS
 * @brief 视频初始化流程枚举类型。
 */
typedef enum
{
	VIDEO_PROCESS_UNINIT = 0,      /**< 未初始化 */
	VIDEO_PROCESS_VOLUME,          /**< 调整音量 */
	VIDEO_PROCESS_SEARCH_LOOP,     /**< 查询循环模式 */
	VIDEO_PROCESS_SIZE,            /**< 设置视频大小 */
	VIDEO_PROCESS_QUERY,           /**< 查询视频 */
	VIDEO_PROCESS_PLAY,            /**< 播放视频 */
	VIDEO_PROCESS_SET_LOOP,        /**< 设置循环 */
	VIDEO_PROCESS_COMPLETE         /**< 初始化完成 */
} VIDEO_INIT_PROCESS;

extern VIDEO_INIT_PROCESS video_init_process; 


/** 视频播放相关定义区域 */
#define SDCARD 1
#define exUDISK 2
#define UDISK 3

#define MP4 0
#define MP3 1
#define JPG 2

#define MAX_MP3_NAME_LEN  		32
#define MAX_VOLUME              40
#define MAX_WIFI_NAME_LEN 		32
#define CHECK_NET_STATUS_ADDR       0x0570
#define CHECK_DEVICE_STATUS_ADDR    0x0571
#define MP4_FILENAME_VP_LIST1       0x0610
#define MP4_FILENAME_VP_LIST2       0x0620
#define MP4_FILENAME_VP_LIST3       0x0630
#define MP4_FILENAME_VP_LIST4       0x0640
#define MP4_FILENAME_VP_LIST5       0x0650
#define VOLUME_SET_ADDR          	0x06D0
#define PLAY_STATUS_ADDR		 	0x06D1
#define LOOP_MODE_ADDR          	0x06D2
#define AUTOPLAY_MODE_ADDR      	0x06D3
#define R11_RESTART_FLAG_ADDR      	0x06D4
#define BIG_SMALL_FLAG_ADDR      	0x06D5

#define NP4_NOW_NUM_ADDR            0x06D7


#define MP4_FILENAME_SP_LIST1       0x3E00
#define MP4_FILENAME_SP_LIST2       0x3E20
#define MP4_FILENAME_SP_LIST3       0x3E40
#define MP4_FILENAME_SP_LIST4       0x3E60
#define MP4_FILENAME_SP_LIST5       0x3E80
/**
 * 在r11_netskinAnalyze.c中定义，此处用于区分，防止占用
#define netWIFI_STATUS_ADDR     0x06D8
#define COMIC_STATUS_ADDR       0x06D9
#define cameraNOW_NUM_ADDR      0x06DA
*/
#define MP4_NOW_PLAY_NAME_ADDR	  	0x06E0

/** 视频播放指令定义区域 */
#define cmdMP4_UPDATEFILE         	0x61
#define cmdMP4_PREVFILE           	0x63
#define cmdMP4_NEXTFILE           	0x62
#define cmdMP4_PLAY                	0x64
#define cmdMP4_PAUSE               	0x65
#define cmdMP4_REPLAY              	0x66
#define cmdMP4_STOP					0x67
#define cmdMP4_AUX_SET              0x6b
#define cmdMP4_PLAY_BY_NUM          0x74
#define cmdMP4_IMG_SET              0x76    /* 0x76为小图播放，0x77为大图播放*/
#define cmdMP4_ROTATE_ANGLE         0x79
#define cmdCHECK_STATUS_NET         0x85
#define cmdCHECK_STATUS_DEVICE      0x88
#define cmdMP4_LOOP_MODE_SET	   	0x90
#define cmdMP4_STORAGE_MEDIUM_STATUS 0xb0
#define cmdMP4_NOW_PLAY_NUMBER      0xb1
#define STORAGE_DISCONN_PAGE         0
#define cmdWIFI_SCAN                0xc0
#define cmdWIFI_CONNECT             0xc1
#define cmdSET_TERNARY_CODE         0xf0
#define cmdSET_WEBSOCKET            0xf1

/** 视频播放按键定义区域 */
#define keyMP4_PAUSE				0x0004
#define keyMP4_STOP                 0x0005
#define keyMP4_AUX_CLOSE			0x000a
#define keyMP4_AUX_ADD				0x000b
#define keyMP4_AUX_SUB				0x000c
#define keyMP4_AUX_SET				0x000f
#define keyMP4_EXIT				 	0x000d
#define keyMP4_REPLAY               0x0013
#define keyMP4_UPDATE_SDCARD       	0x0020
#define keyMP4_UPDATE_exUDISK      	0x002a
#define keyMP4_UPDATE_UDISK        	0x002b
#define keyMP4_1FILE                0x0021
#define keyMP4_2FILE                0x0022
#define keyMP4_3FILE                0x0023
#define keyMP4_4FILE                0x0024
#define keyMP4_5FILE                0x0025
#define keyMP4_IMG_SET_BIG			0x0026
#define keyMP4_IMG_SET_SMALL		0x0027
#define keyMP4_ROTATE_ANGLE			0x0029
#define keyMP4_NONE_LOOP_MODE 	 	0x0036
#define keyMP4_SINGLE_LOOP_MODE   	0x0037
#define keyMP4_ALL_LOOP_MODE      	0x0038
#define keyMP4_CLEAR_PAGE           0x00ff
#define keyMP4_NEXTFILE            	0x0001
#define keyMP4_PREVFILE            	0x0002
#define keyMP4_PREVIOUS_VIDEO       0x0039
#define keyMP4_NEXT_VIDEO           0x003A
#define keyMP4_TOGGLE_PAUSE         0x003B
#define VIDEO_UI_MAX_FILES         5
#define VIDEO_IDLE_TIMEOUT_MS      30000UL

void R11VideoTouchTask(uint8_t allow_start);
uint8_t R11VideoIdleActive(void);
uint16_t R11VideoIdleReturnPage(void);
uint8_t R11VideoWakeBlocked(void);
#define VIDEO_UI_STATUS_ADDR       0x06DB
#define VIDEO_UI_COUNT_ADDR        0x06DC
#define VIDEO_UI_BUTTON_ADDR       0x06DD
void R11VideoUiTask(void);
void R11VideoUiReset(void);

#define keyMP4_PLAY_BY_NUM          0x0A00

/** wifi连接键值定义区域 */
#define keyWIFI_LIST1              	0xaf01
#define keyWIFI_LIST2              	0xaf02
#define keyWIFI_LIST3              	0xaf03
#define keyWIFI_LIST4              	0xaf04
#define keyWIFI_LIST5              	0xaf05
#define keyWIFI_CONNECT             0xaa07
#define keyWIFI_DISCONNECT          0xaa08
#define keyWIFI_SCAN                0xaa09
#define keyWIFI_IP_QUERY            0xaa0a
#define keyWIFI_IP_RETURN           0xaa0f
#define keyWIFI_PREV_LIST           0xaa12
#define keyWIFI_NEXT_LIST           0xaa13

#define keyCHECK_STATUS_WIFI        0xac01
#define keyCHECK_STATUS_ETH0        0xac03
#define keyCHECK_STATUS_4G        	0xac02

#define keyCHECK_STATUS_exUDISK     0xac05
#define keyCHECK_STATUS_SDCARD      0xac06

/** 通用地址宏定义区域 */
#define WIFI_SSID_ADDR             0x04b0
#define WIFI_PASSWD_ADDR           0x04c0
#define R11_RECV_OK                0x01
#define R11_RECV_FAIL              0x00


#define DirPath(Index) ((Index==1)?("url:/mnt/SDCARD/VIDEO/"):\
					   ((Index==2)?("url:/mnt/exUDISK/VIDEO/"):\
                       ((Index==3)?("url:/mnt/UDISK/VIDEO/"):("url:/mnt/exUDISK/VIDEO/"))))

/**
 * @struct PLAYER_T
 * @brief 视频播放器状态结构体。
 */
typedef struct play_state
{
	uint8_t r11_volunme;           /**< 当前音量 */
	uint8_t r11_volunme_Bak;       /**< 音量备份 */
	uint8_t state:3;               /**< 播放状态 */
	uint8_t rotate:2;              /**< 旋转角度 */
	uint8_t save:3;                /**< 保留字段 */
	uint8_t store_type;            /**< 存储类型（1:SDCARD、2:exUDISK、3:UDISK） */
	uint8_t Document_type;         /**< 文件类型（0:MP4 1:MP3 2:JPG） */
	uint8_t page_mp4_nums;         /**< 当前页面视频文件数量 */
	uint16_t serial;               /**< 当前播放序号 */
	uint16_t total_serial;         /**< 视频文件总数量 */
	uint16_t now_play_serial;     /**< 当前播放视频的序号 */
	uint16_t now_play_page;       /**< 当前播放视频所在页面 */
} PLAYER_T;

extern uint16_t mp4_name_len[5];
extern uint8_t mp4_name[5][MAX_MP3_NAME_LEN];
extern PLAYER_T r11_player;
extern uint16_t pixels_arr_h2[5];
extern uint16_t pixels_arr_l2[5];
extern uint8_t wifi_now_offset;

#define Init_Jpeg_Parament();	\
                buf_os_tail         = 0x8000;\
				buf_tail            = 0x8000;\
				Packet_Count        = 0;\
				Packet_TotalCount   = 0;\
				END_CRC             = 0xFF;


#define EX0_Start();	\
		IE0 = 0;\
		EX0 = 1; \
        pic_time_cnt = 50;

#define EX0_Stop();	\
		EX0 = 0;

#define EX1_Start();	\
		IE1 = 0;\
		EX1 = 1;

#define EX1_Stop();	\
		EX1 = 0;

/**
 * @brief JPEG参数初始化，设置相关缓冲区和计数器。
 */
void T5lJpegInit(void);


/**
 * 遍历len长度的数组，遇到0x00，或者0xff，或者'\0',将剩下的数据替换成0x00
 * 如果前几个字符不是/mnt/UDISK/或者/mnt/SDCARD/或者/mnt/exUDISK/,则替换为/mnt/UDISK/tmp
 */
void FormatArrayToFullPath(uint8_t *buf, uint8_t len);


/**
 * @brief 更改图片显示位置和大小。
 * @param x_point X坐标
 * @param y_point Y坐标
 * @param high    高度
 * @param weight  宽度
 * @param big_small_flag 0x00:小图模式，并进行一次刷新 0x01：大图模式，并进行一次刷新 0x02：只更新显示位置，不发送给r11，适用于美容屏
 */
void R11ChangePictureLocate(uint16_t x_point, uint16_t y_point, uint16_t high, uint16_t weight, uint8_t big_small_flag);


/**
 * @brief 处理调试相关的按键值。
 * @param dgus_value 按键值
 */
void R11DebugValueHandle(uint16_t dgus_value);


/**
 * @brief 发送数据到R11串口。
 * @param cmd 指令
 * @param buf 数据缓冲区
 */
void T5lSendUartDataToR11(uint8_t cmd, uint8_t *buf);


/**
 * @brief 清除图片显示。
 * @param clear_type 清除类型
 */
void R11ClearPicture(uint8_t clear_type);


/**
 * @brief 视频播放器主流程处理。
 */
void R11VideoPlayerProcess(void);


/**
 * @brief 处理视频相关的按键值。
 * @param dgus_value 按键值
 */
void R11VideoValueHandle(uint16_t dgus_value);

/**
 * @brief 处理串口协议帧，提取文件名等。
 * @param uart  串口类型指针
 * @param frame 协议帧数据
 * @param len   协议帧长度
 */
void UartR11UserVideoProtocol(UART_TYPE *uart, uint8_t *frame, uint16_t len);


#if R11_WIFI_ENABLED
/**
 * @brief 处理WiFi相关的按键值。
 * @param dgus_value 按键值
 */
void R11WifiValueHandle(uint16_t dgus_value);


/**
 * @brief 处理WiFi相关的协议帧。
 * @param uart  串口类型指针
 * @param frame 协议帧数据
 * @param len   协议帧长度
 */
void UartR11UserWifiProtocol(UART_TYPE *uart,uint8_t *frame, uint16_t len);
#endif /* R11_WIFI_ENABLED */


/**
 * @brief 判断数据包计数（汇编实现，见startup_m51.A51）。
 */
void Judge_Packet_Count(void);

/**
 * @brief 显示调试信息（汇编实现，见startup_m51.A51）。
 */
void Display_Debug_Message(void);

#endif /* sysBEAUTY_MODE_ENABLED ||sysN5CAMERA_MODE_ENABLED || sysADVERTISE_MODE_ENABLED */

#endif /* R11_COMMON_H */

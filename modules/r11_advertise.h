#ifndef R11_ADVERTISE_H
#define R11_ADVERTISE_H

#include "sys.h"
#include "T5LOSConfig.h"
#include <string.h>
#include "uart.h"
#include "r11_common.h"

#if sysADVERTISE_MODE_ENABLED
#define TERNARY_CODE_ADDR       0x0410
#define CPU_INFO_ADDR           0x0482
#define WEBSOCKET_ADDR          0x0680
#define PIXELS_SET_ADDR         0x0580
#define FCLK_DIV_ADDR           0x0581
#define QUANITY_SET_ADDR        0x0582      


#define VIDEO_HIGH_ADDR         0x0586
#define VIDEO_WIDTH_ADDR        0x0587
#define VIDEO_XPOINT_ADDR       0x0588
#define VIDEO_YPOINT_ADDR       0x0589

#define MAIN_HIGH_ADDR          0x058A
#define MAIN_WIDTH_ADDR         0x058B
#define MAIN_XPOINT_ADDR        0x058C
#define MAIN_YPOINT_ADDR        0x058D

#define DETAIL_HIGH_ADDR        0x058E
#define DETAIL_WIDTH_ADDR       0x058F
#define DETAIL_XPOINT_ADDR      0x0590
#define DETAIL_YPOINT_ADDR      0x0591

#define CROP_HIGH_ADDR          0x0592
#define CROP_WIDTH_ADDR         0x0593
#define CROP_XPOINT_ADDR        0x0594
#define CROP_YPOINT_ADDR        0x0595

#define MENU_SCREEN_ADDR        0x05a4   
#define MAIN_SCREEN_ADDR        0x05a5   
#define DETAIL_SCREEN_ADDR      0x05a6
#define HOT_PLUG_ADDR           0x05a7
#define CROP_SCREEN_ADDR        0x05a8
#define VIDEO_SCREEN_ADDR       0x05a9
#define UPLOAD_SCREEN_ADDR      0x05aa
#define WIFI_SCAN_PAGE_ADDR     0x05b8
#define WIFI_DETAIL_PAGE_ADDR   0x05b9
#define WIFI_SCAN_TRANSIT_ADDR  0x05ba
#define WIFI_DTAL_TRANSIT_ADDR  0x05bb
#define WIFI_COMIC_FLAG_ADDR    0x05bc
#define STORE_PAGE_ADDR         0x05bd
#define WIFI_START_ADDR         0x05be

#define VIDEO_FULL_ADDR         0x05c0


#define R11_TASK_INTERVAL       100
#define R11_IDLE_PLAY_TIME_MS   60000UL
#define keyMP4_IDLE_RETURN      0x0200
#define keyMP4_IDLE_LIST        0x0201
#define keyMP4_IDLE_RESTART     0x0202
#define R11_SCAN_ADDRESS     	(uint32_t)0x0600
#define netWIFI_STATUS_ADDR    	0x06D8
#define COMIC_STATUS_ADDR       0x06D9     /** 统一的过渡动画使能标志，初始写1 */



typedef struct
{
	uint16_t screen_ratio;           /* 屏幕分辨率 */
}SCREEN_S;

typedef struct 
{
    uint16_t video_high;         /* 视频显示高度 */
	uint16_t video_weight;       /* 视频显示宽度 */
	uint16_t video_x_point;      /* 视频显示X坐标 */
	uint16_t video_y_point;      /* 视频显示Y坐标 */
	
	uint16_t main_high;          /* 主视图高度 */
	uint16_t main_weight;        /* 主视图宽度 */
	uint16_t main_x_point;       /* 主视图X坐标 */
	uint16_t main_y_point;       /* 主视图Y坐标 */
	
	uint16_t detail_high;        /* 详细视图高度 */
	uint16_t detail_weight;      /* 详细视图宽度 */
	uint16_t detail_x_point;     /* 详细视图X坐标 */
	uint16_t detail_y_point;     /* 详细视图Y坐标 */
	
	uint16_t crop_high;          /* 裁剪区域高度 */
	uint16_t crop_weight;        /* 裁剪区域宽度 */
	uint16_t crop_x_point;       /* 裁剪区域X坐标 */
	uint16_t crop_y_point;       /* 裁剪区域Y坐标 */
} MAINVIEW_S;


typedef struct 
{
	uint8_t scan_flag;           /* WiFi扫描页面标志 */
	uint8_t scan_page;           /* WiFi扫描页面编号 */
	
	uint8_t detail_flag;         /* WiFi详情页面标志 */
	uint8_t detail_page;         /* WiFi详情页面编号 */
	
	uint8_t tr_scan_flag;        /* WiFi扫描传输页面标志 */
	uint8_t tr_scan_page;        /* WiFi扫描传输页面编号 */
	
	uint8_t tr_detail_flag;      /* WiFi详情传输页面标志 */
	uint8_t tr_detail_page;      /* WiFi详情传输页面编号 */
	
	uint8_t comic_flag;          /* WiFi连接页面标志 */
	uint8_t comic_page;          /* WiFi连接页面编号 */
	
	uint8_t store_flag;          /* 商店页面标志 */
	uint8_t store_page;          /* 商店页面编号 */
	
	uint8_t start_flag;          /* WiFi启动页面标志 */
	uint8_t start_page;          /* WiFi启动页面编号 */
	
	uint16_t wifi_start_addr;    /* WiFi启动地址 */
} WIFI_PAGE_S;


typedef struct 
{
	uint16_t restart_flag;		  /* 重启标志，0为未重启，1为重启 */
}R11_STATE;

/**
 * @brief 网络连接状态
 * @note 广告屏连接状态只有断开和连接两种状态
 */
typedef enum
{
	NET_DISCONNECTED,
	NET_CONNECTED
}NET_CONNECTED_STATE;

extern MAINVIEW_S mainview;
extern SCREEN_S screen_opt;
extern R11_STATE r11_state;
extern WIFI_PAGE_S wifi_page;
extern NET_CONNECTED_STATE net_connected_state;

void R11ConfigInitFormLib(void);
/* Call once after APP initialization, before task dispatch, to notify V851. */
void R11AdvertiseNotifyReady(void);
void R11AdvertiseTask(void);
void UartR11UserAdvertiseProtocol(UART_TYPE *uart,uint8_t *frame, uint16_t len);
#endif  /* sysADVERTISE_MODE_ENABLED */

#endif /* R11_ADVERTISE_H */

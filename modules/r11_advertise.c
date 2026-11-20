#include "r11_advertise.h"
#include "r11_timezone.h"

#if sysADVERTISE_MODE_ENABLED

SCREEN_S screen_opt;
MAINVIEW_S mainview;
WIFI_PAGE_S wifi_page;
R11_STATE r11_state = {UINT16_PORT_MAX}; // 初始化R11状态
NET_CONNECTED_STATE net_connected_state = NET_DISCONNECTED;

void R11AdvertiseNotifyReady(void)
{
    uint8_t frame[7] = {0xAAU, 0x55U, 0x00U, 0x03U, 0xF3U, 0U, 0U};
    uint16_t crc;

    /* Match BOOT's AA55 CRC: include header/length, low byte first. */
    crc = crc_16(frame, 5U);
    frame[5] = (uint8_t)crc;
    frame[6] = (uint8_t)(crc >> 8);
    UartSendData(&Uart_R11, frame, sizeof(frame));
}

void R11ConfigInitFormLib(void)
{
    uint16_t read_param[30];
	/** 写入ws地址和三元码信息 */
	FlashToDgus(flashMAIN_BLOCK_ORDER,TERNARY_CODE_ADDR,TERNARY_CODE_ADDR,0x06);
	FlashToDgus(flashMAIN_BLOCK_ORDER,WEBSOCKET_ADDR,WEBSOCKET_ADDR,0x20);
    FlashToDgus(flashMAIN_BLOCK_ORDER,PIXELS_SET_ADDR,PIXELS_SET_ADDR,0x48);
    	/** 1.进行分辨率的初始化，针对2k分辨率需要修改主频 */
	read_dgus_vp(PIXELS_SET_ADDR,(uint8_t*)&read_param[0],1);
	memcpy(&screen_opt, &read_param[0], 2);
	if(screen_opt.screen_ratio == 0)  //19201080
	{
		sys_2k_ratio = 1;
		sysFOSC = 383385600;
		sysFCLK = 383385600;
	}else{
		sys_2k_ratio = 0;
		sysFOSC = 206438400;
		sysFCLK = 206438400;
	}
	/** 2.进行屏幕设置的初始化，包括分频系数，显示质量，截图张数 */
    Icon_Overlay_SP_VP[0] = Icon_Overlay_SP_VP[2] = 0x10000;
	Icon_Overlay_SP_VP[1] = Icon_Overlay_SP_VP[3] = 0x28000;
    Icon_Overlay_SP_VP[4] = Icon_Overlay_SP_VP[5] = Icon_Overlay_SP_VP[6] = 0x3f000;
	Icon_Overlay_SP_VP[7] = Icon_Overlay_SP_VP[8] = Icon_Overlay_SP_VP[9] = Icon_Overlay_SP_VP[10] = 0x3f000;


	/* 3.针对主显示位置进行初始化 */
	read_dgus_vp(VIDEO_HIGH_ADDR,(uint8_t*)&read_param[0],16);
	memcpy(&mainview, &read_param[0], 32);

	Icon_Overlay_SP_X[0] = Icon_Overlay_SP_X[1] = mainview.video_x_point;
	Icon_Overlay_SP_X[2] = Icon_Overlay_SP_X[3] = mainview.video_x_point;
	Icon_Overlay_SP_Y[0] = Icon_Overlay_SP_Y[1] = mainview.video_y_point;
	Icon_Overlay_SP_Y[2] = Icon_Overlay_SP_Y[3] = mainview.video_y_point;

	Icon_Overlay_SP_L[0] = Icon_Overlay_SP_L[1] = mainview.video_high;
	Icon_Overlay_SP_L[2] = Icon_Overlay_SP_L[3] = mainview.video_high;
	Icon_Overlay_SP_H[0] = Icon_Overlay_SP_H[1] = mainview.video_weight;
	Icon_Overlay_SP_H[2] = Icon_Overlay_SP_H[3] = mainview.video_weight;

	Locate_arr[0] = mainview.video_x_point;
	Locate_arr[1] = mainview.video_y_point;


	/** 4.针对页面切换进行初始化 */
	read_dgus_vp(MENU_SCREEN_ADDR,(uint8_t*)&read_param[0],7);
	memcpy(&page_st, &read_param[0], 14);
	/** 5.针对wifi相关过渡页进行初始化 */
	read_dgus_vp(WIFI_SCAN_PAGE_ADDR,(uint8_t*)&read_param[0],7);
	memcpy(&wifi_page, &read_param[0], 14);

	read_dgus_vp(WIFI_START_ADDR,(uint8_t*)&wifi_page.wifi_start_addr,1);
	read_dgus_vp(VIDEO_FULL_ADDR,(uint8_t*)&read_param[0],1);
	page_st.fullvideo_flag = read_param[0] >> 8;
	page_st.fullvideo_page = read_param[0] & 0xff;
}


/*
 * @brief 页面切换时调整图片显示位置。
 */
static void R11PageInitChange()
{
	uint16_t now_pic;
    read_dgus_vp(sysDGUS_PIC_NOW, (uint8_t *)&now_pic, 1);
    if(now_pic == (uint16_t)page_st.detail_page)
	{
		R11ChangePictureLocate(mainview.detail_x_point,mainview.detail_y_point,mainview.detail_high,mainview.detail_weight,0x02);
	}else if(now_pic == (uint16_t)page_st.main_page)
	{
		R11ChangePictureLocate(mainview.main_x_point,mainview.main_y_point,mainview.main_high,mainview.main_weight,0x02);
	}else
	{
		R11ChangePictureLocate(mainview.main_x_point,mainview.main_y_point,mainview.main_high,mainview.main_weight,0x02);
	}
}


/*
 * @brief 上电初始化，发送相关参数到R11。
 * @note 该函数用来设置显示质量，分频系数，8线或者16线。
 */
static void R11StartPowerInit(void)
{
	uint8_t r11_send_buf[10];
	uint16_t quanity_set,fclk_div;
	r11_send_buf[0] = 0xAA;
	r11_send_buf[1] = 0x55;
	r11_send_buf[2] = 0x00;
	r11_send_buf[3] = 0x04; 
	r11_send_buf[4] = 0xd0; 
	read_dgus_vp(QUANITY_SET_ADDR,(uint8_t*)&quanity_set,1);
	r11_send_buf[5]=(uint8_t)quanity_set;
	read_dgus_vp(FCLK_DIV_ADDR,(uint8_t*)&fclk_div,1);
	r11_send_buf[6]=(uint8_t)fclk_div;
	r11_send_buf[7]=Bit8_16_Flag*8;
	UartSendData(&Uart_R11,r11_send_buf,8);
	UartSendData(&Uart2,r11_send_buf,8);
}


static void R11RestartInit()
{
	uint16_t write_param = 1;
    R11PageInitChange();
	T5lSendUartDataToR11(cmdSET_TERNARY_CODE,NULL);
	delay_ms(100);
	T5lSendUartDataToR11(cmdSET_WEBSOCKET,NULL);
	write_dgus_vp(COMIC_STATUS_ADDR,(uint8_t *)&write_param,1);
	R11StartPowerInit();
    R11TimezoneSync();
}



static void R11ValueScanTask(void)
{
    const uint16_t uint16_port_zero = 0;
    uint16_t dgus_value;
  
    #if sysDGUS_AUTO_UPLOAD_ENABLED || uartTA_PROTOCOL_ENABLED
    DgusAutoUpload();
    #endif /* sysDGUS_AUTO_UPLOAD_ENABLED ||uartTA_PROTOCOL_ENABLED */

    read_dgus_vp(R11_SCAN_ADDRESS, (uint8_t *)&dgus_value, 1);
    if(((dgus_value>>8) == 0x00 && (dgus_value & 0x00FF) != 0x00)||(dgus_value>>8) == 0x0A)
	{
		R11VideoValueHandle(dgus_value);
		write_dgus_vp(R11_SCAN_ADDRESS,(uint8_t*)&uint16_port_zero,1);
	}else if((dgus_value>>8) == 0x01)
	{
		R11DebugValueHandle(dgus_value);
		write_dgus_vp(R11_SCAN_ADDRESS,(uint8_t*)&uint16_port_zero,1);
	}
	#if R11_WIFI_ENABLED
	else if((dgus_value>>8) >= 0xaa && (dgus_value>>8) <= 0xaf)
	{
		R11WifiValueHandle(dgus_value);
		write_dgus_vp(R11_SCAN_ADDRESS,(uint8_t*)&uint16_port_zero,1);
	}
	#endif /* R11_WIFI_ENABLED */
}

void R11AdvertiseTask(void)
{
    R11TimezoneTask();
    if(r11_state.restart_flag == 1)
    {
        R11RestartInit();
		video_init_process = VIDEO_PROCESS_UNINIT;
        r11_state.restart_flag = 2;  /* 重置重启标志 */
    }else if(r11_state.restart_flag == 2)
	{
        R11VideoPlayerProcess();
		R11ValueScanTask();
	}
}


void UartR11UserAdvertiseProtocol(UART_TYPE *uart,uint8_t *frame, uint16_t len)
{
	uint16_t now_pic;
    if(len < 5U)
    {
        return;
    }
    if(frame[4] == cmdSET_TIMEZONE)
    {
        UartR11UserTimezoneProtocol(uart, frame, len);
        return;
    }
    if(len < 7U)
    {
        return;
    }
    if(frame[0] == 0xAA && frame[1] == 0x55 && frame[4] == 0x82 && uart == &Uart_R11)
    {
        if(len < 6 || len < ((frame[2]<<8|frame[3])+4))
        {
            return;
        }

        // if((frame[len-1]<<8 |frame[len-2]) != crc_16(&frame[4], len-6))
        // {
        //     return;
        // }else{
        //     len -= 2;
        // }
		
        if(frame[5] == 0x04 && frame[6] == 0x82)
        {
            r11_state.restart_flag = 1;  /* 设置重启标志 */
			write_dgus_vp(CPU_INFO_ADDR,(uint8_t*)&frame[7],(len-7)>>1);
        }else if(frame[5] == 0x04 && frame[6] == 0xa2)
		{
			/** 
			 * 写入网络状态信息 
			 * 0x01未连接，0x02已连接
			 */
			read_dgus_vp(sysDGUS_PIC_NOW,(uint8_t*)&now_pic,1);
			if(wifi_page.tr_detail_flag == 0x5a && now_pic == wifi_page.tr_detail_page)
			{
				
				if(page_st.menu_flag == 0x5a)
				{
					SwitchPageById((uint16_t)page_st.menu_page); 
				}
			}
			if(frame[7] == 0x00 && frame[8] == 0x02 && net_connected_state != NET_CONNECTED)
			{
				net_connected_state = NET_CONNECTED;
				write_dgus_vp(0x4a2,(uint8_t*)&frame[7],1);	
			}else if(frame[7] == 0x00 && frame[8] == 0x01 && net_connected_state != NET_DISCONNECTED)
			{
				net_connected_state = NET_DISCONNECTED;
				write_dgus_vp(0x4a2,(uint8_t*)&frame[7],1);
			}
		}
    }
}

#endif /* sysADVERTISE_MODE_ENABLED */

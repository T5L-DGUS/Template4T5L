#include "sys.h"
#include "uart.h"
#include "timer.h"
#include "t5l_stc.h"
#include "boot_handoff.h"

#if v851PROTOCOL_ENABLED
#include "v851_protocol.h"
#include "v851_wifi.h"
#endif /* v851PROTOCOL_ENABLED */

#if uartMODBUS_PROTOCOL_ENABLED
#include "modbus.h" 
#endif /* uartMODBUS_PROTOCOL_ENABLED */

#include "rtc.h"

#if gpioGPIO_ENABLE
#include "gpio.h"
#endif /* gpioGPIO_ENABLE */

/* 二维码（QR）：编码/绘制在 QR_Encode.c，页面刷新在 qr_page.c */
#include "QR_Encode.h"
#include "qr_page.h"

void main(void)
{

	#if flashDUAL_BACKUP_ENABLED
	T5lNorFlashInit();
	#endif /* flashDUAL_BACKUP_ENABLED */


	T5LCpuInit();
	BootHandoffInit();
	T5l_Stc_Init();

	QR_ConfigInit();		//上电一次性读取并缓存二维码配置（NOR flash 配置区），此后页面切换不再读flash

	#if v851PROTOCOL_ENABLED
	V851ProtocolInit();
	V851WifiInit();
	#endif /* v851PROTOCOL_ENABLED */

	RtcInit();
	SysTaskAdd(0, RTC_INTERVAL, RtcTask);

	SysTaskAdd(2, UART_TASK_INTERVAL, UartProtocalHandleTask);
	SysTaskAdd(6, BOOT_HANDOFF_TASK_INTERVAL, BootHandoffTask);

	#if v851PROTOCOL_ENABLED
	SysTaskAdd(3, V851_WIFI_TASK_INTERVAL, V851WifiTask);
	#endif /* v851PROTOCOL_ENABLED */

	#if v851PROTOCOL_ENABLED
	SysTaskAdd(5, V851_PROTOCOL_TASK_INTERVAL, V851ProtocolTask);
	#endif /* v851PROTOCOL_ENABLED */

	SysTaskAdd(7, T5LTOSTC_TASK_INTERVAL1, T5L_Stc_Poll);
	SysTaskAdd(8, T5LTOSTC_TASK_INTERVAL2, Mult_Task);

	SysTaskAdd(4, 10,  QR_PageFirstRun);	//二维码：10ms检测页面切换
	SysTaskAdd(9, 500, QR_PageRefresh);		//二维码：500ms周期刷新内容

	// SysTaskAdd(8, 1, key_scanf);

	while(1)
	{
		SysTaskRun();
	}
}

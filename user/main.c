#include "sys.h"
#include "uart.h"
#include "timer.h"
#include "t5l_stc.h"

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

void main(void)
{

	#if flashDUAL_BACKUP_ENABLED
	T5lNorFlashInit();
	#endif /* flashDUAL_BACKUP_ENABLED */


	T5LCpuInit();
	T5l_Stc_Init();

	#if v851PROTOCOL_ENABLED
	V851ProtocolInit();
	V851WifiInit();
	#endif /* v851PROTOCOL_ENABLED */

	RtcInit();
	SysTaskAdd(0, RTC_INTERVAL, RtcTask);

	SysTaskAdd(2, UART_TASK_INTERVAL, UartProtocalHandleTask);

	#if v851PROTOCOL_ENABLED
	SysTaskAdd(3, V851_WIFI_TASK_INTERVAL, V851WifiTask);
	#endif /* v851PROTOCOL_ENABLED */

	#if v851PROTOCOL_ENABLED
	SysTaskAdd(5, V851_PROTOCOL_TASK_INTERVAL, V851ProtocolTask);
	#endif /* v851PROTOCOL_ENABLED */

	SysTaskAdd(7, T5LTOSTC_TASK_INTERVAL1, T5L_Stc_Poll);
	SysTaskAdd(8, T5LTOSTC_TASK_INTERVAL2, Mult_Task);

	// SysTaskAdd(8, 1, key_scanf);

	while(1)
	{
		SysTaskRun();
	}
}

#include "sys.h"
#include "uart.h"
#include "rtc.h"
#include "multi_input.h"
#include "slovak_dictionary.h"
#include "croatian_dictionary.h"

void main(void)
{
    #if flashDUAL_BACKUP_ENABLED
    T5lNorFlashInit();
    #endif /* flashDUAL_BACKUP_ENABLED */

    T5LCpuInit();

    RtcInit();
    SysTaskAdd(0U, RTC_INTERVAL, RtcTask);

    if(MultiInputInit(&SlovakLanguagePack) &&
       MultiInputSetSecondaryLanguage(&CroatianLanguagePack))
    {
        SysTaskAdd(MULTI_INPUT_TASK_ID,
                   MULTI_INPUT_TASK_INTERVAL,
                   MultiInputTask);
        SysTaskAdd(MULTI_INPUT_DICTIONARY_TASK_ID,
                   MULTI_INPUT_DICTIONARY_INTERVAL,
                   MultiInputDictionaryTask);
    }

    SysTaskAdd(2U, UART_TASK_INTERVAL, UartProtocalHandleTask);

    while(1)
    {
        SysTaskRun();
    }
}

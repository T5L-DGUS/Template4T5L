#include "r11_timezone.h"
#include "rtc.h"
#include <string.h>

#if sysADVERTISE_MODE_ENABLED

static uint8_t timezone_sign;
static uint8_t timezone_hours = 8U;
static uint8_t timezone_stored;
static uint8_t timezone_startup_sync;
static uint8_t timezone_last_input[4];

static uint8_t R11TimezoneValid(uint8_t sign, uint8_t hours)
{
    return sign <= 1U && hours <= (sign == 0U ? 14U : 12U);
}

static void R11TimezoneStatus(uint8_t status)
{
    uint8_t value[2];
    value[0] = 0U;
    value[1] = status;
    write_dgus_vp(TIMEZONE_STATUS_ADDR, value, 1U);
}

static void R11TimezoneDisplay(uint8_t sign, uint8_t hours)
{
    uint8_t text[16];
    memset(text, 0, sizeof(text));
    memcpy(text, "UTC+00:00", 9U);
    text[3] = sign != 0U && hours != 0U ? '-' : '+';
    text[4] = '0' + hours / 10U;
    text[5] = '0' + hours % 10U;
    write_dgus_vp(TIMEZONE_TEXT_ADDR, text, 8U);
}

static void R11TimezoneLoadInput(void)
{
    timezone_last_input[0] = 0U;
    timezone_last_input[1] = timezone_sign;
    timezone_last_input[2] = 0U;
    timezone_last_input[3] = timezone_hours;
    write_dgus_vp(TIMEZONE_SIGN_ADDR, timezone_last_input, 2U);
    R11TimezoneDisplay(timezone_sign, timezone_hours);
}

void R11TimezoneInit(void)
{
    uint8_t record[8];
    uint8_t command[2] = {0U, 0U};

    timezone_sign = 0U;
    timezone_hours = 8U;
    timezone_stored = 0U;
    timezone_startup_sync = 1U;
    FlashToDgusWithData(flashMAIN_BLOCK_ORDER, TIMEZONE_STORAGE_ADDR,
                       TIMEZONE_STORAGE_ADDR, record, 4U);
    /* Tagged/versioned record keeps erased or older database contents invalid. */
    if(record[0] == 0x54U && record[1] == 0x5AU && record[2] == 1U &&
       record[7] == 0xA5U && record[5] == (uint8_t)(record[3] ^ 0xFFU) &&
       record[6] == (uint8_t)(record[4] ^ 0xFFU) &&
       R11TimezoneValid(record[3], record[4]))
    {
        timezone_sign = record[4] == 0U ? 0U : record[3];
        timezone_hours = record[4];
        timezone_stored = 1U;
    }
    R11TimezoneLoadInput();
    write_dgus_vp(TIMEZONE_COMMAND_ADDR, command, 1U);
    R11TimezoneStatus(TIMEZONE_STATUS_LOADED);
}

void R11TimezoneSync(void)
{
    uint8_t frame[7] = {0xAAU, 0x55U, 0U, 3U, cmdSET_TIMEZONE, 0U, 0U};
    frame[5] = timezone_sign;
    frame[6] = timezone_hours;
    timezone_startup_sync = 0U;
    R11TimezoneStatus(TIMEZONE_STATUS_PENDING);
    /* This command has no CRC, unlike the separate F3 APP-ready notification. */
    UartSendData(&Uart_R11, frame, sizeof(frame));
}

void R11TimezoneTask(void)
{
    uint8_t input[6], record[8], sign, hours;
    uint8_t clear[2] = {0U, 0U};

    read_dgus_vp(TIMEZONE_COMMAND_ADDR, input, 3U);
    /* APP/OTA can restart while V851 stays up and sends no new handshake.
     * The first scheduled task runs after main's F3 ready notification.
     * Save/Enter send below, so the first click cannot trigger two frames. */
    if(timezone_startup_sync != 0U &&
       !(input[0] == 0U && (input[1] == TIMEZONE_COMMAND_SAVE ||
                          input[1] == TIMEZONE_COMMAND_ENTER)))
    {
        R11TimezoneSync();
    }
    if(input[0] != 0U || input[1] != 0U)
    {
        write_dgus_vp(TIMEZONE_COMMAND_ADDR, clear, 1U);
    }
    if(input[0] == 0U && input[1] == TIMEZONE_COMMAND_ENTER)
    {
        R11TimezoneLoadInput();
        R11TimezoneSync();
        return;
    }
    if(input[0] == 0U && input[1] == TIMEZONE_COMMAND_CANCEL)
    {
        R11TimezoneLoadInput();
        R11TimezoneStatus(TIMEZONE_STATUS_LOADED);
        return;
    }
    if(input[2] != 0U || input[4] != 0U || !R11TimezoneValid(input[3], input[5]))
    {
        if(memcmp(timezone_last_input, &input[2], 4U) != 0 || input[1] != 0U)
        {
            memcpy(timezone_last_input, &input[2], 4U);
            R11TimezoneStatus(TIMEZONE_STATUS_INVALID);
        }
        return;
    }
    sign = input[5] == 0U ? 0U : input[3];
    hours = input[5];
    if(memcmp(timezone_last_input, &input[2], 4U) != 0)
    {
        memcpy(timezone_last_input, &input[2], 4U);
        R11TimezoneDisplay(sign, hours);
        R11TimezoneStatus(TIMEZONE_STATUS_LOADED);
    }
    if(input[0] != 0U || input[1] != TIMEZONE_COMMAND_SAVE)
    {
        return;
    }
    if(timezone_stored == 0U || sign != timezone_sign || hours != timezone_hours)
    {
        record[0] = 0x54U;
        record[1] = 0x5AU;
        record[2] = 1U;
        record[3] = sign;
        record[4] = hours;
        record[5] = (uint8_t)(sign ^ 0xFFU);
        record[6] = (uint8_t)(hours ^ 0xFFU);
        record[7] = 0xA5U;
        /* Finish the blocking NOR write before V851 can send its reply. */
        DgusToFlashWithData(flashMAIN_BLOCK_ORDER, TIMEZONE_STORAGE_ADDR,
                           TIMEZONE_STORAGE_ADDR, record, 4U);
        timezone_sign = sign;
        timezone_hours = hours;
        timezone_stored = 1U;
    }
    R11TimezoneLoadInput();
    R11TimezoneSync();
}

static uint8_t R11TimezoneTimeValid(uint8_t *date)
{
    uint8_t days;
    if(date[0] > 99U || date[1] == 0U || date[1] > 12U ||
       date[2] == 0U || date[3] > 23U || date[4] > 59U || date[5] > 59U)
    {
        return 0U;
    }
    days = 31U;
    if(date[1] == 2U)
    {
        days = date[0] % 4U == 0U ? 29U : 28U;
    }
    else if(date[1] == 4U || date[1] == 6U || date[1] == 9U || date[1] == 11U)
    {
        days = 30U;
    }
    return date[2] <= days;
}

void UartR11UserTimezoneProtocol(UART_TYPE *uart, uint8_t *frame, uint16_t len)
{
    uint8_t rtc[8], input[4], sign;
    if(uart != &Uart_R11 || len != 13U || frame[0] != 0xAAU || frame[1] != 0x55U ||
       frame[2] != 0U || (frame[3] != 4U && frame[3] != 9U) || frame[4] != cmdSET_TIMEZONE)
    {
        return;
    }
    if(!R11TimezoneValid(frame[5], frame[6]) || !R11TimezoneTimeValid(&frame[7]))
    {
        return;
    }
    sign = frame[6] == 0U ? 0U : frame[5];
    /* A late reply to an older setting must not overwrite the current clock. */
    if(sign != timezone_sign || frame[6] != timezone_hours)
    {
        return;
    }
    memcpy(rtc, &frame[7], 3U);
    rtc[3] = 0U;
    memcpy(&rtc[4], &frame[10], 3U);
    rtc[7] = 0U;
    RtcSetTime(rtc);
    RtcReadTime();
    read_dgus_vp(TIMEZONE_SIGN_ADDR, input, 2U);
    if(input[0] == 0U && input[2] == 0U && input[1] <= 1U &&
       (input[1] == timezone_sign || timezone_hours == 0U) && input[3] == timezone_hours)
    {
        R11TimezoneStatus(TIMEZONE_STATUS_SYNCED);
    }
}

#endif

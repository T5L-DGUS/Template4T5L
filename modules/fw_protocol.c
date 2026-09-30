#ifdef FW_PROTOCOL_TEST
#include "fw_test_platform.h"
#else
#include "fw_protocol.h"
#include "modbus.h"
#include "timer.h"
#if sysADVERTISE_MODE_ENABLED
#include "r11_common.h"
#endif
#endif

/* FW uses Modbus on UART2. VP values preserve wire units. */
#define FW_ADDRESS 1
#define FW_TIMEOUT 500UL
#define FW_ERR_TIMEOUT 0x0100
#define FW_ERR_VALUE   0x0101
#define FW_ERR_BUSY    0x0102
/* Local settings: never forwarded as FW register addresses. */
#define LOCAL_ENABLE 0x37A0
#define LOCAL_HOURS  0x37A1
#define LOCAL_MINS   0x37A2
#define LOCAL_LIGHT  0x37A3
#define LOCAL_STATE  0x37A4
#define LOCAL_REMAIN 0x37A8
#define LOCAL_RECORD 0x37B0
static uint8_t timer_enabled, timer_fault, timer_wait, timer_owner;
static uint8_t local_save, saved_enable, brightness, timer_running;
static uint16_t timer_minutes, saved_minutes;
static uint32_t timer_elapsed, timer_at, local_display_at;
static void LocalDraft(void);
static void LocalCommit(void);
static void LocalTask(uint32_t now);
typedef struct { uint8_t first; uint8_t count; } FwRange;
static code FwRange ranges[] = {
    {0,2}, {4,2}, {8,2}, {11,1}, {13,2}, {16,4}, {21,1},
    {30,8}, {39,1}, {41,5}, {50,2}, {53,1}, {69,9}, {79,5}, {85,1}
};
static code FwRange live_ranges[] = {
    {13,1}, {16,1}, {30,3}, {34,4}, {39,1}, {41,4}, {50,2}, {82,1}
};
static code uint8_t basic_fields[] = {4,5};
static code uint8_t advanced_fields[] = {0,1,8,9,11,17,70,76,80,81,82,83,85};
static code uint8_t display_fields[] = {4,5,0,1,8,9,11,17,70,76,80,81,82,83,85};
static code uint8_t display_decimals[] = {0,1,1,0,0,0,0,0,0,0,1,2,0,1,0};
static code uint8_t live_fields[] = {13,30,31,32,34,35,36,37,50,51,43,0};
static code uint8_t live_decimals[] = {0,1,1,1,1,2,0,0,2,0,2,1};
static uint16_t xdata edit_base[89], save_values[13];
static uint8_t xdata edit_valid[12], save_addresses[13];
static uint8_t full_refresh, save_count, save_index, ui_write, editing;
static uint16_t fw_page, return_page, nav_target, operation_page;
static uint16_t warning_submit_page;
static uint32_t refreshed_at, failed_at, alarm_at, edit_at;
static uint8_t offline, alarm_cursor;
static void UiAfterReply(uint16_t address, uint16_t value);
static void UiDisplay(void);
static uint8_t xdata rx[64];
static uint8_t xdata mirror_valid[12];
/* Last confirmed display survives an in-flight write. Control decisions still
 * require mirror_valid and therefore cannot act on stale dependent settings. */
static uint8_t xdata display_valid[12];
static uint8_t rx_size;
static uint8_t command, range_index, video_ready, software_ready;
static uint8_t refresh_active, refresh_pending;
static uint16_t request_address, request_value, request_count;
static uint16_t last_page;
static uint16_t ui_result_page;
static uint32_t sent_at, last_rx;

static uint32_t FwNow(void)
{
    uint32_t now;
    uint8_t enabled = ET0;
    ET0 = 0;
    now = GetSysTick();
    ET0 = enabled;
    return now;
}

static void FwSet(uint16_t address, uint16_t value)
{
    write_dgus_vp(address, (uint8_t *)&value, 1);
}

static uint16_t FwGet(uint16_t address)
{
    uint16_t value;
    read_dgus_vp(address, (uint8_t *)&value, 1);
    return value;
}
static void UiSet(uint16_t address, uint16_t value)
{
    if(FwGet(address)!=value) FwSet(address,value);
}

static void FwInvalidateMirror(void)
{
    uint8_t i;
    for(i = 0; i < sizeof(mirror_valid); ++i) mirror_valid[i] = 0;
}

static uint8_t UiPage(uint16_t p)
{
    switch(p) {
        case 1: case 2: case 3: case 4: case 5: case 6: case 7:
        case 8: case 9: case 10: case 11: case 12: case 13:
        case 15: case 16: case 17: case 18: case 19: case 20: case 21: case 22:
        case 27: case 30: case 31: case 39: case 40: case 42: case 43: return 1;
        default: return 0;
    }
}
static uint8_t UiWarningPage(uint16_t p)
{
    return p==8 || p==10 || p==11 || p==12 || p==13 ||
        (p>=15 && p<=22) || p==43;
}
/* Fixed page mapping: never use the independently rotating alarm cursor. */
static uint8_t UiWarningAlarm(uint16_t p, uint8_t *reg, uint8_t *alarm_position)
{
    *reg=41;
    switch(p) {
        case 11: *alarm_position=13; break; case 13: *alarm_position=2; break;
        case 15: *alarm_position=7; break; case 16: *alarm_position=0; break;
        case 17: *alarm_position=14; break; case 18: *alarm_position=4; break;
        case 20: *alarm_position=1; break; case 22: *alarm_position=6; break;
        case 10: *reg=42; *alarm_position=14; return 43;
        case 21: *reg=42; *alarm_position=13; return 44;
        case 43: *reg=42; *alarm_position=12; return 42;
        default: return 0;
    }
    return 7;
}
static uint8_t UiKey(uint16_t k)
{
    switch(k) {
        case 1: case 2: case 3: case 4: case 7: case 25: case 26:
        case 27: case 28: case 31: case 32: case 33: case 34: case 35:
        case 36: case 38: case 39: case 40: case 41: case 42: case 43:
        case 44: case 50: case 51: return 1;
        default: return 0;
    }
}
static uint8_t Valid(uint16_t a) { return (mirror_valid[a>>3] & (1U<<(a&7))) != 0; }
static uint8_t DisplayValid(uint16_t a) { return (display_valid[a>>3] & (1U<<(a&7))) != 0; }
static uint16_t UiLogicalPage(void)
{
#if sysADVERTISE_MODE_ENABLED
    if(R11VideoIdleActive()) return R11VideoIdleReturnPage();
#endif
    return ReadPageId();
}
static void UiText(uint16_t vp, const char *s, uint8_t words)
{
    uint8_t i, ended=0;
    uint16_t v;
    for(i=0;i<words;i++) {
        v=0;
        if(!ended && *s) { v=(uint16_t)(uint8_t)*s++ << 8; } else ended=1;
        if(!ended && *s) { v|=(uint8_t)*s++; } else ended=1;
        UiSet(vp+i,v);
    }
}
static void UiNumber(uint16_t vp, uint16_t value, uint8_t decimals, uint8_t sign, uint8_t valid, uint8_t words)
{
    char buf[10], rev[8];
    uint8_t n=0, j=0, neg=0;
    uint16_t mag=value;
    if(!valid) { UiText(vp,"--",words); return; }
    if(sign && (int16_t)value<0) { neg=1; mag=(uint16_t)(-(int16_t)value); }
    do { rev[n++]=(char)('0'+mag%10); mag/=10; } while(mag || n<=decimals);
    if(neg) buf[j++]='-';
    while(n) { buf[j++]=rev[--n]; if(decimals && n==decimals) buf[j++]='.'; }
    buf[j]=0; UiText(vp,buf,words);
}
static void LocalDraft(void)
{
    FwSet(LOCAL_ENABLE,timer_enabled);
    FwSet(LOCAL_HOURS,timer_minutes/60);
    FwSet(LOCAL_MINS,timer_minutes%60);
}
static uint16_t LocalChecksum(uint16_t enabled, uint16_t minutes)
{
    return (uint16_t)(0xA71D ^ (enabled*257U) ^ minutes ^ (minutes<<5));
}
static uint8_t LocalRecordValid(void)
{
    uint16_t e=FwGet(LOCAL_RECORD+2), m=FwGet(LOCAL_RECORD+3);
    return FwGet(LOCAL_RECORD)==0x5343 && FwGet(LOCAL_RECORD+1)==1 &&
        e<=1 && m<=1499 && (!e || m) &&
        FwGet(LOCAL_RECORD+4)==LocalChecksum(e,m) &&
        FwGet(LOCAL_RECORD+5)==(uint16_t)~LocalChecksum(e,m);
}
static void LocalInit(void)
{
    timer_enabled=0; timer_minutes=1440;
#ifndef FW_PROTOCOL_TEST
    FlashToDgus(flashMAIN_BLOCK_ORDER,LOCAL_RECORD,LOCAL_RECORD,6);
    if(LocalRecordValid()) {
        timer_enabled=(uint8_t)FwGet(LOCAL_RECORD+2);
        timer_minutes=FwGet(LOCAL_RECORD+3);
    }
#endif
    timer_fault=timer_wait=timer_owner=timer_running=local_save=0;
    timer_elapsed=0; timer_at=FwNow(); local_display_at=timer_at-100UL;
    brightness=100; FwSet(LOCAL_LIGHT,100); FwSet(0x0082,0x6464);
    LocalDraft();
}
/* Invoked only when the complete FW save succeeded and the bus is idle. */
static void LocalCommit(void)
{
    uint16_t sum;
    local_save=0;
    if(saved_enable==timer_enabled && saved_minutes==timer_minutes) return;
    sum=LocalChecksum(saved_enable,saved_minutes);
    FwSet(LOCAL_RECORD,0x5343); FwSet(LOCAL_RECORD+1,1);
    FwSet(LOCAL_RECORD+2,saved_enable); FwSet(LOCAL_RECORD+3,saved_minutes);
    FwSet(LOCAL_RECORD+4,sum); FwSet(LOCAL_RECORD+5,(uint16_t)~sum);
#ifndef FW_PROTOCOL_TEST
    DgusToFlash(flashMAIN_BLOCK_ORDER,LOCAL_RECORD,LOCAL_RECORD,6);
    FlashToDgus(flashMAIN_BLOCK_ORDER,LOCAL_RECORD,LOCAL_RECORD,6);
    if(!LocalRecordValid() || FwGet(LOCAL_RECORD+2)!=saved_enable ||
       FwGet(LOCAL_RECORD+3)!=saved_minutes) { FwSet(FW_UI_RESULT_VP,3); return; }
#endif
    timer_elapsed=0; timer_running=0; timer_at=FwNow();
    /* Changing just the interval must not re-arm a failed/submitted command. */
    if(!saved_enable || !timer_enabled) timer_fault=timer_wait=0;
    timer_enabled=saved_enable; timer_minutes=saved_minutes;
}
static void LocalTask(uint32_t now)
{
    uint16_t v=FwGet(LOCAL_LIGHT), mode=FwGet(FW_MIRROR_VP+39);
    uint8_t state, running;
    uint32_t limit=(uint32_t)timer_minutes*60000UL, left, delta=now-timer_at;
    char time_text[9];
    timer_at=now;
    if(v>100) { v=brightness; FwSet(LOCAL_LIGHT,v); }
    if(v!=brightness) { brightness=(uint8_t)v; FwSet(0x0082,(v<<8)|v); }
    running=timer_enabled && !timer_fault && !timer_wait && !offline &&
        Valid(39) && (mode==2 || mode==3);
    if(running && timer_running && timer_elapsed<limit) {
        if(delta>=limit-timer_elapsed) timer_elapsed=limit;
        else timer_elapsed+=delta;
    }
    timer_running=running;
    if(!timer_enabled) state=0;
    else if(timer_fault) state=7;
    else if(timer_wait) state=6;
    else if(offline || !Valid(39)) state=3;
    else if(mode==4) state=8;
    else if(mode!=2 && mode!=3) state=2;
    else if(timer_elapsed<limit) state=1;
    else if(!Valid(82) || !Valid(44)) state=3;
    else if(!FwGet(FW_MIRROR_VP+82)) state=4;
    else if(!(FwGet(FW_MIRROR_VP+44)&128)) state=5;
    else {
        state=9;
        if(!command && !ui_write && !save_count && !local_save && !FwGet(FW_WRITE_TRIGGER_VP)) {
            FwSet(FW_WRITE_ADDR_VP,14); FwSet(FW_WRITE_VALUE_VP,40);
            FwSet(FW_WRITE_TRIGGER_VP,1); timer_owner=timer_wait=1;
            state=6;
        }
    }
    if(now-local_display_at<100UL) return;
    local_display_at=now;
    UiSet(LOCAL_STATE,state);
    left=(limit>timer_elapsed?(limit-timer_elapsed+999UL)/1000UL:0);
    time_text[0]='0'+(uint8_t)(left/36000UL); time_text[1]='0'+(uint8_t)(left/3600UL%10);
    time_text[2]=':'; time_text[3]='0'+(uint8_t)(left/600UL%6);
    time_text[4]='0'+(uint8_t)(left/60UL%10); time_text[5]=':';
    time_text[6]='0'+(uint8_t)(left/10UL%6); time_text[7]='0'+(uint8_t)(left%10);
    time_text[8]=0; UiText(LOCAL_REMAIN,time_text,4);
    UiNumber(0x37A5,FwGet(LOCAL_HOURS),0,0,1,1);
    UiNumber(0x37A6,FwGet(LOCAL_MINS),0,0,1,1);
    UiNumber(0x37AC,brightness,0,0,1,2);
}
static void UiDisplay(void)
{
    uint8_t i, a;
    uint16_t result=FwGet(FW_UI_RESULT_VP);
    /* A previous page's failed action is not a failure to enter this page. */
    if(ui_result_page!=UiLogicalPage()) result=0;
    for(i=0;i<sizeof(live_fields);i++) {
        a=live_fields[i];
        UiNumber(FW_UI_LIVE_VP+i*4,FwGet(FW_MIRROR_VP+a),live_decimals[i],
            a>=30 && a<=32,!offline && DisplayValid(a),4);
    }
    for(i=0;i<sizeof(display_fields);i++) {
        a=display_fields[i];
        UiNumber(FW_UI_EDIT_TEXT_VP+i*3,FwGet(FW_EDIT_VP+a),display_decimals[i],a==5,
            !offline && editing && (edit_valid[a>>3]&(1U<<(a&7))),3);
    }
    UiSet(FW_UI_MODE_VP,(!offline && DisplayValid(39) && FwGet(FW_MIRROR_VP+39)<=6)?FwGet(FW_MIRROR_VP+39):7);
    UiSet(FW_UI_PARTS_VP,(!offline && DisplayValid(44))?FwGet(FW_MIRROR_VP+44):0);
    UiSet(FW_UI_READY_VP,!offline && Valid(16));
    UiSet(FW_UI_STATUS_ICON_VP,offline?4:result);
    UiText(FW_UI_STATUS_VP,offline?"Offline":(result==1?"Saving":
        (result==3?"Failed":(result==2?"OK":"Online"))),8);
}
static void UiAfterReply(uint16_t address, uint16_t value)
{
    if(address==39 && value==4) {
        timer_elapsed=0; timer_wait=0; timer_running=0;
    }
    if(editing) {
        if(!(edit_valid[address>>3] & (1U<<(address&7))) || FwGet(FW_EDIT_VP+address)==edit_base[address])
            FwSet(FW_EDIT_VP+address,value);
        edit_base[address]=value;
        edit_valid[address>>3]|=(uint8_t)(1U<<(address&7));
    }
    if(address==16 && (value==1 || value==42)) operation_page=value;
    if(address==16 && value!=fw_page) {
        fw_page=value;
        if(UiPage(value) && ReadPageId()!=value) SwitchPageById(value);
    }
}

/* FW table 20260914: only non-reserved R/W parameters may be submitted. */
static uint8_t FwWritable(uint16_t address, uint16_t value)
{
    switch(address) {
        case 0: return value >= 3 && value <= 34;
        case 1: return value >= 3 && value <= 60;
        case 4: case 17: case 18:
        case 69: case 70: case 71: case 76: case 79: case 82:
            return value <= 1;
        case 5: return (int16_t)value >= -150 && (int16_t)value <= 150;
        case 8: return value >= 50 && value <= 999;
        case 9: return value <= 999;
        case 11: return value <= 2;
        case 13: case 75: return 1;
        case 14: return value <= 99;
        case 16: return value >= 1 && value <= 99;
        case 19: return value >= 10 && value <= 60;
        case 21: return value <= 50;
        case 72: return value >= 800 && value <= 3000;
        case 73: return value >= 50 && value <= 150;
        case 74: return value >= 20 && value <= 100;
        case 77: return value <= 30000;
        case 80: return value >= 13 && value <= 34;
        case 81: return value >= 90 && value <= 110;
        case 83: return value >= 650 && value <= 750;
        case 85: return value >= 10 && value <= 60;
        default: return 0;
    }
}

static void FwFinish(uint16_t error)
{
    uint8_t i;
    if(timer_owner && command==6) {
        if(error) timer_fault=1;
        timer_owner=0;
    }
    FwSet(FW_LINK_VP, error ? 2 : 1);
    FwSet(FW_ERROR_VP, error);
    if(command == 6) FwSet(FW_WRITE_RESULT_VP, error ? 3 : 2);
    if(error) {
        local_save=0;
        /* Never replay a failed write; recovery performs reads only. */
        refresh_active = refresh_pending = range_index = 0;
        FwInvalidateMirror();
        for(i=0;i<sizeof(display_valid);i++) display_valid[i]=0;
        offline=1; failed_at=FwNow(); save_count=save_index=0;
        FwSet(FW_WRITE_TRIGGER_VP,0);
        if(ui_write) FwSet(FW_UI_RESULT_VP,3);
        ui_write=0; nav_target=0; warning_submit_page=0;
    } else if(command == 6) {
        /* A setting can affect other values: restart from the first range. */
        refresh_active = range_index = 0;
        refresh_pending = 1;
        full_refresh=1;
        if(ui_write) {
            if(editing && request_address!=14 && request_address!=16) edit_base[request_address]=request_value;
            if(nav_target) {
                fw_page=nav_target;
                if(nav_target==1 || nav_target==42) operation_page=nav_target;
                if(ReadPageId()!=nav_target) SwitchPageById(nav_target);
                nav_target=0;
            }
            if(save_count && save_index<save_count) ++save_index;
            if(save_index>=save_count) { save_count=save_index=0; if(local_save) local_save=2; FwSet(FW_UI_RESULT_VP,2); }
            ui_write=0;
        }
    } else if(range_index == (full_refresh?sizeof(ranges)/sizeof(ranges[0]):sizeof(live_ranges)/sizeof(live_ranges[0]))) {
        refresh_active = 0;
        refreshed_at=FwNow(); offline=0;
    }
    command = 0;
    UiDisplay();
}

static void FwReply(uint8_t *frame, uint8_t length)
{
    uint16_t i, value;
    if(!command || frame[0] != FW_ADDRESS) return;
    if(FwNow() - sent_at >= FW_TIMEOUT) return;
    if(frame[1] == (command | 0x80) && length == 5) {
        if(frame[2]) FwFinish(frame[2]);
        return;
    }
    if(frame[1] != command) return;
    if(command == 3 && frame[2] == request_count * 2 && length == frame[2] + 5) {
        for(i = 0; i < request_count; ++i) {
            value = ((uint16_t)frame[3 + i * 2] << 8) | frame[4 + i * 2];
            FwSet(FW_MIRROR_VP + request_address + i, value);
            mirror_valid[(request_address + i) >> 3] |= (uint8_t)(1U << ((request_address + i) & 7));
            display_valid[(request_address + i) >> 3] |= (uint8_t)(1U << ((request_address + i) & 7));
            UiAfterReply(request_address+i,value);
        }
        FwFinish(0);
    } else if(command == 6 && length == 8 &&
              (((uint16_t)frame[2] << 8) | frame[3]) == request_address &&
              (((uint16_t)frame[4] << 8) | frame[5]) == request_value) {
        FwSet(FW_MIRROR_VP + request_address, request_value);
        mirror_valid[request_address >> 3] |= (uint8_t)(1U << (request_address & 7));
        display_valid[request_address >> 3] |= (uint8_t)(1U << (request_address & 7));
        FwFinish(0);
    }
}

/* Persistent UART2 assembler; never interpret an 06 echo as an 03 response. */
void FwProtocolFeed(uint8_t *bytes, uint16_t length)
{
    uint16_t index, crc;
    uint8_t needed, drop, i;
    uint32_t now = FwNow();
    if(now - last_rx >= FW_TIMEOUT) rx_size = 0;
    last_rx = now;
    for(index = 0; index < length; ++index) {
        if(rx_size == sizeof(rx)) rx_size = 0;
        rx[rx_size++] = bytes[index];
        while(rx_size) {
            drop = 0;
            if(rx[0] != FW_ADDRESS) drop = 1;
            else if(rx_size < 2) break;
            else {
                if(rx[1] == 3) {
                    if(rx_size < 3) break;
                    needed = rx[2] + 5;
                    if(rx[2] == 0 || rx[2] > sizeof(rx) - 5 || (rx[2] & 1)) drop = 1;
                } else if(rx[1] == 6) needed = 8;
                else if(rx[1] == 0x83 || rx[1] == 0x86) needed = 5;
                else drop = 1;
                if(!drop) {
                    if(rx_size < needed) break;
                    crc = crc_16(rx, needed - 2);
                    if(rx[needed - 2] == (uint8_t)crc && rx[needed - 1] == (uint8_t)(crc >> 8)) {
                        FwReply(rx, needed);
                        drop = needed;
                    } else drop = 1;
                }
            }
            for(i = drop; i < rx_size; ++i) rx[i - drop] = rx[i];
            rx_size -= drop;
        }
    }
}

void FwUsbProtocol(UART_TYPE *uart, uint8_t *frame, uint16_t length)
{
#if sysBEAUTY_MODE_ENABLED || sysN5CAMERA_MODE_ENABLED || sysADVERTISE_MODE_ENABLED
    if(uart != &Uart_R11 || length != 6 || frame[0] != 0xAA || frame[1] != 0x55 ||
       frame[2] != 0 || frame[3] != 2 || frame[4] != 0xFB) return;
    switch(frame[5]) {
        case 0:
            video_ready = software_ready = 0;
            FwSet(USB_VIDEO_VP, 0); FwSet(USB_SOFTWARE_VP, 0);
            FwSet(USB_COMPLETE_VP, 0);
            FwSet(USB_VIDEO_GO_VP, 0); FwSet(USB_SOFTWARE_GO_VP, 0);
            break;
        case 1:
            if(!video_ready) FwSet(USB_VIDEO_GO_VP, 0);
            video_ready = 1; FwSet(USB_VIDEO_VP, 1); FwSet(USB_COMPLETE_VP, 0);
            break;
        case 2:
            if(!software_ready) FwSet(USB_SOFTWARE_GO_VP, 0);
            software_ready = 1; FwSet(USB_SOFTWARE_VP, 1);
            break;
        case 3:
            video_ready = 0;
            FwSet(USB_VIDEO_VP, 0); FwSet(USB_VIDEO_GO_VP, 0); FwSet(USB_COMPLETE_VP, 1);
            break;
        default: break;
    }
#endif
}

void FwProtocolInit(void)
{
    uint16_t i;
    command = rx_size = range_index = video_ready = software_ready = 0;
    refresh_active = 0;
    refresh_pending = 1;
    last_page = ReadPageId();
    ui_result_page=last_page;
    sent_at = last_rx = FwNow();
    FwInvalidateMirror();
    for(i=0;i<sizeof(display_valid);i++) display_valid[i]=0;
    full_refresh=1; offline=1; refreshed_at=failed_at=alarm_at=edit_at=FwNow();
    save_count=save_index=ui_write=alarm_cursor=0; editing=(last_page==2 || last_page==27);
    fw_page=0xffff; return_page=42; operation_page=42; nav_target=warning_submit_page=0;
    for(i=0;i<sizeof(edit_valid);i++) edit_valid[i]=0;
    for(i = FW_WRITE_ADDR_VP; i <= FW_ERROR_VP; ++i) FwSet(i, 0);
    for(i = USB_VIDEO_VP; i <= USB_SOFTWARE_GO_VP; ++i) FwSet(i, 0);
    for(i = 0; i <= 88; ++i) FwSet(FW_MIRROR_VP + i, 0);
    for(i=FW_UI_EVENT_VP;i<FW_UI_LIVE_VP;i++) FwSet(i,0);
    LocalInit();
    UiDisplay();
}

/* Display one defined alarm at a time. Unknown bits never acquire a command. */
static code uint8_t alarm_bits[] = {0,1,2,4,6,7,13,14,16,17,21,22,24,25,26,28,29,30};
static code uint8_t alarm_keys[] = {7,7,7,7,7,7,7,7,0,0,0,0,0,0,0,42,44,43};
static void UiAlarm(uint32_t now)
{
    uint8_t i,b,index;
    uint16_t bits, current=FwGet(FW_UI_ALARM_VP);
    if(offline || !DisplayValid(41) || !DisplayValid(42)) { UiSet(FW_UI_ALARM_VP,0); return; }
    /* The dwell time applies only while the displayed alarm is still active.
     * An acknowledged clear is not proof: wait for the actual FW alarm bits. */
    if(current && current<=sizeof(alarm_bits)) {
        b=alarm_bits[current-1];
        if((FwGet(FW_MIRROR_VP+41+(b>=16))&(1U<<(b&15))) && now-alarm_at<3000UL) return;
    }
    alarm_at=now;
    for(i=0;i<sizeof(alarm_bits);i++) {
        index=(alarm_cursor+i)%sizeof(alarm_bits); b=alarm_bits[index];
        bits=FwGet(FW_MIRROR_VP+41+(b>=16));
        if(bits & (1U<<(b&15))) {
            UiSet(FW_UI_ALARM_VP,index+1);
            alarm_cursor=(index+1)%sizeof(alarm_bits); return;
        }
    }
    UiSet(FW_UI_ALARM_VP,0);
}
static void UiSubmit(uint16_t address,uint16_t value)
{
    ui_result_page=UiLogicalPage();
    FwSet(FW_WRITE_ADDR_VP,address); FwSet(FW_WRITE_VALUE_VP,value);
    FwSet(FW_WRITE_TRIGGER_VP,1); FwSet(FW_UI_RESULT_VP,1); ui_write=1;
}
static void UiEvent(void)
{
    uint16_t e=FwGet(FW_UI_EVENT_VP), p=ReadPageId(), v, target;
    uint8_t i,a,n,good;
    if(!e) return;
    FwSet(FW_UI_EVENT_VP,0);
    if((e>>8)!=1 && (e>>8)!=2 && (e<0x0300 || e>0x0303) && e!=0x0400 && e!=0x0401) return;
    if(!ui_write && !save_count && !FwGet(FW_WRITE_TRIGGER_VP)) ui_result_page=p;
    if(e==0x0400) { SwitchPageById(120); return; }
    if(e==0x0401) { SwitchPageById(9); return; }
    if(e==0x0301 && !ui_write && !save_count) {
        LocalDraft();
        for(i=0;i<89;i++) if(edit_valid[i>>3] & (1U<<(i&7))) FwSet(FW_EDIT_VP+i,edit_base[i]);
        e=0x0200|return_page;
    }
    if(offline || !Valid(16) || ui_write || save_count || command==6 || FwGet(FW_WRITE_TRIGGER_VP)) {
        if(!ui_write && !save_count) FwSet(FW_UI_RESULT_VP,3);
        return;
    }
    if((e>>8)==1) {
        v=e&255;
        if((v==38 && !(FwGet(FW_MIRROR_VP+44)&4)) ||
           (v==40 && !(FwGet(FW_MIRROR_VP+44)&128))) { FwSet(FW_UI_RESULT_VP,3); return; }
        if(UiKey(v)) UiSubmit(14,v); else FwSet(FW_UI_RESULT_VP,3);
    } else if((e>>8)==2) {
        target=e&255;
        if(target==0) target=UiWarningPage(p)?operation_page:return_page;
        if(UiPage(target)) { if(p!=2 && p!=27) return_page=p; nav_target=target; UiSubmit(16,target); }
    } else if(e==0x0300 && editing) {
        n=p==2?sizeof(basic_fields):sizeof(advanced_fields);
        good=1; save_count=save_index=0;
        local_save=0;
        if(p==2) {
            v=FwGet(LOCAL_ENABLE); target=FwGet(LOCAL_HOURS);
            if(v>1 || target>24 || FwGet(LOCAL_MINS)>59) good=0;
            saved_enable=(uint8_t)v;
            saved_minutes=target*60+FwGet(LOCAL_MINS);
            if(saved_enable && !saved_minutes) good=0;
        }
        for(i=0;i<n;i++) {
            a=p==2?basic_fields[i]:advanced_fields[i]; v=FwGet(FW_EDIT_VP+a);
            if(!(edit_valid[a>>3] & (1U<<(a&7))) || (v!=edit_base[a] && !FwWritable(a,v))) good=0;
            if(v!=edit_base[a]) { save_addresses[save_count]=a; save_values[save_count++]=v; }
        }
        if(!good) { save_count=0; FwSet(FW_UI_RESULT_VP,3); }
        else {
            if(p==2) local_save=save_count?1:2;
            FwSet(FW_UI_RESULT_VP,save_count?1:2);
        }
    } else if(e==0x0303) {
        n=UiWarningAlarm(p,&a,&i);
        if(n && Valid(a) && (FwGet(FW_MIRROR_VP+a)&(1U<<i))) { UiSubmit(14,n); warning_submit_page=p; }
        else FwSet(FW_UI_RESULT_VP,3);
    } else if(e==0x0302) {
        v=FwGet(FW_UI_ALARM_VP);
        if(v && v<=sizeof(alarm_bits)) {
            a=alarm_bits[v-1];
            if(alarm_keys[v-1] && Valid(41+(a>=16)) && (FwGet(FW_MIRROR_VP+41+(a>=16)) & (1U<<(a&15)))) UiSubmit(14,alarm_keys[v-1]);
        }
    }
}

void FwProtocolTask(void)
{
    uint16_t trigger, address, value, page;
    uint8_t alarm_reg,alarm_bit,alarm_key;
    uint32_t now = FwNow();
#if sysBEAUTY_MODE_ENABLED || sysN5CAMERA_MODE_ENABLED || sysADVERTISE_MODE_ENABLED
    uint8_t fb[6] = {0xAA, 0x55, 0, 2, 0xFB, 1};
    trigger = FwGet(USB_VIDEO_GO_VP);
    if(trigger) {
        FwSet(USB_VIDEO_GO_VP, 0);
        if(trigger == 1 && video_ready) UartSendData(&Uart_R11, fb, 6);
    }
    trigger = FwGet(USB_SOFTWARE_GO_VP);
    if(trigger) {
        FwSet(USB_SOFTWARE_GO_VP, 0);
        fb[5] = 2;
        if(trigger == 1 && software_ready) UartSendData(&Uart_R11, fb, 6);
    }
#endif
    if(command && now - sent_at >= FW_TIMEOUT) {
        FwFinish(FW_ERR_TIMEOUT);
        rx_size = 0;
    }
#if sysADVERTISE_MODE_ENABLED
    R11VideoTouchTask(!ui_write && !save_count && command!=6 && !FwGet(FW_WRITE_TRIGGER_VP));
#endif
    page = UiLogicalPage();
    if(page != last_page) {
        last_page = page;
        /* Local overlays/player do not trigger extra business reads. */
        if(!offline && (page==2 || page==27)) refresh_pending = 1;
        editing=(page==2 || page==27);
        if(page==2) LocalDraft();
        /* Clear completed, page-local feedback; keep active operation ownership. */
        if(!ui_write && !save_count) { FwSet(FW_UI_RESULT_VP,0); ui_result_page=page; }
        if(editing) {
            for(address=0;address<sizeof(edit_valid);address++) edit_valid[address]=0;
        }
    }
#if sysADVERTISE_MODE_ENABLED
    if(R11VideoIdleActive() || R11VideoWakeBlocked()) FwSet(FW_UI_EVENT_VP,0);
    else
#endif
    UiEvent();
    if(local_save==2 && !command) LocalCommit();
    UiAlarm(now);
    if(now-edit_at>=100UL) { edit_at=now; UiDisplay(); }
    if(!ui_write && save_count && save_index<save_count && !command)
        UiSubmit(save_addresses[save_index],save_values[save_index]);
    LocalTask(FwNow());
    trigger = FwGet(FW_WRITE_TRIGGER_VP);
    if(trigger && command != 3) {
        FwSet(FW_WRITE_TRIGGER_VP, 0);
        address = FwGet(FW_WRITE_ADDR_VP);
        value = FwGet(FW_WRITE_VALUE_VP);
        if(warning_submit_page) {
            alarm_key=UiWarningAlarm(warning_submit_page,&alarm_reg,&alarm_bit);
            if(page!=warning_submit_page || !alarm_key || !Valid(alarm_reg) ||
               !(FwGet(FW_MIRROR_VP+alarm_reg)&(1U<<alarm_bit))) {
                warning_submit_page=0;ui_write=0;
                FwSet(FW_UI_RESULT_VP,3);FwSet(FW_WRITE_RESULT_VP,3);return;
            }
            warning_submit_page=0;
        }
        if(command || offline || trigger != 1 || !FwWritable(address, value)) {
            if(timer_owner) { timer_owner=0; timer_fault=1; }
            local_save=0;
            FwSet(FW_WRITE_RESULT_VP, 3);
            FwSet(FW_ERROR_VP, command ? FW_ERR_BUSY : FW_ERR_VALUE);
            if(ui_write) { ui_write=0; save_count=save_index=0; nav_target=0; FwSet(FW_UI_RESULT_VP,3); }
        } else if(address != 14 &&
                  (mirror_valid[address >> 3] & (uint8_t)(1U << (address & 7))) &&
                  FwGet(FW_MIRROR_VP + address) == value) {
            /* Ordinary same-value submissions succeed without bus traffic. */
            FwSet(FW_WRITE_RESULT_VP, 2);
            FwSet(FW_ERROR_VP, 0);
            if(ui_write) {
                if(nav_target) {
                    fw_page=nav_target;
                    if(nav_target==1 || nav_target==42) operation_page=nav_target;
                    if(ReadPageId()!=nav_target) SwitchPageById(nav_target);
                    nav_target=0;
                }
                if(save_count) ++save_index;
                if(save_index>=save_count) { save_count=save_index=0; if(local_save) local_save=2; FwSet(FW_UI_RESULT_VP,2); }
                ui_write=0;
            }
        } else {
            request_address = address; request_value = value;
            /* Related settings may change before the post-write readback. */
            FwInvalidateMirror();
            command = 6; sent_at = now; rx_size = 0;
            FwSet(FW_WRITE_RESULT_VP, 1);
            SendModbusWriteSingleRegisterFrame(&Uart2, FW_ADDRESS, address, value);
        }
        return;
    }
    if(command) return;
    if(!refresh_active) {
        if(!refresh_pending) {
            if(offline) { if(now-failed_at<3000UL) return; }
            else if(now-refreshed_at<1000UL) return;
            full_refresh=offline;
        } else full_refresh=1;
        refresh_pending = 0;
        refresh_active = 1;
        range_index = 0;
    }
    request_address = full_refresh?ranges[range_index].first:live_ranges[range_index].first;
    request_count = full_refresh?ranges[range_index].count:live_ranges[range_index].count;
    ++range_index;
    command = 3; sent_at = now; rx_size = 0;
    SendModbusReadHoldingRegistersFrame(&Uart2, FW_ADDRESS, request_address, request_count);
}

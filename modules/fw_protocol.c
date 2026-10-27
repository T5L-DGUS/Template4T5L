#ifdef FW_PROTOCOL_TEST
#include "fw_test_platform.h"
#else
/* Keep the UI state machine within the existing T5L code-memory banks. */
#pragma OPTIMIZE(8,SIZE)
#include "fw_protocol.h"
#include "modbus.h"
#include "timer.h"
#if sysADVERTISE_MODE_ENABLED
#include "r11_common.h"
#endif
#endif

/* FW uses Modbus on UART4. VP values preserve wire units. */
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
    {13,1}, {16,1}, {30,3}, {34,4}, {39,1}, {41,4}, {50,2}, {53,1}, {82,1}
};
static code uint8_t basic_fields[] = {4,5};
static code uint8_t advanced_fields[] = {0,1,8,9,11,17,70,76,80,81,82,83,85};
static code uint8_t advanced2_fields[] = {18,19,21,69,71,72,73,74,75,77,79};
static code uint8_t advanced2_decimals[] = {0,0,1,0,0,0,1,1,0,0,0};
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
static uint16_t hardness_seen, hardness_limit_seen;
static uint8_t hardness_ready;
static uint16_t temp_low_seen, temp_high_seen;
static uint8_t temp_limits_ready;
static uint8_t xdata pass_enabled, pass_set, pass_authorized, pass_mode, pass_length;
static uint8_t xdata pass_draft_set, pass_saved_enable, pass_saved_set, pass_notify, force_page_write;
static uint32_t xdata pass_value, pass_draft, pass_saved, pass_input, pass_first;
static uint16_t xdata pass_origin, pass_target, start_submit_page;
static uint32_t xdata pass_touch_at, pass_guard_at;
static uint8_t xdata pass_touch_down, pass_touch_guard;
/* Preserve the original home action until its actual bus submission. */
static uint16_t xdata home_event, home_target;
static void UiSubmit(uint16_t address,uint16_t value);
static void PassDraft(void);
static void PassInit(void);
static uint8_t PassCommit(void);
static void PassOpen(uint8_t mode,uint16_t target,uint8_t notify);
static void UiNavigate(uint16_t target);

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
static uint8_t UiSettingsPage(uint16_t p) { return p==2 || p==27 || p==FW_UI_ADV2_PAGE; }
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
static uint8_t UiFormatNumber(char *buf, uint16_t value, uint8_t decimals, uint8_t sign, uint8_t valid)
{
    char rev[8];
    uint8_t n=0, j=0, neg=0;
    uint16_t mag=value;
    if(!valid) { buf[0]='-'; buf[1]='-'; buf[2]=0; return 2; }
    if(sign && (int16_t)value<0) { neg=1; mag=(uint16_t)(-(int16_t)value); }
    do { rev[n++]=(char)('0'+mag%10); mag/=10; } while(mag || n<=decimals);
    if(neg) buf[j++]='-';
    while(n) { buf[j++]=rev[--n]; if(decimals && n==decimals) buf[j++]='.'; }
    buf[j]=0; return j;
}
static void UiNumber(uint16_t vp, uint16_t value, uint8_t decimals, uint8_t sign, uint8_t valid, uint8_t words)
{
    char buf[10];
    UiFormatNumber(buf,value,decimals,sign,valid); UiText(vp,buf,words);
}
static void UiCylinderTemperature(void)
{
    char buf[17];
    uint8_t n;
    n=UiFormatNumber(buf,FwGet(FW_MIRROR_VP+30),1,1,!offline && DisplayValid(30));
    buf[n++]='/';
    UiFormatNumber(buf+n,FwGet(FW_MIRROR_VP+5),1,1,!offline && DisplayValid(5));
    UiText(FW_UI_CYLINDER_TEXT_VP,buf,8);
}
/* FW6/FW30 pages are the running view; keep FW16 unchanged on the bus. */
static uint16_t UiRunPage(void)
{
    return !offline && Valid(30) && (int16_t)FwGet(FW_MIRROR_VP+30)<-30?30:6;
}
static void UiRunPageTask(void)
{
    uint16_t p=ReadPageId(), target;
    if((fw_page!=6 && fw_page!=30) || (p!=6 && p!=30)) return;
    /* An in-flight write invalidates control data but must not flicker the view. */
    if(!offline && !Valid(30)) return;
    target=UiRunPage();
    if(p!=target) SwitchPageById(target);
}
static void LocalDraft(void)
{
    FwSet(LOCAL_ENABLE,timer_enabled);
    FwSet(LOCAL_HOURS,timer_minutes/60);
    FwSet(LOCAL_MINS,timer_minutes%60);
    PassDraft();
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
#endif
    if(LocalRecordValid()) {
        timer_enabled=(uint8_t)FwGet(LOCAL_RECORD+2);
        timer_minutes=FwGet(LOCAL_RECORD+3);
    }
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
    if(!PassCommit()) { FwSet(FW_UI_RESULT_VP,3); return; }
    if(saved_enable==timer_enabled && saved_minutes==timer_minutes) return;
    sum=LocalChecksum(saved_enable,saved_minutes);
    FwSet(LOCAL_RECORD,0x5343); FwSet(LOCAL_RECORD+1,1);
    FwSet(LOCAL_RECORD+2,saved_enable); FwSet(LOCAL_RECORD+3,saved_minutes);
    FwSet(LOCAL_RECORD+4,sum); FwSet(LOCAL_RECORD+5,(uint16_t)~sum);
#ifndef FW_PROTOCOL_TEST
    DgusToFlash(flashMAIN_BLOCK_ORDER,LOCAL_RECORD,LOCAL_RECORD,6);
    FlashToDgus(flashMAIN_BLOCK_ORDER,LOCAL_RECORD,LOCAL_RECORD,6);
#endif
    if(!LocalRecordValid() || FwGet(LOCAL_RECORD+2)!=saved_enable ||
       FwGet(LOCAL_RECORD+3)!=saved_minutes) { FwSet(FW_UI_RESULT_VP,3); return; }
    timer_elapsed=0; timer_running=0; timer_at=FwNow();
    /* Changing just the interval must not re-arm a failed/submitted command. */
    if(!saved_enable || !timer_enabled) timer_fault=timer_wait=0;
    timer_enabled=saved_enable; timer_minutes=saved_minutes;
}
/* 0 failed, 1 satisfied, 2 not yet known. Temperature units are 0.1 C. */
static uint8_t UiCondition(uint8_t item)
{
    if(offline) return 2;
    if(item==0) {
        if((Valid(30) && (int16_t)FwGet(FW_MIRROR_VP+30)<0) ||
           (Valid(37) && FwGet(FW_MIRROR_VP+37)>0)) return 1;
        return Valid(30) && Valid(37)?0:2;
    }
    if(item==1) {
        if(!Valid(41) || !Valid(42)) return 2;
        return !(FwGet(FW_MIRROR_VP+41)&0x60D7) && !(FwGet(FW_MIRROR_VP+42)&0x7763);
    }
    if(!Valid(30)) return 2;
    return (int16_t)FwGet(FW_MIRROR_VP+30)<20;
}
/* Reasons share the same icon table on both confirmation pages. */
static uint8_t UiCommandReason(uint16_t key)
{
    uint8_t c;
    uint16_t mode;
    if(key==4 || key==50 || key==3 || key==38) {
        if(offline || !Valid(39)) return 1;
        mode=FwGet(FW_MIRROR_VP+39);
        if(key==38) return mode==3?0:6;
        if(!Valid(41)) return 1;
        if(FwGet(FW_MIRROR_VP+41)) return 3;
        if(key==3) return mode==0 || mode==3?0:6;
        if(key==4 && mode!=0 && mode!=2 && mode!=3) return 6;
        if(key==50) {
            if(mode!=0) return 6;
            if(!Valid(30)) return 1;
            if((int16_t)FwGet(FW_MIRROR_VP+30)>=100) return 4;
        }
    }
    if(key==1 || key==7 || key==42 || key==43 || key==44) return 0;
    if(offline || !Valid(42)) return 1;
    if(FwGet(FW_MIRROR_VP+42)&1) return 5;
    if(key!=40 && key!=51) return 0;
    c=UiCondition(0); if(c!=1) return c==2?1:2;
    c=UiCondition(1); if(c!=1) return c==2?1:3;
    if(key==51) { c=UiCondition(2); if(c!=1) return c==2?1:4; }
    if(key==40 && (!Valid(44) || !(FwGet(FW_MIRROR_VP+44)&128))) return Valid(44)?6:1;
    return 0;
}
/* target=0 means a machine key; nonzero means navigation only. */
static uint8_t UiHomeAction(uint16_t e, uint16_t *target)
{
    uint16_t mode;
    *target=0;
    if(e==0x0227 || e==0x0203 || e==0x0103 || e==0x0126) {
        if(offline || !Valid(39)) return 0;
        mode=FwGet(FW_MIRROR_VP+39);
        if(e==0x0227 || e==0x0203) {
            if(!Valid(82) || FwGet(FW_MIRROR_VP+82)!=1) return 0;
            if(mode==0) *target=e==0x0227?39:3;
            else if(e==0x0227 && mode==4) *target=40;
            else if(e==0x0203 && mode==6) *target=4;
            return *target!=0;
        }
        if(mode==2) { *target=12; return 1; }
        if(e==0x0126) {
            if(mode==4) { *target=40; return 1; }
            if(mode==6) { *target=4; return 1; }
        }
    }
    if((e>>8)==1) return !UiCommandReason(e&255);
    return 1;
}
static void UiCancelAction(void)
{
    FwSet(FW_WRITE_TRIGGER_VP,0);
    ui_write=0; nav_target=0; start_submit_page=0; force_page_write=0;
    home_event=home_target=0;
    FwSet(FW_UI_RESULT_VP,3); FwSet(FW_WRITE_RESULT_VP,3);
}
static void PassMask(void)
{
    char s[7]; uint8_t i;
    for(i=0;i<pass_length;i++) s[i]='*';
    s[i]=0; UiText(FW_UI_PASS_MASK,s,4);
}
static void PassDraft(void)
{
    pass_draft=pass_value; pass_draft_set=pass_set;
    FwSet(FW_UI_PASS_ENABLE,pass_enabled); FwSet(FW_UI_PASS_SET,pass_set);
}
static uint16_t PassChecksum(uint16_t e,uint16_t set,uint32_t value)
{
    return (uint16_t)(0xC615 ^ e ^ (set<<8) ^ (uint16_t)value ^ (uint16_t)(value>>16));
}
static uint8_t PassRecordValid(void)
{
    uint16_t e=FwGet(FW_UI_PASS_RECORD+2), s=FwGet(FW_UI_PASS_RECORD+3), sum;
    uint32_t v=((uint32_t)FwGet(FW_UI_PASS_RECORD+4)<<16)|FwGet(FW_UI_PASS_RECORD+5);
    sum=PassChecksum(e,s,v);
    return FwGet(FW_UI_PASS_RECORD)==0x5057 && FwGet(FW_UI_PASS_RECORD+1)==1 &&
        e<=1 && s<=1 && (!e || s) && v<=999999UL &&
        FwGet(FW_UI_PASS_RECORD+6)==sum && FwGet(FW_UI_PASS_RECORD+7)==(uint16_t)~sum;
}
static void PassInit(void)
{
    pass_enabled=pass_set=pass_authorized=pass_mode=pass_length=0;
    pass_value=pass_input=pass_first=0; force_page_write=0; start_submit_page=0;
    pass_touch_at=pass_guard_at=FwNow(); pass_touch_down=pass_touch_guard=0;
#ifndef FW_PROTOCOL_TEST
    FlashToDgus(flashMAIN_BLOCK_ORDER,FW_UI_PASS_RECORD,FW_UI_PASS_RECORD,8);
#endif
    if(PassRecordValid()) {
        pass_enabled=(uint8_t)FwGet(FW_UI_PASS_RECORD+2); pass_set=(uint8_t)FwGet(FW_UI_PASS_RECORD+3);
        pass_value=((uint32_t)FwGet(FW_UI_PASS_RECORD+4)<<16)|FwGet(FW_UI_PASS_RECORD+5);
    }
    PassDraft(); PassMask(); FwSet(FW_UI_PASS_STATE,0);
}
static uint8_t PassCommit(void)
{
    uint16_t sum;
    if(pass_saved_enable==pass_enabled && pass_saved_set==pass_set && pass_saved==pass_value) return 1;
    sum=PassChecksum(pass_saved_enable,pass_saved_set,pass_saved);
    FwSet(FW_UI_PASS_RECORD,0x5057); FwSet(FW_UI_PASS_RECORD+1,1);
    FwSet(FW_UI_PASS_RECORD+2,pass_saved_enable); FwSet(FW_UI_PASS_RECORD+3,pass_saved_set);
    FwSet(FW_UI_PASS_RECORD+4,(uint16_t)(pass_saved>>16)); FwSet(FW_UI_PASS_RECORD+5,(uint16_t)pass_saved);
    FwSet(FW_UI_PASS_RECORD+6,sum); FwSet(FW_UI_PASS_RECORD+7,(uint16_t)~sum);
#ifndef FW_PROTOCOL_TEST
    DgusToFlash(flashMAIN_BLOCK_ORDER,FW_UI_PASS_RECORD,FW_UI_PASS_RECORD,8);
    FlashToDgus(flashMAIN_BLOCK_ORDER,FW_UI_PASS_RECORD,FW_UI_PASS_RECORD,8);
#endif
    if(!PassRecordValid() || FwGet(FW_UI_PASS_RECORD+2)!=pass_saved_enable ||
       FwGet(FW_UI_PASS_RECORD+3)!=pass_saved_set ||
       FwGet(FW_UI_PASS_RECORD+4)!=(uint16_t)(pass_saved>>16) ||
       FwGet(FW_UI_PASS_RECORD+5)!=(uint16_t)pass_saved) return 0;
    if(!pass_enabled) { pass_authorized=1; pass_touch_at=FwNow(); }
    pass_enabled=pass_saved_enable; pass_set=pass_saved_set; pass_value=pass_saved;
    /* Saving within an authenticated settings session does not re-lock it. */
    return 1;
}
static void PassOpen(uint8_t mode,uint16_t target,uint8_t notify)
{
    pass_origin=ReadPageId(); if(pass_origin==0 || pass_origin==1 || pass_origin==42 || pass_origin==FW_UI_PASS_PAGE) pass_origin=1;
    if(mode==1) { target=42; pass_origin=1; }
    pass_mode=mode; pass_target=target; pass_notify=notify; pass_length=0; pass_input=pass_first=0;
    FwSet(FW_UI_PASS_STATE,mode==1?0:1); PassMask(); FwSet(FW_UI_EVENT_VP,0);
    SwitchPageById(FW_UI_PASS_PAGE);
}
static void UiNavigate(uint16_t target)
{
    if(target==1) target=42;
    if(target==42 && pass_enabled && !pass_authorized) {
        if(ReadPageId()==FW_UI_PASS_PAGE && pass_mode==1) return;
        target=1;
    }
    if(target==6 || target==30) target=UiRunPage();
    pass_mode=0; pass_input=pass_first=0; pass_length=0; PassMask();
    if(ReadPageId()!=target) SwitchPageById(target);
}
static void PassEvent(uint16_t e)
{
    if(ui_write || save_count || command==6 || FwGet(FW_WRITE_TRIGGER_VP)) return;
    if(e==0x0500 && ReadPageId()==2) { PassOpen(2,2,0); return; }
    if(ReadPageId()!=FW_UI_PASS_PAGE || !pass_mode) return;
    if(e>=0x0510 && e<=0x0519 && pass_length<6) { pass_input=pass_input*10+(e&15); ++pass_length; }
    else if(e==0x0520) { if(pass_length) { pass_input/=10; --pass_length; } }
    else if(e==0x0522) { pass_mode=0; pass_input=pass_first=0; pass_length=0; SwitchPageById(pass_origin); }
    else if(e==0x0521) {
        if(pass_length!=6) { FwSet(FW_UI_PASS_STATE,5); return; }
        if(pass_mode==2) { pass_first=pass_input; pass_mode=3; FwSet(FW_UI_PASS_STATE,2); }
        else if(pass_mode==3) {
            if(pass_input!=pass_first) { pass_mode=2; FwSet(FW_UI_PASS_STATE,4); }
            else { pass_draft=pass_input; pass_draft_set=1; FwSet(FW_UI_PASS_SET,1); pass_mode=0; SwitchPageById(2); }
        } else if(pass_input!=pass_value) FwSet(FW_UI_PASS_STATE,3);
        else if(offline || !Valid(16)) { FwSet(FW_UI_PASS_STATE,6); return; }
        else {
            pass_authorized=1; pass_touch_at=FwNow();
            if(pass_notify) { nav_target=pass_target; force_page_write=1; UiSubmit(16,pass_target); }
            else UiNavigate(pass_target);
        }
        pass_input=0; pass_length=0;
    }
    PassMask();
}
/* The FW task owns TP_Status. Video receives the same sampled event. */
static void PassTouchTask(uint16_t touch,uint32_t now)
{
    uint16_t p=ReadPageId();
    uint8_t state=(uint8_t)touch, updated=(touch>>8)==0x5a && state<=3;
    if(updated) {
        pass_touch_down=state==1 || state==3;
        pass_touch_at=now;
    }
    if(pass_touch_down) pass_touch_at=now;
    if(pass_enabled && pass_authorized && now-pass_touch_at>=FW_UI_PASS_TIMEOUT_MS) {
        pass_authorized=0;
        if(p==42) {
            if(home_event && command!=6) UiCancelAction();
            FwSet(FW_UI_EVENT_VP,0); FwSet(0x0600,0);
            SwitchPageById(1); p=1;
        }
    }
    if(pass_touch_guard) {
        FwSet(FW_UI_EVENT_VP,0); FwSet(0x0600,0);
        if(pass_touch_down) pass_guard_at=now;
        else if(now-pass_guard_at>=200UL) pass_touch_guard=0;
        return;
    }
    if(p==1) {
        FwSet(FW_UI_EVENT_VP,0); FwSet(0x0600,0);
        if(!pass_enabled || pass_authorized) UiNavigate(42);
        else if(updated && state==1) {
            PassOpen(1,42,1); pass_touch_guard=1; pass_guard_at=now;
        }
    } else if(p==42 && pass_enabled && !pass_authorized) UiNavigate(42);
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
    else if(UiCommandReason(40)) state=5;
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
    uint8_t i, a, parts_valid=!offline && DisplayValid(44);
    uint16_t parts=FwGet(FW_MIRROR_VP+44);
    uint16_t result=FwGet(FW_UI_RESULT_VP);
    uint8_t reason=UiCommandReason(UiLogicalPage()==3?51:40);
    for(i=0;i<3;i++) UiSet(FW_UI_CHECK_VP+i,UiCondition(i));
    UiSet(FW_UI_LID_VP,!offline && DisplayValid(42) && (FwGet(FW_MIRROR_VP+42)&1));
    UiSet(FW_UI_START_VP,!reason);
    UiSet(FW_UI_REASON_VP,reason);
    /* A previous page's failed action is not a failure to enter this page. */
    if(ui_result_page!=UiLogicalPage()) result=0;
    for(i=0;i<sizeof(live_fields);i++) {
        a=live_fields[i];
        UiNumber(FW_UI_LIVE_VP+i*4,FwGet(FW_MIRROR_VP+a),live_decimals[i],
            a>=30 && a<=32,!offline && DisplayValid(a),4);
    }
    UiCylinderTemperature();
    for(i=0;i<sizeof(display_fields);i++) {
        a=display_fields[i];
        UiNumber(FW_UI_EDIT_TEXT_VP+i*3,FwGet(FW_EDIT_VP+a),display_decimals[i],a==5,
            !offline && editing && (edit_valid[a>>3]&(1U<<(a&7))),3);
    }
    for(i=0;i<sizeof(advanced2_fields);i++) {
        a=advanced2_fields[i];
        UiNumber(FW_UI_ADV2_TEXT_VP+i*3,FwGet(FW_EDIT_VP+a),advanced2_decimals[i],0,
            !offline && editing && (edit_valid[a>>3]&(1U<<(a&7))),3);
        if(a==18 || a==69 || a==71 || a==79)
            UiSet(0x37EA+(a==18?0:(a==69?1:(a==71?2:3))),
                !offline && editing && (edit_valid[a>>3]&(1U<<(a&7))) && FwGet(FW_EDIT_VP+a)<=1?FwGet(FW_EDIT_VP+a):2);
    }
    UiNumber(0x37E1,FwGet(FW_MIRROR_VP+43),2,0,!offline && DisplayValid(43),3);
    UiNumber(0x37E4,FwGet(FW_MIRROR_VP+44),0,0,!offline && DisplayValid(44),3);
    UiNumber(0x37E7,FwGet(FW_MIRROR_VP+53),1,1,!offline && DisplayValid(53),3);
    UiSet(FW_UI_MODE_VP,(!offline && DisplayValid(39) && FwGet(FW_MIRROR_VP+39)<=6)?FwGet(FW_MIRROR_VP+39):7);
    UiSet(FW_UI_PARTS_VP,parts_valid?parts:0);
    /* Read-only FW44 indicators: 2 distinguishes unavailable data from bit 0.
     * The host owns these bits; no screen touch writes FW44 or machine keys. */
    for(i=0;i<16;i++) UiSet(FW_UI_PART_BIT_VP+i,parts_valid?((parts>>i)&1U):2);
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
        /* A controller page change supersedes a queued screen action. */
        if(ui_write && command==3 && (nav_target || start_submit_page)) {
            FwSet(FW_WRITE_TRIGGER_VP,0); ui_write=0; nav_target=start_submit_page=0;
            force_page_write=0; FwSet(FW_UI_RESULT_VP,3);
            home_event=home_target=0;
            if(pass_mode==1) pass_authorized=0;
        }
        if(UiPage(value) && ReadPageId()!=value) UiNavigate(value);
    }
}

/* FW table 20260930: only non-reserved R/W parameters may be submitted. */
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

/* Link drafts on keyboard confirmation or an upper-limit edit. Never alter
 * initial control-board data silently. Both registers must have been read. */
static void UiHardness(void)
{
    uint16_t h, limit;
    if(UiLogicalPage()!=27 || !editing || save_count || ui_write) return;
    if(offline || !(edit_valid[0]&1) || !(edit_valid[10]&1)) {
        hardness_ready=0; UiSet(FW_UI_HARDNESS_VP,3); return;
    }
    h=FwGet(FW_EDIT_VP); limit=FwGet(FW_EDIT_VP+80);
    if(hardness_ready && (h!=hardness_seen || limit!=hardness_limit_seen)) {
        if((h!=edit_base[0] || limit!=edit_base[80]) && FwWritable(0,h) && FwWritable(80,limit) && h>limit) {
            h=limit; FwSet(FW_EDIT_VP,h); UiSet(FW_UI_HARDNESS_VP,2);
        } else UiSet(FW_UI_HARDNESS_VP,h>limit?1:0);
    } else if(!hardness_ready) UiSet(FW_UI_HARDNESS_VP,h>limit?1:0);
    hardness_seen=h; hardness_limit_seen=limit; hardness_ready=1;
}
/* FW74 lower temperature difference must not exceed FW73 upper difference.
 * Link only user-edited drafts, after both values have been read successfully. */
static void UiTemperatureLimits(void)
{
    uint16_t low, high;
    if(UiLogicalPage()!=FW_UI_ADV2_PAGE || !editing || save_count || ui_write) return;
    if(offline || (edit_valid[9]&6)!=6) { temp_limits_ready=0; return; }
    low=FwGet(FW_EDIT_VP+74); high=FwGet(FW_EDIT_VP+73);
    if(temp_limits_ready && (low!=temp_low_seen || high!=temp_high_seen) &&
       (low!=edit_base[74] || high!=edit_base[73]) &&
       FwWritable(74,low) && FwWritable(73,high) && low>high) {
        low=high; FwSet(FW_EDIT_VP+74,low);
    }
    temp_low_seen=low; temp_high_seen=high; temp_limits_ready=1;
}
/* Recheck every actual write against the last confirmed partner value, including
 * legacy VP3700 writes. These confirmed values are discarded on link failure. */
static uint8_t FwPairedWritable(uint16_t a,uint16_t v)
{
    if(a==0) return DisplayValid(80) && v<=FwGet(FW_MIRROR_VP+80);
    if(a==80) return DisplayValid(0) && FwGet(FW_MIRROR_VP)<=v;
    if(a==73) return DisplayValid(74) && FwGet(FW_MIRROR_VP+74)<=v;
    if(a==74) return DisplayValid(73) && v<=FwGet(FW_MIRROR_VP+73);
    return 1;
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
        if(pass_mode==1) { pass_authorized=0; FwSet(FW_UI_PASS_STATE,6); }
        force_page_write=0; start_submit_page=0;
        home_event=home_target=0;
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
                UiNavigate(nav_target);
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
    if(command==6) { force_page_write=0; start_submit_page=0; home_event=home_target=0; }
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

/* Persistent UART4 assembler; never interpret an 06 echo as an 03 response. */
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
    save_count=save_index=ui_write=alarm_cursor=hardness_ready=temp_limits_ready=0; editing=UiSettingsPage(last_page);
    FwSet(FW_UI_HARDNESS_VP,0);
    fw_page=0xffff; return_page=42; operation_page=42; nav_target=warning_submit_page=0;
    home_event=home_target=0;
    for(i=0;i<sizeof(edit_valid);i++) edit_valid[i]=0;
    for(i = FW_WRITE_ADDR_VP; i <= FW_ERROR_VP; ++i) FwSet(i, 0);
    for(i = USB_VIDEO_VP; i <= USB_SOFTWARE_GO_VP; ++i) FwSet(i, 0);
    for(i = 0; i <= 88; ++i) FwSet(FW_MIRROR_VP + i, 0);
    for(i=FW_UI_EVENT_VP;i<FW_UI_LIVE_VP;i++) FwSet(i,0);
    PassInit(); LocalInit();
    if(pass_enabled) UiNavigate(42);
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
    uint16_t e=FwGet(FW_UI_EVENT_VP), p=ReadPageId(), v, target, resolved=0;
    uint8_t i,a,n,good;
    if(!e) return;
    FwSet(FW_UI_EVENT_VP,0);
    if(p==1 || pass_touch_guard) return;
    if((e>>8)==5) { PassEvent(e); return; }
    if(p==FW_UI_PASS_PAGE) return;
    if((e>>8)!=1 && (e>>8)!=2 && (e<0x0300 || e>0x0303) && (e<0x0400 || e>0x0405)) return;
    if(!ui_write && !save_count && !FwGet(FW_WRITE_TRIGGER_VP)) ui_result_page=p;
    if(e==0x0400) { SwitchPageById(120); return; }
    if(e==0x0401) { SwitchPageById(9); return; }
    if(e==0x0402 || e==0x0403) {
        if(ui_write || save_count || command==6 || FwGet(FW_WRITE_TRIGGER_VP)) return;
        if(e==0x0402 && p==2) { LocalDraft(); SwitchPageById(FW_UI_ADV2_PAGE); }
        if(e==0x0403 && p==FW_UI_ADV2_PAGE) SwitchPageById(2);
        return;
    }
    if(e==0x0404 || e==0x0405) {
        if(ui_write || save_count || command==6 || FwGet(FW_WRITE_TRIGGER_VP)) return;
        if(e==0x0404 && p==31) SwitchPageById(FW_UI_BOARD2_PAGE);
        if(e==0x0405 && p==FW_UI_BOARD2_PAGE) SwitchPageById(31);
        return;
    }
    if(e==0x0301 && !ui_write && !save_count) {
        if(p==FW_UI_ADV2_PAGE) { SwitchPageById(2); return; }
        LocalDraft();
        for(i=0;i<89;i++) if(edit_valid[i>>3] & (1U<<(i&7))) FwSet(FW_EDIT_VP+i,edit_base[i]);
        e=0x0200|return_page;
    }
    if(offline || !Valid(16) || ui_write || save_count || command==6 || FwGet(FW_WRITE_TRIGGER_VP)) {
        if(!ui_write && !save_count) FwSet(FW_UI_RESULT_VP,3);
        return;
    }
    if(p==42 && ((e>>8)==1 || (e>>8)==2)) {
        if((pass_enabled && !pass_authorized) || !UiHomeAction(e,&resolved)) {
            FwSet(FW_UI_RESULT_VP,3); return;
        }
        home_event=e; home_target=resolved;
        if(resolved) e=0x0200|resolved;
    }
    if((e>>8)==1) {
        v=e&255;
        if(UiCommandReason(v) ||
           (v==40 && !(FwGet(FW_MIRROR_VP+44)&128))) { home_event=home_target=0; FwSet(FW_UI_RESULT_VP,3); return; }
        if(UiKey(v)) { if(v==40 || v==51) start_submit_page=p; UiSubmit(14,v); }
        else FwSet(FW_UI_RESULT_VP,3);
    } else if((e>>8)==2) {
        target=e&255;
        if(target==0) target=(p==4 || p==40)?42:(UiWarningPage(p)?operation_page:return_page);
        if(target==1) target=42;
        if(target==42 && pass_enabled && !pass_authorized) { home_event=home_target=0; UiNavigate(42); return; }
        if(p==9 && target==42) force_page_write=1;
        if(UiPage(target)) { if(!UiSettingsPage(p)) return_page=p; nav_target=target; UiSubmit(16,target); }
    } else if(e==0x0300 && editing) {
        UiHardness();
        UiTemperatureLimits();
        n=p==2?sizeof(basic_fields):(p==FW_UI_ADV2_PAGE?sizeof(advanced2_fields):sizeof(advanced_fields));
        good=1; save_count=save_index=0;
        local_save=0;
        if(p==2) {
            v=FwGet(LOCAL_ENABLE); target=FwGet(LOCAL_HOURS);
            if(v>1 || target>24 || FwGet(LOCAL_MINS)>59) good=0;
            saved_enable=(uint8_t)v;
            saved_minutes=target*60+FwGet(LOCAL_MINS);
            if(saved_enable && !saved_minutes) good=0;
            v=FwGet(FW_UI_PASS_ENABLE);
            if(v>1 || (v && !pass_draft_set)) good=0;
            pass_saved_enable=(uint8_t)v; pass_saved_set=pass_draft_set; pass_saved=pass_draft;
        }
        for(i=0;i<n;i++) {
            a=p==2?basic_fields[i]:(p==FW_UI_ADV2_PAGE?advanced2_fields[i]:advanced_fields[i]); v=FwGet(FW_EDIT_VP+a);
            if(!(edit_valid[a>>3] & (1U<<(a&7))) || (v!=edit_base[a] && !FwWritable(a,v))) good=0;
            if(v!=edit_base[a]) { save_addresses[save_count]=a; save_values[save_count++]=v; }
        }
        if(p==27) {
            if(FwGet(FW_EDIT_VP)>FwGet(FW_EDIT_VP+80)) { good=0; FwSet(FW_UI_HARDNESS_VP,1); }
            /* When increasing the ceiling, acknowledge it before increasing
             * hardness. On a decrease, FW0 is already first in advanced_fields. */
            if(FwGet(FW_EDIT_VP+80)>edit_base[80]) {
                for(i=0;i<save_count;i++) if(save_addresses[i]==80) {
                    v=save_values[i];
                    while(i) { save_addresses[i]=save_addresses[i-1]; save_values[i]=save_values[i-1]; --i; }
                    save_addresses[0]=80; save_values[0]=v; break;
                }
            }
        }
        if(p==FW_UI_ADV2_PAGE) {
            if(FwGet(FW_EDIT_VP+74)>FwGet(FW_EDIT_VP+73)) good=0;
            /* Default order is upper then lower. If reducing the upper bound,
             * acknowledge a changed lower bound first, preserving the relation
             * even if a later write fails and the remainder is not submitted. */
            if(FwGet(FW_EDIT_VP+73)<edit_base[73]) {
                for(i=0;i<save_count;i++) if(save_addresses[i]==74) {
                    v=save_values[i];
                    while(i) { save_addresses[i]=save_addresses[i-1]; save_values[i]=save_values[i-1]; --i; }
                    save_addresses[0]=74; save_values[0]=v; break;
                }
            }
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
    if(!ui_write) home_event=home_target=0;
}

void FwProtocolTask(void)
{
    uint16_t trigger, address, value, page, touch, resolved;
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
    touch=FwGet(0x0016);
    if((touch>>8)==0x5a) FwSet(0x0016,0);
    PassTouchTask(touch,now);
    UiRunPageTask();
#if sysADVERTISE_MODE_ENABLED
    R11VideoTouchTask(touch);
#endif
    page = UiLogicalPage();
    if(page != last_page) {
        /* Local overlays/player do not trigger extra business reads. */
        if(!offline && UiSettingsPage(page)) refresh_pending = 1;
        editing=UiSettingsPage(page) || (page==FW_UI_PASS_PAGE && pass_mode>=2);
        hardness_ready=temp_limits_ready=0;
        if(page==2 && last_page!=FW_UI_PASS_PAGE) LocalDraft();
        /* Clear completed, page-local feedback; keep active operation ownership. */
        if(!ui_write && !save_count) { FwSet(FW_UI_RESULT_VP,0); ui_result_page=page; }
        if(editing && page!=FW_UI_PASS_PAGE && last_page!=FW_UI_PASS_PAGE) {
            for(address=0;address<sizeof(edit_valid);address++) edit_valid[address]=0;
        }
        last_page=page;
    }
#if sysADVERTISE_MODE_ENABLED
    if(R11VideoIdleActive() || R11VideoWakeBlocked()) FwSet(FW_UI_EVENT_VP,0);
    else
#endif
    UiEvent();
    UiHardness();
    UiTemperatureLimits();
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
        if(home_event && (page!=42 || (pass_enabled && !pass_authorized) ||
           !UiHomeAction(home_event,&resolved) || resolved!=home_target)) {
            UiCancelAction(); return;
        }
        if(address==14 && (UiCommandReason(value) ||
           (start_submit_page && page!=start_submit_page))) {
            UiCancelAction();
            if(timer_owner) { timer_owner=0; timer_wait=0; timer_fault=1; }
            FwSet(FW_UI_RESULT_VP,3); FwSet(FW_WRITE_RESULT_VP,3); return;
        }
        if(warning_submit_page) {
            alarm_key=UiWarningAlarm(warning_submit_page,&alarm_reg,&alarm_bit);
            if(page!=warning_submit_page || !alarm_key || !Valid(alarm_reg) ||
               !(FwGet(FW_MIRROR_VP+alarm_reg)&(1U<<alarm_bit))) {
                warning_submit_page=0;ui_write=0;
                FwSet(FW_UI_RESULT_VP,3);FwSet(FW_WRITE_RESULT_VP,3);return;
            }
            warning_submit_page=0;
        }
        if(command || offline || trigger != 1 || !FwWritable(address, value) || !FwPairedWritable(address,value)) {
            if(timer_owner) { timer_owner=0; timer_fault=1; }
            local_save=0;
            force_page_write=0; start_submit_page=0;
            home_event=home_target=0;
            FwSet(FW_WRITE_RESULT_VP, 3);
            FwSet(FW_ERROR_VP, command ? FW_ERR_BUSY : FW_ERR_VALUE);
            if(ui_write) { ui_write=0; save_count=save_index=0; nav_target=0; FwSet(FW_UI_RESULT_VP,3); }
        } else if(address != 14 && !force_page_write &&
                  (mirror_valid[address >> 3] & (uint8_t)(1U << (address & 7))) &&
                  FwGet(FW_MIRROR_VP + address) == value) {
            /* Ordinary same-value submissions succeed without bus traffic. */
            FwSet(FW_WRITE_RESULT_VP, 2);
            FwSet(FW_ERROR_VP, 0);
            if(ui_write) {
                if(nav_target) {
                    fw_page=nav_target;
                    if(nav_target==1 || nav_target==42) operation_page=nav_target;
                    UiNavigate(nav_target);
                    nav_target=0;
                }
                if(save_count) ++save_index;
                if(save_index>=save_count) { save_count=save_index=0; if(local_save) local_save=2; FwSet(FW_UI_RESULT_VP,2); }
                ui_write=0;
                home_event=home_target=0;
            }
        } else {
            request_address = address; request_value = value;
            /* Related settings may change before the post-write readback. */
            FwInvalidateMirror();
            command = 6; sent_at = now; rx_size = 0;
            FwSet(FW_WRITE_RESULT_VP, 1);
            SendModbusWriteSingleRegisterFrame(&Uart4, FW_ADDRESS, address, value);
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
    SendModbusReadHoldingRegistersFrame(&Uart4, FW_ADDRESS, request_address, request_count);
}

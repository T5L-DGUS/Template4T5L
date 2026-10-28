/* Exercise the production state machine with fixed-width host types and UART/VP mocks. */
#include "fw_test_platform.h"

typedef struct { uint8_t command; uint16_t address, value; } Request;
UART_TYPE Uart4 = {4}, Uart_R11 = {11};
uint8_t ET0 = 1;
static uint16_t vp[0x4000], page, device[89];
static uint32_t tick;
static Request requests[2048];
static unsigned request_count, usb_count, checks, failed_line;
static uint8_t usb_frame[6];
static uint8_t idle_active,wake_blocked;
static uint16_t idle_return;
static uint16_t video_touch_sample;
static uint8_t record_fail, delay_page_switch;
static uint16_t pending_page;
static uint16_t record_fail_address;
void FwTestRecordStore(uint16_t address,uint8_t words) { if(record_fail && (!record_fail_address || record_fail_address==address)) vp[address+words-1]^=1; }
void RtcSetTime(uint8_t *t) {
 unsigned i; for(i=0;i<4;i++) vp[0x10+i]=((uint16_t)t[i*2]<<8)|t[i*2+1];
}
void RtcReadTime(void) {}
void R11VideoTouchTask(uint16_t touch) {video_touch_sample=touch;}
uint8_t R11VideoIdleActive(void) {return idle_active && page==121;}
uint16_t R11VideoIdleReturnPage(void) {return idle_return;}
uint8_t R11VideoWakeBlocked(void) {return wake_blocked;}

#define CHECK(condition) do { ++checks; if (!(condition)) { failed_line = __LINE__; return; } } while (0)

uint32_t GetSysTick(void) { return tick; }
uint16_t ReadPageId(void) { return page; }
void SwitchPageById(uint16_t p) { if(delay_page_switch) pending_page=p; else page=p; }
void write_dgus_vp(uint32_t address, uint8_t *bytes, uint16_t count)
{
    uint16_t i;
    for (i = 0; i < count; ++i) vp[address + i] = ((uint16_t *)bytes)[i];
}
void read_dgus_vp(uint32_t address, uint8_t *bytes, uint8_t count)
{
    uint16_t i;
    for (i = 0; i < count; ++i) ((uint16_t *)bytes)[i] = vp[address + i];
}
uint16_t crc_16(uint8_t *bytes, uint16_t length)
{
    uint16_t crc = 0xffff, i;
    uint8_t bit;
    for (i = 0; i < length; ++i) {
        crc ^= bytes[i];
        for (bit = 0; bit < 8; ++bit) crc = (crc >> 1) ^ ((crc & 1) ? 0xa001 : 0);
    }
    return crc;
}
void SendModbusReadHoldingRegistersFrame(UART_TYPE *uart, uint8_t station,
                                        uint16_t address, uint16_t count)
{
    if (uart != &Uart4 || station != 1 || request_count == 2048) { failed_line = __LINE__; return; }
    requests[request_count].command = 3;
    requests[request_count].address = address;
    requests[request_count++].value = count;
}
void SendModbusWriteSingleRegisterFrame(UART_TYPE *uart, uint8_t station,
                                      uint16_t address, uint16_t value)
{
    if (uart != &Uart4 || station != 1 || request_count == 2048) { failed_line = __LINE__; return; }
    requests[request_count].command = 6;
    requests[request_count].address = address;
    requests[request_count++].value = value;
}
void UartSendData(UART_TYPE *uart, uint8_t *bytes, uint16_t length)
{
    uint16_t i;
    if (uart != &Uart_R11 || length != 6) { failed_line = __LINE__; return; }
    ++usb_count;
    for (i = 0; i < 6; ++i) usb_frame[i] = bytes[i];
}
static void add_crc(uint8_t *frame, uint8_t length)
{
    uint16_t crc = crc_16(frame, length - 2);
    frame[length - 2] = (uint8_t)crc;
    frame[length - 1] = (uint8_t)(crc >> 8);
}
static uint16_t device_value(uint16_t address)
{
    return device[address];
}
static uint8_t make_reply(uint8_t *frame)
{
    Request *request = &requests[request_count - 1];
    uint16_t i, value;
    uint8_t length;
    frame[0] = 1; frame[1] = request->command;
    if (request->command == 3) {
        frame[2] = (uint8_t)(request->value * 2);
        for (i = 0; i < request->value; ++i) {
            value = device_value(request->address + i);
            frame[3 + i * 2] = (uint8_t)(value >> 8);
            frame[4 + i * 2] = (uint8_t)value;
        }
        length = (uint8_t)(5 + request->value * 2);
    } else {
        frame[2] = (uint8_t)(request->address >> 8); frame[3] = (uint8_t)request->address;
        frame[4] = (uint8_t)(request->value >> 8); frame[5] = (uint8_t)request->value;
        length = 8;
    }
    add_crc(frame, length);
    return length;
}
static void reply(void)
{
    uint8_t frame[64], length = make_reply(frame);
    if(requests[request_count-1].command==6) device[requests[request_count-1].address]=requests[request_count-1].value;
    FwProtocolFeed(frame, length);
}
static void reset_at(uint32_t start)
{
    unsigned i;
    for (i = 0; i < sizeof(vp) / sizeof(vp[0]); ++i) vp[i] = 0;
    for(i=0;i<89;i++) device[i]=0;
    device[0]=20;device[1]=20;device[5]=(uint16_t)-50;device[8]=100;device[9]=150;
    device[16]=42;device[80]=30;device[81]=100;device[83]=700;device[85]=30;device[44]=0xffff;
    page = 42; tick = start; request_count = usb_count = 0;
    idle_active=wake_blocked=0;idle_return=42;
    record_fail=delay_page_switch=0; pending_page=0;
    record_fail_address=0;
    vp[0x10]=0x1a0a;vp[0x11]=0x1b02;vp[0x12]=0x0100;vp[0x13]=0;
    FwProtocolInit();
}
static void reset(void) { reset_at(0); }
static void finish_snapshot(void)
{
    unsigned i, before;
    for (i = 0; i < 31; ++i) {
        before = request_count;
        reply(); FwProtocolTask();
        if (request_count == before) return;
    }
    failed_line = __LINE__;
}
static void submit(uint16_t address, uint16_t value)
{
    vp[FW_WRITE_ADDR_VP] = address; vp[FW_WRITE_VALUE_VP] = value;
    vp[FW_WRITE_TRIGGER_VP] = 1; FwProtocolTask();
}
static void online(void) { reset(); FwProtocolTask(); finish_snapshot(); }
static void event(uint16_t e) { vp[FW_UI_EVENT_VP]=e; FwProtocolTask(); }
static void go(uint16_t p) {
 const char *digits="123456";
 event(0x0200|p);
 if(p==27 && page==FW_UI_PASS_PAGE) { while(*digits)event(0x0510+(uint16_t)(*digits++-'0'));event(0x0521); }
 reply(); FwProtocolTask(); finish_snapshot();
}
static void advance(uint32_t ms) { tick+=ms;FwProtocolTask(); }
static void exception(void) { uint8_t f[5]={1,0x86,3,0,0};add_crc(f,5);FwProtocolFeed(f,5); }
static int text_is(uint16_t a,const char *s) {
 unsigned i=0;while(s[i]) {uint8_t c=(i&1)?vp[a+i/2]&255:vp[a+i/2]>>8;if(c!=(uint8_t)s[i])return 0;i++;}return 1;
}
static void test_initial_and_poll(void) {
 unsigned before,i;
 reset();CHECK(vp[FW_UI_READY_VP]==0);CHECK(text_is(0x3720,"--"));event(0x0101);CHECK(requests[0].command==3);
 finish_snapshot();CHECK(request_count==15);CHECK(vp[FW_UI_READY_VP]==1);
 CHECK(text_is(0x3724,"0.0"));CHECK(vp[FW_MIRROR_VP+3]==0);
 before=request_count;advance(999);CHECK(request_count==before);advance(1);finish_snapshot();CHECK(request_count==before+10);
 for(i=before;i<request_count;i++)CHECK(requests[i].command==3 && requests[i].address!=0);
}
static void test_units_signed_and_bounds(void) {
 unsigned before;
 online();device[30]=(uint16_t)-123;device[35]=2875;device[50]=9999;device[13]=65535;
 advance(1000);finish_snapshot();CHECK(text_is(0x3724,"-12.3"));CHECK(text_is(0x3734,"28.75"));CHECK(text_is(0x3740,"99.99"));CHECK(text_is(0x3720,"65535"));
 go(2);before=request_count;vp[FW_EDIT_VP+5]=(uint16_t)-151;event(0x0300);CHECK(request_count==before);CHECK(vp[FW_UI_RESULT_VP]==3);
 vp[FW_EDIT_VP+5]=(uint16_t)-150;event(0x0300);CHECK(requests[request_count-1].value==(uint16_t)-150);reply();FwProtocolTask();finish_snapshot();
 vp[FW_EDIT_VP+5]=150;event(0x0300);CHECK(requests[request_count-1].value==150);reply();FwProtocolTask();finish_snapshot();
 vp[FW_EDIT_VP+5]=151;before=request_count;event(0x0300);CHECK(request_count==before);
}
static void test_edit_refresh_cancel(void) {
 online();go(2);CHECK(vp[FW_EDIT_VP+5]==(uint16_t)-50);vp[FW_EDIT_VP+5]=25;
 advance(1000);finish_snapshot();CHECK(vp[FW_EDIT_VP+5]==25);CHECK(text_is(0x3753,"2.5"));
 event(0x0301);CHECK(vp[FW_EDIT_VP+5]==(uint16_t)-50);CHECK(requests[request_count-1].address==16);
 reply();CHECK(page==42);
}
static void test_save_snapshot_and_repeat(void) {
 unsigned before;
 online();go(27);vp[FW_EDIT_VP+10]=1;vp[FW_EDIT_VP+1]=22;event(0x0300);
 CHECK(requests[request_count-1].address==10);CHECK(requests[request_count-1].value==1);
 before=request_count;event(0x0300);CHECK(request_count==before);vp[FW_EDIT_VP+1]=23;
 reply();FwProtocolTask();CHECK(requests[request_count-1].address==1);CHECK(requests[request_count-1].value==22);
 reply();FwProtocolTask();finish_snapshot();CHECK(vp[FW_EDIT_VP+1]==23);CHECK(vp[FW_UI_RESULT_VP]==2);
 event(0x0300);CHECK(requests[request_count-1].address==1);CHECK(requests[request_count-1].value==23);
}
static void test_partial_failure_and_recovery(void) {
 unsigned before;
 online();go(27);vp[FW_EDIT_VP+10]=1;vp[FW_EDIT_VP+1]=22;event(0x0300);reply();FwProtocolTask();exception();
 CHECK(device[10]==1 && device[1]==20);CHECK(vp[FW_UI_RESULT_VP]==3);CHECK(vp[FW_UI_READY_VP]==0);
 before=request_count;advance(2999);CHECK(request_count==before);advance(1);CHECK(requests[request_count-1].command==3);finish_snapshot();
 CHECK(vp[FW_EDIT_VP+10]==1 && vp[FW_EDIT_VP+1]==22);event(0x0300);CHECK(requests[request_count-1].command==6 && requests[request_count-1].address==1);
}
static void test_read_timeout_cancels_queued_action(void) {
 unsigned before;
 online();advance(1000);event(0x0101);before=request_count;CHECK(vp[FW_WRITE_TRIGGER_VP]==1);
 advance(500);CHECK(vp[FW_WRITE_TRIGGER_VP]==0);CHECK(request_count==before);CHECK(vp[FW_UI_RESULT_VP]==3);
 event(0x0102);CHECK(request_count==before);advance(3000);CHECK(requests[request_count-1].command==3);
}
static void test_no_command_retry_and_fast_clicks(void) {
 unsigned before;
 online();event(0x0101);before=request_count;event(0x0102);CHECK(request_count==before);
 advance(500);CHECK(vp[FW_UI_RESULT_VP]==3);advance(3000);finish_snapshot();
 CHECK(requests[before].command==3);CHECK(device[14]==0);
}
static void test_pages_local_and_unknown(void) {
 unsigned before;
 online();event(0x0400);CHECK(page==120);before=request_count;FwProtocolTask();CHECK(request_count==before);
 advance(1000);finish_snapshot();CHECK(page==120);device[16]=88;advance(1000);finish_snapshot();CHECK(page==120);
 device[16]=9;advance(1000);finish_snapshot();CHECK(page==9);event(0x0278);CHECK(page==9);CHECK(request_count==before+30);
 event(0x0400);event(0x0401);CHECK(page==9);
 event(0x022a);CHECK(page==9);reply();CHECK(page==42);
}
static void test_alarm_mapping(void) {
 unsigned before;
 online();device[41]=8;device[42]=0;advance(1000);finish_snapshot();CHECK(vp[FW_UI_ALARM_VP]==0);before=request_count;event(0x0302);CHECK(request_count==before);
 device[41]=0;device[42]=1U<<14;advance(3000);finish_snapshot();FwProtocolTask();CHECK(vp[FW_UI_ALARM_VP]==18);
 event(0x0302);CHECK(requests[request_count-1].address==14 && requests[request_count-1].value==43);
}
static void test_warning_pages_and_return(void) {
 static const uint16_t ps[]={8,10,11,12,13,15,16,17,18,19,20,21,22,43};
 unsigned i,before;
 for(i=0;i<sizeof(ps)/sizeof(ps[0]);i++) {
  online();device[16]=ps[i];advance(1000);finish_snapshot();CHECK(page==ps[i]);
  before=request_count;event(0x0200);CHECK(page==(ps[i]==17?42:ps[i]));CHECK(requests[before].address==16 && requests[before].value==42);
  reply();CHECK(page==42);
 }
 online();go(42);device[16]=43;advance(1000);finish_snapshot();event(0x0200);CHECK(requests[request_count-1].value==42);reply();CHECK(page==42);
 /* A warning can replace FW16 before the first read following a navigation ACK. */
 online();go(9);event(0x022a);reply();CHECK(page==42);
 device[16]=43;FwProtocolTask();finish_snapshot();CHECK(page==43);
 event(0x0200);CHECK(requests[request_count-1].address==16 && requests[request_count-1].value==42);
 reset();device[16]=19;FwProtocolTask();finish_snapshot();event(0x0200);CHECK(requests[request_count-1].value==42);
}
static void test_warning_clear_fixed_mapping(void) {
 static const uint8_t ps[]={11,13,15,16,17,18,20,22,10,21,43};
 static const uint8_t regs[]={41,41,41,41,41,41,41,41,42,42,42};
 static const uint8_t bits[]={13,2,7,0,14,4,1,6,14,13,12};
 static const uint8_t keys[]={7,7,7,7,7,7,7,7,43,44,42};
 unsigned i,before;
 for(i=0;i<sizeof(ps);i++) {
  if(ps[i]==17) continue; /* Confirm now acknowledges locally; covered separately. */
  online();device[16]=ps[i];device[41]=device[42]=0xffff;advance(1000);finish_snapshot();
  vp[FW_UI_ALARM_VP]=1;before=request_count;event(0x0303);
  CHECK(request_count==before+1 && requests[before].address==14 && requests[before].value==keys[i]);
  event(0x0303);CHECK(request_count==before+1);reply();CHECK(page==ps[i]);
  CHECK(vp[FW_MIRROR_VP+regs[i]]&(1U<<bits[i]));
 }
}
static void test_warning_clear_invalid_and_timeout(void) {
 unsigned before;
 online();device[16]=10;device[42]=1U<<12;advance(1000);finish_snapshot();before=request_count;
 event(0x0303);CHECK(request_count==before && vp[FW_UI_RESULT_VP]==3);
 device[42]|=1U<<14;advance(1000);finish_snapshot();event(0x0303);before=request_count;
 advance(500);CHECK(vp[FW_UI_RESULT_VP]==3 && page==10);event(0x0303);CHECK(request_count==before);
 advance(3000);CHECK(requests[request_count-1].command==3);finish_snapshot();
 device[42]=0;advance(1000);finish_snapshot();before=request_count;event(0x0303);CHECK(request_count==before);
 online();device[16]=19;device[42]=1;advance(1000);finish_snapshot();before=request_count;event(0x0303);CHECK(request_count==before);
 online();device[16]=8;advance(1000);finish_snapshot();before=request_count;event(0x0303);CHECK(request_count==before);
}
static void test_warning_clear_queued_page_change(void) {
 unsigned before;
 online();device[16]=10;device[42]=1U<<14;advance(1000);finish_snapshot();
 advance(1000);reply();FwProtocolTask();CHECK(requests[request_count-1].address==16);
 before=request_count;event(0x0303);CHECK(request_count==before);
 device[16]=19;reply();FwProtocolTask();CHECK(page==19 && request_count==before && vp[FW_UI_RESULT_VP]==3);
}
static void test_component_gates_and_noops(void) {
 unsigned before;
 online();device[44]=0;advance(1000);finish_snapshot();before=request_count;event(0x0126);CHECK(request_count==before);event(0x0128);CHECK(request_count==before);
 submit(0,20);CHECK(request_count==before);CHECK(vp[FW_WRITE_RESULT_VP]==2);
 submit(11,3);CHECK(request_count==before);CHECK(vp[FW_WRITE_RESULT_VP]==3);
 submit(3,1);CHECK(request_count==before);
}
static void test_parser_split_crc_and_deadline(void) {
 uint8_t f[64],n;unsigned before;
 reset_at(UINT32_MAX-100);FwProtocolTask();n=make_reply(f);f[n-1]^=1;FwProtocolFeed(f,n);CHECK(vp[FW_MIRROR_VP]==0);
 n=make_reply(f);FwProtocolFeed(f,2);FwProtocolFeed(f+2,n-2);CHECK(vp[FW_MIRROR_VP]==20);FwProtocolTask();
 n=make_reply(f);before=request_count;advance(500);FwProtocolFeed(f,n);CHECK(vp[FW_ERROR_VP]==0x100);CHECK(request_count==before);
 advance(3000);CHECK(requests[request_count-1].command==3);
}
static void test_usb_independent(void) {
 uint8_t f[6]={0xaa,0x55,0,2,0xfb,1};
 online();FwUsbProtocol(&Uart4,f,6);CHECK(!vp[USB_VIDEO_VP]);FwUsbProtocol(&Uart_R11,f,6);CHECK(vp[USB_VIDEO_VP]);
 vp[USB_VIDEO_GO_VP]=1;FwProtocolTask();CHECK(usb_count==1 && usb_frame[5]==1);
 f[5]=2;FwUsbProtocol(&Uart_R11,f,6);vp[USB_SOFTWARE_GO_VP]=1;FwProtocolTask();CHECK(usb_count==2 && usb_frame[5]==2);
 f[5]=3;FwUsbProtocol(&Uart_R11,f,6);CHECK(vp[USB_COMPLETE_VP]);CHECK(!vp[USB_VIDEO_VP]);
}
extern int printf(const char *,...);
static void test_result_belongs_to_original_page(void) {
 static const uint16_t ps[]={8,10,11,12,13,15,16,17,18,19,20,21,22,43};
 unsigned before,i;
 for(i=0;i<sizeof(ps)/sizeof(ps[0]);i++) {
  online();event(0x0303);advance(100);CHECK(vp[FW_UI_STATUS_ICON_VP]==3);
  device[16]=ps[i];advance(1000);finish_snapshot();CHECK(page==ps[i] && vp[FW_UI_STATUS_ICON_VP]==0);
 }
 online();event(0x0303);advance(100);CHECK(vp[FW_UI_STATUS_ICON_VP]==3);
 before=request_count;device[16]=11;advance(1000);finish_snapshot();
 CHECK(page==11 && vp[FW_UI_RESULT_VP]==0 && vp[FW_UI_STATUS_ICON_VP]==0);
 CHECK(requests[before].command==3);event(0x0303);advance(100);CHECK(vp[FW_UI_STATUS_ICON_VP]==3);
 device[16]=13;advance(1000);finish_snapshot();CHECK(page==13 && vp[FW_UI_STATUS_ICON_VP]==0);
 /* A queued action cancelled by a controller page change must not mark the new page failed. */
 online();device[16]=10;device[42]=1U<<14;advance(1000);finish_snapshot();
 advance(1000);reply();FwProtocolTask();event(0x0303);device[16]=19;reply();FwProtocolTask();advance(100);
 CHECK(page==19 && vp[FW_UI_STATUS_ICON_VP]==0);
}
static void test_idle_keeps_edit_session_and_blocks_wake_action(void) {
 unsigned before;
 online();go(27);vp[FW_EDIT_VP+10]=1;idle_return=27;idle_active=1;page=121;
 advance(1000);finish_snapshot();CHECK(page==121 && vp[FW_EDIT_VP+10]==1);
 before=request_count;event(0x0101);CHECK(request_count==before && vp[FW_UI_EVENT_VP]==0);
 page=27;idle_active=0;wake_blocked=1;event(0x0101);CHECK(request_count==before);
 wake_blocked=0;FwProtocolTask();CHECK(vp[FW_EDIT_VP+10]==1);
 event(0x0300);CHECK(requests[request_count-1].address==10 && requests[request_count-1].value==1);
}
static void refresh_device(void) { advance(1000);finish_snapshot();advance(100); }
static void pass_digits(const char *s) { while(*s)event(0x0510+(uint16_t)(*s++-'0')); }
static void configure_password(void) {
 go(2);event(0x0500);CHECK(page==FW_UI_PASS_PAGE);
 pass_digits("001234");CHECK(text_is(FW_UI_PASS_MASK,"******"));event(0x0521);
 CHECK(vp[FW_UI_PASS_STATE]==2);pass_digits("001234");event(0x0521);CHECK(page==2);
 vp[FW_UI_PASS_ENABLE]=1;event(0x0300);FwProtocolTask();CHECK(vp[FW_UI_RESULT_VP]==2);
}
static void test_start_conditions(void) {
 static const int16_t temperatures[]={-1,0,19,20};
 unsigned i,before;
 for(i=0;i<4;i++) {
  online();device[30]=(uint16_t)temperatures[i];device[37]=0;device[16]=3;refresh_device();
  CHECK(vp[FW_UI_CHECK_VP]==(i==0));CHECK(vp[FW_UI_CHECK_VP+2]==(i<3));
  before=request_count;event(0x0133);CHECK(request_count==before+(i==0));
  if(i==0)CHECK(requests[request_count-1].address==14 && requests[request_count-1].value==51);
  online();device[30]=(uint16_t)temperatures[i];device[37]=1;device[16]=3;refresh_device();
  CHECK(vp[FW_UI_CHECK_VP]==1);before=request_count;event(0x0133);CHECK(request_count==before+(i<3));
 }
 online();device[37]=1;device[16]=39;refresh_device();before=request_count;event(0x0128);
 CHECK(request_count==before+1 && requests[before].value==40);CHECK(page==39);
 reply();FwProtocolTask();finish_snapshot();CHECK(page==39);
 online();device[37]=1;device[16]=39;device[44]=0;refresh_device();before=request_count;event(0x0128);
 CHECK(request_count==before && vp[FW_UI_REASON_VP]==6);
}
static void test_alarm_and_lid_gates(void) {
 static const uint16_t masks[]={0x60d7,0x7763};unsigned r,b,before;
 for(r=0;r<2;r++)for(b=0;b<16;b++) {
  online();device[37]=1;device[16]=3;device[41+r]=(uint16_t)(1U<<b);refresh_device();
  CHECK(vp[FW_UI_CHECK_VP+1]==(!(masks[r]&(1U<<b))));before=request_count;event(0x0133);
  CHECK(request_count==before+(!(masks[r]&(1U<<b))));
 }
 online();device[37]=1;device[41]=0x81;device[42]=0x1001;refresh_device();CHECK(vp[FW_UI_LID_VP]==1);
 before=request_count;event(0x0102);CHECK(request_count==before);submit(14,50);CHECK(request_count==before);
 event(0x011b);CHECK(request_count==before);event(0x0101);CHECK(request_count==before+1);
 reply();FwProtocolTask();finish_snapshot();event(0x0202);CHECK(requests[request_count-1].address==16);
 reply();FwProtocolTask();finish_snapshot();device[42]=0;refresh_device();CHECK(vp[FW_UI_LID_VP]==0);
 online();device[37]=1;refresh_device();advance(1000);advance(500);before=request_count;event(0x0133);
 CHECK(request_count==before && vp[FW_UI_CHECK_VP]==2);
}
static void test_queued_start_recheck(void) {
 unsigned before;
 online();device[37]=1;device[16]=3;refresh_device();
 advance(1000);reply();FwProtocolTask();reply();FwProtocolTask();
 CHECK(requests[request_count-1].address==30);before=request_count;event(0x0133);
 CHECK(request_count==before);device[30]=20;reply();FwProtocolTask();
 CHECK(request_count==before && vp[FW_UI_RESULT_VP]==3);
 online();device[37]=1;device[16]=3;refresh_device();advance(1000);reply();FwProtocolTask();
 before=request_count;event(0x0133);device[16]=19;reply();FwProtocolTask();
 CHECK(page==19);CHECK(request_count==before+1 && requests[before].command==3);
}
static void touch(uint8_t state) { vp[0x0016]=0x5a00|state;FwProtocolTask(); }
static void open_locked_password(void) {
 touch(1);CHECK(page==142);CHECK(vp[0x0016]==0 && video_touch_sample==0x5a01);
 touch(2);advance(200);finish_snapshot();
}
static void unlock_password(void) {
 pass_digits("001234");event(0x0521);CHECK(page==142);
 reply();FwProtocolTask();finish_snapshot();CHECK(page==42);
}
static void expire_session(void) { touch(2);advance(60000);finish_snapshot(); }
static void test_password_settings_and_persistence(void) {
 unsigned before;uint16_t record[8];unsigned i;
 online();go(2);vp[FW_UI_PASS_ENABLE]=1;event(0x0300);CHECK(vp[FW_UI_RESULT_VP]==3);
 event(0x0500);pass_digits("123");event(0x0521);CHECK(vp[FW_UI_PASS_STATE]==5);
 event(0x0522);CHECK(page==2);event(0x0500);pass_digits("123456");event(0x0521);
 pass_digits("123457");event(0x0521);CHECK(page==142 && vp[FW_UI_PASS_STATE]==4);
 event(0x0522);event(0x0301);reply();FwProtocolTask();finish_snapshot();CHECK(vp[FW_UI_PASS_ENABLE]==0);
 configure_password();CHECK(vp[FW_UI_PASS_RECORD]==0x5057);CHECK(vp[FW_UI_PASS_RECORD+5]==1234);
 for(i=0;i<8;i++)record[i]=vp[FW_UI_PASS_RECORD+i];
 event(0x0500);pass_digits("999999");event(0x0521);pass_digits("999999");event(0x0521);
 event(0x0301);reply();FwProtocolTask();finish_snapshot();CHECK(vp[FW_UI_PASS_RECORD+5]==1234);
 for(i=0;i<8;i++)CHECK(record[i]==vp[FW_UI_PASS_RECORD+i]);
 FwProtocolInit();FwProtocolTask();finish_snapshot();CHECK(page==1);open_locked_password();
 before=request_count;pass_digits("1234");event(0x0521);CHECK(page==142 && request_count==before);
 event(0x0522);CHECK(page==1);open_locked_password();
 before=request_count;
 pass_digits("009999");event(0x0521);CHECK(vp[FW_UI_PASS_STATE]==3 && request_count==before);
 pass_digits("001235");event(0x0520);event(0x0514);event(0x0521);
 CHECK(requests[request_count-1].address==16 && requests[request_count-1].value==42 && page==142);
 before=request_count;event(0x0521);CHECK(request_count==before);
 reply();FwProtocolTask();finish_snapshot();CHECK(page==42);
 device[82]=1;refresh_device();go(3);event(0x0200);reply();FwProtocolTask();finish_snapshot();CHECK(page==42);
 go(9);go(1);CHECK(page==42);
}
static void test_password_host_and_navigation(void) {
 static const uint16_t ps[]={6,9,12,30};
 unsigned before,i;
 online();page=9;FwProtocolTask();before=request_count;event(0x0201);
 CHECK(request_count==before+1 && requests[before].address==16 && requests[before].value==42 && page==9);
 reply();FwProtocolTask();finish_snapshot();CHECK(page==42);
 configure_password();
 for(i=0;i<4;i++) {go(ps[i]);go(1);CHECK(page==42);}
 go(9);expire_session();CHECK(page==9);
 device[16]=42;refresh_device();CHECK(page==1);open_locked_password();
 before=request_count;event(0x0102);CHECK(request_count==before);
 unlock_password();
 go(12);expire_session();event(0x0200);CHECK(page==1);open_locked_password();event(0x0522);CHECK(page==1);
 open_locked_password();pass_digits("001234");event(0x0521);exception();CHECK(page==142 && vp[FW_UI_PASS_STATE]==6);
 advance(3000);finish_snapshot();unlock_password();
 go(9);expire_session();event(0x0201);CHECK(page==1);open_locked_password();
 device[16]=10;device[42]=0x4000;refresh_device();CHECK(page==10);
}
static void test_password_draft_and_timer_lid(void) {
 unsigned before;uint16_t original;
 online();go(2);vp[FW_EDIT_VP+5]=(uint16_t)-60;vp[0x37A1]=1;
 event(0x0500);pass_digits("000000");event(0x0521);pass_digits("000000");event(0x0521);
 FwProtocolTask();finish_snapshot();CHECK(page==2 && vp[FW_EDIT_VP+5]==(uint16_t)-60 && vp[0x37A1]==1);
 vp[FW_UI_PASS_ENABLE]=1;event(0x0300);CHECK(requests[request_count-1].address==5);
 CHECK(vp[FW_UI_PASS_RECORD]==0);exception();CHECK(vp[FW_UI_PASS_RECORD]==0);
 advance(3000);finish_snapshot();event(0x0300);reply();FwProtocolTask();finish_snapshot();FwProtocolTask();
 CHECK(vp[FW_UI_PASS_RECORD+2]==1 && vp[FW_UI_PASS_RECORD+3]==1 && vp[FW_UI_PASS_RECORD+5]==0);
 original=vp[FW_UI_PASS_RECORD+6];vp[FW_UI_PASS_ENABLE]=0;event(0x0301);reply();FwProtocolTask();finish_snapshot();
 CHECK(vp[FW_UI_PASS_RECORD+2]==1 && vp[FW_UI_PASS_RECORD+6]==original);
 online();go(2);vp[0x37A0]=1;vp[0x37A1]=1;vp[0x37A2]=1;event(0x0300);FwProtocolTask();
 device[39]=3;device[82]=1;device[37]=1;device[42]=1;refresh_device();
 before=request_count;vp[0x12]=0x0101;advance(100);finish_snapshot();
 CHECK(device[14]==40 && vp[0x37A4]==6);
 CHECK(request_count>before);
}
static void check_home(uint16_t e,uint16_t address,uint16_t value) {
 unsigned before=request_count;
 event(e);
 if(!address) {CHECK(request_count==before && vp[FW_UI_RESULT_VP]==3);return;}
 CHECK(request_count==before+1 && requests[before].command==6);
 CHECK(requests[before].address==address && requests[before].value==value);
 reply();FwProtocolTask();finish_snapshot();
 CHECK(page==(address==16?value:42));
}
static void test_home_mode_matrix(void) {
 unsigned m;
 for(m=0;m<8;m++) {
  online();device[39]=m;device[82]=1;device[44]=0;refresh_device();
  check_home(FW_UI_RUN_EVENT,(m==0||m==2||m==3)?14:0,4);if(failed_line)return;
  online();device[39]=m;device[82]=1;refresh_device();
  check_home(0x0227,(m==0||m==4)?16:0,m==4?40:39);if(failed_line)return;
  online();device[39]=m;device[82]=1;refresh_device();
  check_home(0x0203,(m==0||m==6)?16:0,m==6?4:3);if(failed_line)return;
  online();device[39]=m;refresh_device();
  check_home(0x0132,m==0?14:0,50);if(failed_line)return;
  online();device[39]=m;refresh_device();
  check_home(0x0103,m==2?16:((m==0||m==3)?14:0),m==2?12:3);if(failed_line)return;
  online();device[39]=m;device[44]=0;refresh_device();
  check_home(0x0126,m==3?14:((m==2||m==4||m==6)?16:0),m==3?38:(m==2?12:(m==4?40:4)));if(failed_line)return;
 }
}
static void test_home_alarm_temperature_and_data(void) {
 unsigned b,m,before;static const int16_t temps[]={-300,-1,0,99,100,101};
 for(b=0;b<16;b++) {
  online();device[41]=1U<<b;refresh_device();before=request_count;
  event(FW_UI_RUN_EVENT);event(0x0132);event(0x0103);CHECK(request_count==before);
  online();device[42]=1U<<b;refresh_device();
  check_home(FW_UI_RUN_EVENT,b?14:0,4);if(failed_line)return;
  check_home(0x0132,b?14:0,50);if(failed_line)return;
 }
 for(b=0;b<6;b++) {
  online();device[30]=(uint16_t)temps[b];refresh_device();
  check_home(0x0132,b<4?14:0,50);if(failed_line)return;
 }
 for(b=0;b<3;b+=2) for(m=0;m<3;m++) {
  online();device[39]=m==0?0:(m==1?4:6);device[82]=b;refresh_device();before=request_count;
  event(0x0227);event(0x0203);CHECK(request_count==before);
 }
 online();device[39]=2;device[41]=0xffff;device[42]=1;refresh_device();check_home(0x0103,16,12);if(failed_line)return;
 online();check_home(FW_UI_RUN_EVENT,14,4);if(failed_line)return;
 check_home(FW_UI_RUN_EVENT,14,4);if(failed_line)return;
 check_home(0x0102,14,2);if(failed_line)return;
 before=request_count;submit(39,4);CHECK(request_count==before && vp[FW_WRITE_RESULT_VP]==3);
 reset();FwProtocolTask();reply();FwProtocolTask();reply();FwProtocolTask();before=request_count;
 event(FW_UI_RUN_EVENT);event(0x0132);event(0x0103);event(0x0227);event(0x0203);event(0x0126);CHECK(request_count==before);
 online();advance(1000);advance(500);before=request_count;
 event(FW_UI_RUN_EVENT);event(0x0227);event(0x0203);event(0x0103);event(0x0132);event(0x0126);CHECK(request_count==before);
}
static void wait_read(uint16_t address) {
 unsigned i;advance(1000);
 for(i=0;i<20;i++) {
  if(requests[request_count-1].command==3 && requests[request_count-1].address==address)return;
  reply();FwProtocolTask();
 }
 CHECK(0);
}
static void test_home_queued_rechecks(void) {
 static const uint16_t events[]={0x0104,0x0104,0x0132,0x0227,0x0203,0x0103,0x0126};
 static const uint16_t addresses[]={39,41,30,82,82,39,39};
 static const uint16_t changes[]={4,1,100,0,0,2,4};
 unsigned i,before;
 for(i=0;i<7;i++) {
  online();device[82]=1;device[39]=i==6?3:0;refresh_device();wait_read(addresses[i]);
  before=request_count;event(events[i]);CHECK(vp[FW_WRITE_TRIGGER_VP]==1 && request_count==before);
  device[addresses[i]]=changes[i];reply();FwProtocolTask();
  CHECK(request_count==before && vp[FW_WRITE_TRIGGER_VP]==0 && vp[FW_UI_RESULT_VP]==3 && page==42);
 }
 online();wait_read(16);before=request_count;event(FW_UI_RUN_EVENT);device[16]=19;reply();FwProtocolTask();
 CHECK(request_count==before && page==19 && vp[FW_WRITE_TRIGGER_VP]==0);
}
static void test_mode_page_returns(void) {
 unsigned i;static const uint16_t active[]={4,40};
 for(i=0;i<2;i++) {
  online();device[82]=1;refresh_device();go(i?39:3);device[16]=active[i];refresh_device();
  event(0x0200);CHECK(requests[request_count-1].address==16 && requests[request_count-1].value==42);
  reply();FwProtocolTask();finish_snapshot();CHECK(page==42);
 }
 online();device[39]=3;refresh_device();check_home(0x0126,14,38);if(failed_line)return;
 device[16]=6;refresh_device();CHECK(page==6);go(1);device[30]=(uint16_t)-31;device[16]=30;refresh_device();CHECK(page==30);
}
static void test_password_timeout_and_touch(void) {
 unsigned before;
 online();configure_password();go(42);touch(2);
 advance(59999);finish_snapshot();CHECK(page==42);advance(1);finish_snapshot();CHECK(page==1);
 before=request_count;event(0x0101);event(0x0400);event(0x0202);event(0x0511);CHECK(page==1 && request_count==before);
 vp[FW_UI_EVENT_VP]=FW_UI_RUN_EVENT;touch(1);CHECK(page==142 && request_count==before);
 CHECK(vp[FW_UI_EVENT_VP]==0 && vp[0x0016]==0 && video_touch_sample==0x5a01);
 event(0x0519);CHECK(vp[FW_UI_PASS_MASK]==0);touch(3);touch(2);advance(199);event(0x0519);CHECK(vp[FW_UI_PASS_MASK]==0);
 advance(1);unlock_password();
 touch(1);advance(120000);finish_snapshot();CHECK(page==42);
 touch(3);advance(120000);finish_snapshot();CHECK(page==42);
 touch(2);advance(59000);finish_snapshot();CHECK(page==42);
 touch(1);touch(2);advance(59000);finish_snapshot();CHECK(page==42);
 advance(1000);finish_snapshot();CHECK(page==1);
 open_locked_password();event(0x0522);CHECK(page==1);
 open_locked_password();unlock_password();
 touch(2);advance(59990);event(FW_UI_RUN_EVENT);CHECK(vp[FW_WRITE_TRIGGER_VP]==1);
 before=request_count;advance(10);CHECK(page==1 && vp[FW_WRITE_TRIGGER_VP]==0 && request_count==before);
 finish_snapshot();CHECK(device[14]==0);
}
static void test_password_other_pages_and_wrap(void) {
 unsigned i,before;static const uint16_t active[]={4,40,2,120};
 for(i=0;i<4;i++) {
  online();configure_password();go(42);
  if(active[i]==120)event(0x0400);else go(active[i]);
  expire_session();CHECK(page==active[i]);
  if(active[i]==120)event(0x0401);
  event(i<2?0x0200:0x022a);CHECK(page==1);open_locked_password();
  event(0x0522);CHECK(page==1);
 }
 online();configure_password();go(42);go(2);expire_session();
 event(0x0500);pass_digits("000001");event(0x0521);pass_digits("000001");event(0x0521);event(0x0300);FwProtocolTask();
 event(0x022a);CHECK(page==1);open_locked_password();pass_digits("000001");event(0x0521);reply();FwProtocolTask();finish_snapshot();CHECK(page==42);
 /* Expiry does not disable the currently displayed confirmation page. */
 online();configure_password();go(42);device[82]=1;device[37]=1;refresh_device();go(3);expire_session();
 before=request_count;event(0x0133);CHECK(request_count==before+1 && requests[before].value==51);
 online();configure_password();go(42);tick=UINT32_MAX-30000;touch(2);finish_snapshot();
 advance(59999);finish_snapshot();CHECK(page==42);advance(1);finish_snapshot();CHECK(page==1);
 /* Boot and controller requests for either operation page cannot bypass login. */
 online();configure_password();page=0;device[16]=42;FwProtocolInit();CHECK(page==1);FwProtocolTask();finish_snapshot();
 CHECK(page==1);device[16]=1;refresh_device();CHECK(page==1);
 open_locked_password();unlock_password();device[16]=9;refresh_device();expire_session();
 device[16]=1;refresh_device();CHECK(page==1);open_locked_password();unlock_password();
}
static void test_cylinder_temperature_text(void) {
 unsigned before;
 reset();CHECK(text_is(FW_UI_CYLINDER_TEXT_VP,"--") && text_is(FW_UI_SET_TEMP_TEXT_VP,"--"));
 FwProtocolTask();finish_snapshot();CHECK(text_is(FW_UI_CYLINDER_TEXT_VP,"0.0") && text_is(FW_UI_SET_TEMP_TEXT_VP,"-5.0"));
 device[30]=(uint16_t)-123;refresh_device();CHECK(text_is(FW_UI_CYLINDER_TEXT_VP,"-12.3") && text_is(FW_UI_SET_TEMP_TEXT_VP,"-5.0"));
 CHECK(text_is(0x3724,"-12.3"));
 /* Live FW5 changes must refresh on home/running pages without visiting settings. */
 device[5]=(uint16_t)-77;refresh_device();
 CHECK(page==42 && text_is(FW_UI_SET_TEMP_TEXT_VP,"-7.7"));
 device[5]=(uint16_t)-50;refresh_device();
 go(2);vp[FW_EDIT_VP+5]=(uint16_t)-65;advance(100);
 CHECK(text_is(FW_UI_CYLINDER_TEXT_VP,"-12.3") && text_is(FW_UI_SET_TEMP_TEXT_VP,"-5.0"));
 event(0x0300);reply();FwProtocolTask();finish_snapshot();
 CHECK(text_is(FW_UI_CYLINDER_TEXT_VP,"-12.3") && text_is(FW_UI_SET_TEMP_TEXT_VP,"-6.5"));
 go(42);device[16]=6;device[30]=(uint16_t)-30;refresh_device();
 CHECK(page==6 && text_is(FW_UI_CYLINDER_TEXT_VP,"-3.0") && text_is(FW_UI_SET_TEMP_TEXT_VP,"-6.5"));
 device[30]=0x8000;refresh_device();CHECK(text_is(FW_UI_CYLINDER_TEXT_VP,"-3276.8") && text_is(FW_UI_SET_TEMP_TEXT_VP,"-6.5"));
 CHECK(vp[FW_UI_CYLINDER_TEXT_VP+4]==0);
 advance(1000);before=request_count;advance(500);
 CHECK(request_count==before && text_is(FW_UI_CYLINDER_TEXT_VP,"--") && text_is(FW_UI_SET_TEMP_TEXT_VP,"-6.5"));
}
static void test_running_temperature_pages(void) {
 static const int16_t temperatures[]={-31,-30,-29,0,100,-300};
 unsigned i,j,before;
 for(j=0;j<2;j++) {
  online();device[39]=2+j;device[16]=j?30:6;
  for(i=0;i<sizeof(temperatures)/sizeof(temperatures[0]);i++) {
   device[30]=(uint16_t)temperatures[i];before=request_count;refresh_device();
   CHECK(page==(temperatures[i]<-30?30:6));
   CHECK(device[16]==(j?30:6));
   while(before<request_count) {CHECK(requests[before].command==3);++before;}
  }
  go(42);device[30]=(uint16_t)-31;refresh_device();CHECK(page==42);
  go(2);refresh_device();CHECK(page==2);
  device[16]=19;refresh_device();CHECK(page==19);
 }
 reset();device[16]=30;FwProtocolTask();finish_snapshot();CHECK(page==6);
 device[30]=(uint16_t)-31;refresh_device();CHECK(page==30);
 advance(1000);advance(500);CHECK(page==6);
 advance(3000);finish_snapshot();CHECK(page==30);
}
static void test_password_cancel_touch_release(void) {
 unsigned before;
 online();configure_password();go(42);expire_session();CHECK(page==1);
 touch(0);touch(2);touch(3);CHECK(page==1);touch(2);
 open_locked_password();touch(1);event(0x0522);CHECK(page==1);
 before=request_count;touch(3);touch(2);touch(0);advance(200);
 CHECK(page==1 && request_count==before);
 refresh_device();CHECK(page==1);
 open_locked_password();unlock_password();
}
static void test_manual_video_entry(void) {
 unsigned i;static const uint16_t pages[]={42,2,27,9};
 for(i=0;i<4;i++) {
  online();if(pages[i]!=42)go(pages[i]);advance(30000);finish_snapshot();CHECK(page==pages[i]);
  advance(60000);finish_snapshot();CHECK(page==pages[i]);
 }
 online();event(0x0400);CHECK(page==120);advance(60000);finish_snapshot();CHECK(page==120);
 event(0x0401);CHECK(page==9);
}
#include "fw_update_cases.h"
int main(void) {
 void (*updates[])(void)={test_setting_temperature_independent,test_advanced_cancel_delayed,test_alarm_batch_clear,test_power_return_offline,test_alarm_timestamp,test_admin_delayed_page_switch,test_daily_leading_zero,test_daily_schedule,test_daily_offline_and_migration,test_admin_password,test_admin_save_failure_and_factory_login,test_admin_backup_and_locked_pause_return,test_pause_controls,test_pause_queued_changes,test_defrost_display,test_power_alarm_confirmation,test_clock_and_level_settings,test_clock_page_drafts_and_schedule};
 void (*tests[])(void)={test_initial_and_poll,test_units_signed_and_bounds,test_edit_refresh_cancel,test_save_snapshot_and_repeat,test_partial_failure_and_recovery,test_read_timeout_cancels_queued_action,test_no_command_retry_and_fast_clicks,test_pages_local_and_unknown,test_alarm_mapping,test_warning_pages_and_return,test_warning_clear_fixed_mapping,test_warning_clear_invalid_and_timeout,test_warning_clear_queued_page_change,test_component_gates_and_noops,test_parser_split_crc_and_deadline,test_usb_independent};
 unsigned i;for(i=0;i<sizeof(updates)/sizeof(updates[0]);i++){updates[i]();if(failed_line){printf("FAIL update scenario %u line %u\n",i+1,failed_line);return 1;}}
 for(i=0;i<sizeof(tests)/sizeof(tests[0]);i++){tests[i]();if(failed_line){printf("FAIL scenario %u line %u\n",i+1,failed_line);return 1;}}
 test_result_belongs_to_original_page();if(failed_line){printf("FAIL page result line %u\n",failed_line);return 1;}++i;
 test_idle_keeps_edit_session_and_blocks_wake_action();if(failed_line){printf("FAIL idle edit line %u\n",failed_line);return 1;}++i;
 test_start_conditions();if(failed_line){printf("FAIL conditions line %u\n",failed_line);return 1;}++i;
 test_alarm_and_lid_gates();if(failed_line){printf("FAIL gates line %u\n",failed_line);return 1;}++i;
 test_queued_start_recheck();if(failed_line){printf("FAIL recheck line %u\n",failed_line);return 1;}++i;
 test_password_settings_and_persistence();if(failed_line){printf("FAIL password line %u\n",failed_line);return 1;}++i;
 test_password_host_and_navigation();if(failed_line){printf("FAIL password navigation line %u\n",failed_line);return 1;}++i;
 test_password_draft_and_timer_lid();if(failed_line){printf("FAIL drafts/timer line %u\n",failed_line);return 1;}++i;
 test_home_mode_matrix();if(failed_line){printf("FAIL home modes line %u\n",failed_line);return 1;}++i;
 test_home_alarm_temperature_and_data();if(failed_line){printf("FAIL home conditions line %u\n",failed_line);return 1;}++i;
 test_home_queued_rechecks();if(failed_line){printf("FAIL home queue line %u\n",failed_line);return 1;}++i;
 test_mode_page_returns();if(failed_line){printf("FAIL mode return line %u\n",failed_line);return 1;}++i;
 test_password_timeout_and_touch();if(failed_line){printf("FAIL timeout/touch line %u\n",failed_line);return 1;}++i;
 test_password_other_pages_and_wrap();if(failed_line){printf("FAIL timeout/navigation line %u\n",failed_line);return 1;}++i;
 test_manual_video_entry();if(failed_line){printf("FAIL manual video line %u\n",failed_line);return 1;}++i;
 test_cylinder_temperature_text();if(failed_line){printf("FAIL cylinder text line %u\n",failed_line);return 1;}++i;
 test_running_temperature_pages();if(failed_line){printf("FAIL running temperature line %u\n",failed_line);return 1;}++i;
 test_password_cancel_touch_release();if(failed_line){printf("FAIL cancel touch line %u\n",failed_line);return 1;}++i;
 printf("FW: %u scenarios, %u assertions passed\n",i+(unsigned)(sizeof(updates)/sizeof(updates[0])),checks);return 0;
}

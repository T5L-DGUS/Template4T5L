/* Exercise the production state machine with fixed-width host types and UART/VP mocks. */
#include "fw_test_platform.h"

typedef struct { uint8_t command; uint16_t address, value; } Request;
UART_TYPE Uart2 = {2}, Uart_R11 = {11};
uint8_t ET0 = 1;
static uint16_t vp[0x4000], page, device[89];
static uint32_t tick;
static Request requests[256];
static unsigned request_count, usb_count, checks, failed_line;
static uint8_t usb_frame[6];

#define CHECK(condition) do { ++checks; if (!(condition)) { failed_line = __LINE__; return; } } while (0)

uint32_t GetSysTick(void) { return tick; }
uint16_t ReadPageId(void) { return page; }
void SwitchPageById(uint16_t p) { page=p; }
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
    if (uart != &Uart2 || station != 1 || request_count == 256) { failed_line = __LINE__; return; }
    requests[request_count].command = 3;
    requests[request_count].address = address;
    requests[request_count++].value = count;
}
void SendModbusWriteSingleRegisterFrame(UART_TYPE *uart, uint8_t station,
                                      uint16_t address, uint16_t value)
{
    if (uart != &Uart2 || station != 1 || request_count == 256) { failed_line = __LINE__; return; }
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
    device[16]=1;device[80]=30;device[81]=100;device[83]=700;device[85]=30;device[44]=0xffff;
    page = 1; tick = start; request_count = usb_count = 0;
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
static void go(uint16_t p) { event(0x0200|p); reply(); FwProtocolTask(); finish_snapshot(); }
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
 before=request_count;advance(999);CHECK(request_count==before);advance(1);finish_snapshot();CHECK(request_count==before+7);
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
 reply();CHECK(page==1);
}
static void test_save_snapshot_and_repeat(void) {
 unsigned before;
 online();go(27);vp[FW_EDIT_VP]=21;vp[FW_EDIT_VP+1]=22;event(0x0300);
 CHECK(requests[request_count-1].address==0);CHECK(requests[request_count-1].value==21);
 before=request_count;event(0x0300);CHECK(request_count==before);vp[FW_EDIT_VP+1]=23;
 reply();FwProtocolTask();CHECK(requests[request_count-1].address==1);CHECK(requests[request_count-1].value==22);
 reply();FwProtocolTask();finish_snapshot();CHECK(vp[FW_EDIT_VP+1]==23);CHECK(vp[FW_UI_RESULT_VP]==2);
 event(0x0300);CHECK(requests[request_count-1].address==1);CHECK(requests[request_count-1].value==23);
}
static void test_partial_failure_and_recovery(void) {
 unsigned before;
 online();go(27);vp[FW_EDIT_VP]=21;vp[FW_EDIT_VP+1]=22;event(0x0300);reply();FwProtocolTask();exception();
 CHECK(device[0]==21 && device[1]==20);CHECK(vp[FW_UI_RESULT_VP]==3);CHECK(vp[FW_UI_READY_VP]==0);
 before=request_count;advance(2999);CHECK(request_count==before);advance(1);CHECK(requests[request_count-1].command==3);finish_snapshot();
 CHECK(vp[FW_EDIT_VP]==21 && vp[FW_EDIT_VP+1]==22);event(0x0300);CHECK(requests[request_count-1].command==6 && requests[request_count-1].address==1);
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
 device[16]=9;advance(1000);finish_snapshot();CHECK(page==9);event(0x0278);CHECK(page==9);CHECK(request_count==before+21);
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
  before=request_count;event(0x0200);CHECK(page==ps[i]);CHECK(requests[before].address==16 && requests[before].value==1);
  reply();CHECK(page==1);
 }
 online();go(42);device[16]=43;advance(1000);finish_snapshot();event(0x0200);CHECK(requests[request_count-1].value==42);reply();CHECK(page==42);
 /* A warning can replace FW16 before the first read following a navigation ACK. */
 online();event(0x022a);reply();CHECK(page==42);
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
 online();FwUsbProtocol(&Uart2,f,6);CHECK(!vp[USB_VIDEO_VP]);FwUsbProtocol(&Uart_R11,f,6);CHECK(vp[USB_VIDEO_VP]);
 vp[USB_VIDEO_GO_VP]=1;FwProtocolTask();CHECK(usb_count==1 && usb_frame[5]==1);
 f[5]=2;FwUsbProtocol(&Uart_R11,f,6);vp[USB_SOFTWARE_GO_VP]=1;FwProtocolTask();CHECK(usb_count==2 && usb_frame[5]==2);
 f[5]=3;FwUsbProtocol(&Uart_R11,f,6);CHECK(vp[USB_COMPLETE_VP]);CHECK(!vp[USB_VIDEO_VP]);
}
extern int printf(const char *,...);
int main(void) {
 void (*tests[])(void)={test_initial_and_poll,test_units_signed_and_bounds,test_edit_refresh_cancel,test_save_snapshot_and_repeat,test_partial_failure_and_recovery,test_read_timeout_cancels_queued_action,test_no_command_retry_and_fast_clicks,test_pages_local_and_unknown,test_alarm_mapping,test_warning_pages_and_return,test_warning_clear_fixed_mapping,test_warning_clear_invalid_and_timeout,test_warning_clear_queued_page_change,test_component_gates_and_noops,test_parser_split_crc_and_deadline,test_usb_independent};
 unsigned i;for(i=0;i<sizeof(tests)/sizeof(tests[0]);i++){tests[i]();if(failed_line){printf("FAIL scenario %u line %u\n",i+1,failed_line);return 1;}}
 printf("FW: %u scenarios, %u assertions passed\n",i,checks);return 0;
}

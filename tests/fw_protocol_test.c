/* Exercise the production state machine with fixed-width host types and UART/VP mocks. */
#include "fw_test_platform.h"

typedef struct { uint8_t command; uint16_t address, value; } Request;
UART_TYPE Uart2 = {2}, Uart_R11 = {11};
uint8_t ET0 = 1;
static uint16_t vp[0x4000], page;
static uint32_t tick;
static Request requests[256];
static unsigned request_count, usb_count, checks, failed_line;
static uint8_t usb_frame[6];

#define CHECK(condition) do { ++checks; if (!(condition)) { failed_line = __LINE__; return; } } while (0)

uint32_t GetSysTick(void) { return tick; }
uint16_t ReadPageId(void) { return page; }
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
    if (address == 0 || address == 1) return 20;
    if (address == 14) return 7;
    return address;
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
    FwProtocolFeed(frame, length);
}
static void reset_at(uint32_t start)
{
    unsigned i;
    for (i = 0; i < sizeof(vp) / sizeof(vp[0]); ++i) vp[i] = 0;
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
static void idle_for(uint32_t milliseconds)
{
    uint32_t elapsed;
    for (elapsed = 0; elapsed < milliseconds; elapsed += 10) {
        tick += 10; FwProtocolTask();
    }
}
static void submit(uint16_t address, uint16_t value)
{
    vp[FW_WRITE_ADDR_VP] = address; vp[FW_WRITE_VALUE_VP] = value;
    vp[FW_WRITE_TRIGGER_VP] = 1; FwProtocolTask();
}
static void test_snapshot_ranges_and_idle(void)
{
    static const uint8_t starts[] = {0,4,8,11,13,16,21,30,39,41,50,53,69,79,85};
    static const uint8_t counts[] = {2,2,2,1,2,4,1,8,1,5,2,1,9,5,1};
    unsigned i;
    reset(); ET0 = 0; FwProtocolTask(); CHECK(ET0 == 0); ET0 = 1;
    CHECK(request_count == 1);
    finish_snapshot(); CHECK(!failed_line); CHECK(request_count == 15);
    for (i = 0; i < 15; ++i) {
        CHECK(requests[i].command == 3);
        CHECK(requests[i].address == starts[i]); CHECK(requests[i].value == counts[i]);
    }
    CHECK(vp[FW_MIRROR_VP + 33] == 33); CHECK(vp[FW_MIRROR_VP + 53] == 53);
    CHECK(vp[FW_MIRROR_VP + 3] == 0); CHECK(vp[FW_MIRROR_VP + 40] == 0);
    CHECK(vp[FW_MIRROR_VP + 52] == 0); CHECK(vp[FW_MIRROR_VP + 78] == 0);
    idle_for(10000); CHECK(request_count == 15); CHECK(ET0 == 1);
    ++page; FwProtocolTask(); CHECK(request_count == 16);
    finish_snapshot(); CHECK(request_count == 30);
    idle_for(3000); CHECK(request_count == 30);
}
static void test_pages_coalesce(void)
{
    unsigned i;
    reset(); FwProtocolTask();
    for (i = 0; i < 20; ++i) { ++page; FwProtocolTask(); }
    CHECK(request_count == 1);
    finish_snapshot(); CHECK(!failed_line); CHECK(request_count == 30);
    idle_for(5000); CHECK(request_count == 30);
}
static void test_write_priority_and_refresh(void)
{
    reset(); FwProtocolTask(); submit(0, 21);
    CHECK(request_count == 1); CHECK(vp[FW_WRITE_TRIGGER_VP] == 1);
    reply(); FwProtocolTask();
    CHECK(request_count == 2); CHECK(requests[1].command == 6);
    CHECK(requests[1].address == 0); CHECK(requests[1].value == 21);
    CHECK(vp[FW_WRITE_RESULT_VP] == 1);
    reply(); CHECK(vp[FW_WRITE_RESULT_VP] == 2); CHECK(vp[FW_MIRROR_VP] == 21);
    FwProtocolTask(); CHECK(request_count == 3);
    CHECK(requests[2].command == 3); CHECK(requests[2].address == 0);
    finish_snapshot(); CHECK(request_count == 17);
    idle_for(2000); CHECK(request_count == 17);
}
static void test_noop_unknown_and_commands(void)
{
    reset(); FwProtocolTask(); finish_snapshot(); CHECK(request_count == 15);
    submit(0, 20); CHECK(request_count == 15); CHECK(vp[FW_WRITE_RESULT_VP] == 2);
    idle_for(2000); CHECK(request_count == 15);
    submit(14, 7); CHECK(request_count == 16); CHECK(requests[15].command == 6);
    reply(); FwProtocolTask(); finish_snapshot(); CHECK(request_count == 31);
    submit(14, 7); CHECK(request_count == 32); CHECK(requests[31].command == 6);
    reset(); submit(0, 20);
    CHECK(request_count == 1); CHECK(requests[0].command == 6);
}
static void test_related_setting_invalidates_old_mirror(void)
{
    reset(); FwProtocolTask(); finish_snapshot();
    CHECK(vp[FW_MIRROR_VP + 1] == 20);
    submit(0, 21); CHECK(request_count == 16); reply();
    /* Register 0 may change register 1 remotely before a fresh read observes it. */
    submit(1, 20); CHECK(request_count == 17);
    CHECK(requests[16].command == 6); CHECK(requests[16].address == 1);
    CHECK(requests[16].value == 20);
    reply(); FwProtocolTask(); CHECK(request_count == 18);
    CHECK(requests[17].command == 3); CHECK(requests[17].address == 0);
    finish_snapshot(); CHECK(request_count == 32);
}
static void test_timeout_cancels_all_work(void)
{
    reset(); FwProtocolTask(); ++page; FwProtocolTask();
    tick = 499; FwProtocolTask(); CHECK(vp[FW_ERROR_VP] == 0); CHECK(request_count == 1);
    tick = 500; FwProtocolTask(); CHECK(vp[FW_ERROR_VP] == 0x100);
    idle_for(5000); CHECK(request_count == 1);
    ++page; FwProtocolTask(); CHECK(request_count == 2); CHECK(requests[1].address == 0);
    finish_snapshot(); CHECK(request_count == 16);
}
static void test_exception_and_uncertain_write(void)
{
    uint8_t frame[5] = {1,0x83,2,0,0};
    reset(); FwProtocolTask(); ++page; FwProtocolTask(); add_crc(frame, 5);
    FwProtocolFeed(frame, 5); FwProtocolTask();
    CHECK(vp[FW_ERROR_VP] == 2); idle_for(2000); CHECK(request_count == 1);
    ++page; FwProtocolTask(); finish_snapshot(); CHECK(request_count == 16);
    submit(0, 21); CHECK(request_count == 17);
    tick += 500; FwProtocolTask(); CHECK(vp[FW_WRITE_RESULT_VP] == 3);
    idle_for(2000); CHECK(request_count == 17);
    /* A timed-out write could have changed the device: old mirror must not suppress this. */
    submit(0, 20); CHECK(request_count == 18); CHECK(requests[17].command == 6);
    frame[1] = 0x86; frame[2] = 3; add_crc(frame, 5); FwProtocolFeed(frame, 5);
    CHECK(vp[FW_WRITE_RESULT_VP] == 3); CHECK(vp[FW_ERROR_VP] == 3);
    idle_for(2000); CHECK(request_count == 18);
}
static void test_validation(void)
{
    static const uint16_t rejected[] = {3,40,52,78,30,53,88,65535};
    unsigned i;
    reset(); FwProtocolTask(); finish_snapshot();
    for (i = 0; i < sizeof(rejected) / sizeof(rejected[0]); ++i) {
        submit(rejected[i], 20); CHECK(request_count == 15);
        CHECK(vp[FW_WRITE_RESULT_VP] == 3); CHECK(vp[FW_ERROR_VP] == 0x101);
    }
    submit(11, 3); CHECK(request_count == 15); CHECK(vp[FW_ERROR_VP] == 0x101);
    submit(5, (uint16_t)-151); CHECK(request_count == 15);
    submit(5, 151); CHECK(request_count == 15);
    reset(); submit(11, 2); CHECK(request_count == 1); CHECK(requests[0].command == 6);
    reset(); submit(11, 0); CHECK(request_count == 1); CHECK(requests[0].command == 6);
    reset(); submit(11, 1); CHECK(request_count == 1); CHECK(requests[0].command == 6);
    reset(); submit(5, (uint16_t)-150); CHECK(request_count == 1); CHECK(requests[0].value == (uint16_t)-150);
    reset(); submit(5, 150); CHECK(request_count == 1); CHECK(requests[0].command == 6);
}
static void test_malformed_and_split_frames(void)
{
    uint8_t frame[64], length, other[8] = {1,6,0,0,0,20,0,0};
    reset(); FwProtocolTask(); length = make_reply(frame);
    frame[length - 1] ^= 1; FwProtocolFeed(frame, length); FwProtocolTask();
    CHECK(request_count == 1); CHECK(vp[FW_MIRROR_VP] == 0);
    /* Valid CRC with a different function must not satisfy a pending read. */
    add_crc(other, 8); FwProtocolFeed(other, 8); FwProtocolTask(); CHECK(request_count == 1);
    /* Structurally valid short response, but wrong count for this request. */
    frame[0] = 1; frame[1] = 3; frame[2] = 2; frame[3] = 0; frame[4] = 20;
    add_crc(frame, 7); FwProtocolFeed(frame, 7); FwProtocolTask(); CHECK(request_count == 1);
    length = make_reply(frame); FwProtocolFeed(frame, 2); FwProtocolTask(); CHECK(request_count == 1);
    FwProtocolFeed(frame + 2, length - 2); FwProtocolTask(); CHECK(request_count == 2);
    CHECK(vp[FW_MIRROR_VP] == 20); CHECK(requests[1].address == 4);
    reset(); submit(0, 21); length = make_reply(frame);
    frame[5] = 22; add_crc(frame, length); FwProtocolFeed(frame, length); FwProtocolTask();
    CHECK(request_count == 1); CHECK(vp[FW_WRITE_RESULT_VP] == 1); CHECK(vp[FW_MIRROR_VP] == 0);
    length = make_reply(frame); FwProtocolFeed(frame, 4); FwProtocolFeed(frame + 4, length - 4);
    CHECK(vp[FW_WRITE_RESULT_VP] == 2); CHECK(vp[FW_MIRROR_VP] == 21);
}
static void test_wraparound_and_late_reply(void)
{
    uint8_t frame[64], length;
    reset_at(UINT32_MAX - 100); FwProtocolTask(); length = make_reply(frame);
    tick += 499; FwProtocolTask(); CHECK(request_count == 1); CHECK(vp[FW_ERROR_VP] == 0);
    ++tick; FwProtocolFeed(frame, length); CHECK(vp[FW_MIRROR_VP] == 0);
    FwProtocolTask(); CHECK(vp[FW_ERROR_VP] == 0x100);
    idle_for(3000); CHECK(request_count == 1);
    ++page; FwProtocolTask(); CHECK(request_count == 2);
    reply(); CHECK(vp[FW_MIRROR_VP] == 20);
}
static void test_usb_handshake(void)
{
    uint8_t frame[6] = {0xaa,0x55,0,2,0xfb,1};
    reset(); FwProtocolTask(); finish_snapshot();
    vp[USB_VIDEO_GO_VP] = vp[USB_SOFTWARE_GO_VP] = 1; FwProtocolTask(); CHECK(usb_count == 0);
    FwUsbProtocol(&Uart2, frame, 6); CHECK(vp[USB_VIDEO_VP] == 0);
    FwUsbProtocol(&Uart_R11, frame, 5); CHECK(vp[USB_VIDEO_VP] == 0);
    FwUsbProtocol(&Uart_R11, frame, 6); CHECK(vp[USB_VIDEO_VP] == 1);
    vp[USB_VIDEO_GO_VP] = 1;
    FwUsbProtocol(&Uart_R11, frame, 6); CHECK(vp[USB_VIDEO_GO_VP] == 1);
    FwProtocolTask(); CHECK(usb_count == 1); CHECK(usb_frame[5] == 1);
    CHECK(vp[USB_VIDEO_GO_VP] == 0);
    frame[5] = 2; FwUsbProtocol(&Uart_R11, frame, 6);
    vp[USB_SOFTWARE_GO_VP] = 1; FwProtocolTask(); CHECK(usb_count == 2); CHECK(usb_frame[5] == 2);
    frame[5] = 3; FwUsbProtocol(&Uart_R11, frame, 6);
    CHECK(vp[USB_COMPLETE_VP] == 1); CHECK(vp[USB_VIDEO_VP] == 0);
    vp[USB_VIDEO_GO_VP] = 1; FwProtocolTask(); CHECK(usb_count == 2);
    frame[5] = 0; FwUsbProtocol(&Uart_R11, frame, 6);
    CHECK(vp[USB_COMPLETE_VP] == 0); CHECK(vp[USB_SOFTWARE_VP] == 0);
    vp[USB_SOFTWARE_GO_VP] = 1; FwProtocolTask(); CHECK(usb_count == 2);
    CHECK(request_count == 15);
}

unsigned get_check_count(void) { return checks; }
unsigned run_tests(void)
{
    static void (*const tests[])(void) = {
        test_snapshot_ranges_and_idle, test_pages_coalesce, test_write_priority_and_refresh,
        test_noop_unknown_and_commands, test_related_setting_invalidates_old_mirror,
        test_timeout_cancels_all_work,
        test_exception_and_uncertain_write, test_validation, test_malformed_and_split_frames,
        test_wraparound_and_late_reply, test_usb_handshake
    };
    unsigned i;
    checks = failed_line = 0;
    for (i = 0; i < sizeof(tests) / sizeof(tests[0]); ++i) {
        tests[i]();
        if (failed_line) return failed_line;
    }
    return 0;
}
int main(void) { return run_tests() ? 1 : 0; }

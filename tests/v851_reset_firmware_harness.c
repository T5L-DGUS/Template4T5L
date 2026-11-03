#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "v851_protocol.c"
#include "v851_wifi.c"
#include "v851_tlv_app.c"
#include "crc_impl.h"

UART_TYPE Uart4;
static uint8_t vp[0x10000 * 2];
static uint8_t sent[16][2048];
static uint16_t sent_len[16];
static unsigned sent_count, writes;
static uint8_t hold_uart_busy;
static unsigned flash_writes, flash_inits;
static uint32_t mock_tick;
uint32_t GetSysTick(void) { return mock_tick; }
void SwitchPageById(uint8_t page) { (void)page; }
void write_dgus_vp(uint32_t addr, const uint8_t *data, uint16_t words)
{ assert(addr + words <= 0x10000); memcpy(vp + addr * 2, data, words * 2); ++writes; }
void read_dgus_vp(uint32_t addr, uint8_t *data, uint16_t words)
{ memcpy(data, vp + addr * 2, words * 2); }
void UartSendData(UART_TYPE *uart, const uint8_t *data, uint16_t length)
{ assert(sent_count < 16); memcpy(sent[sent_count], data, length); sent_len[sent_count++] = length;
  if(hold_uart_busy) uart->TxBusy = 1; }
uint8_t V851ControlInfoValidateSegment(uint8_t t, const uint8_t *p, uint16_t n)
{ (void)t; (void)p; (void)n; return 1; }
uint8_t V851ControlInfoApplySegment(uint8_t t, const uint8_t *p, uint16_t n)
{ (void)t; (void)p; (void)n; return 1; }
uint8_t V851ControlInfoReportStructType(uint8_t i) { return i; }
uint16_t V851ControlInfoBuildFields(uint8_t t, uint8_t *p, uint16_t n)
{ (void)t; (void)p; (void)n; return 0; }
uint16_t V851ControlInfoScanChanged(void) { return 0; }

/* Existing reset cases start after the independently tested startup frame. */
static void InitAfterStartupHandshake(void)
{
    mock_tick = 0;
    memset(&Uart4, 0, sizeof(Uart4));
    hold_uart_busy = 0;
    sent_count = 0;
    V851ProtocolInit();
    V851ProtocolTask();
    assert(sent_count == 1 && sent_len[0] == 9 &&
           sent[0][4] == 0x35 && sent[0][5] == 0x0A);
    sent_count = 0;
}

static uint8_t compare_passwd(uint8_t *password) { return memcmp(password, "1234", 4) == 0; }
static void Set_Input(uint8_t page, uint8_t control) { (void)page; (void)control; }
static void nor_flash_write(uint32_t addr, uint8_t *buffer, uint16_t words)
{
    unsigned i;
    (void)addr;
    assert(!V851ProtocolFactoryResetBusy());
    assert(!Uart4.TxBusy && Uart4.TxHead == Uart4.TxTail);
    assert(sent_count && sent[sent_count - 1][4] == 0x39);
    for(i = 0; i < words * 2; ++i) assert(buffer[i] == 0);
    ++flash_writes;
}
static void Flash_DataInit(void) { ++flash_inits; }
#include "factory_reset_stc.h"

static void test_key_to_wire_before_flash(void)
{
    unsigned i;
    uint16_t key = 0x2801;
    InitAfterStartupHandshake(); sent_count = flash_writes = flash_inits = 0;
    hold_uart_busy = 1;
    /* Wrong password cannot send or erase settings. */
    memcpy(vp + 0x5366 * 2, "0000", 4);
    memcpy(vp + 0x5300 * 2, &key, 2);
    TestResetKeyScan(); V851ProtocolTask();
    assert(!T5lStcServiceFactoryReset());
    assert(!sent_count && !flash_writes);
    /* Correct password consumes the key without entering blocking flash I/O. */
    memcpy(vp + 0x5366 * 2, "1234", 4);
    memcpy(vp + 0x5300 * 2, &key, 2);
    TestResetKeyScan();
    assert(vp[0x5300 * 2] == 0 && vp[0x5300 * 2 + 1] == 0);
    assert(V851ProtocolFactoryResetBusy());
    assert(T5lStcServiceFactoryReset() && !flash_writes);
    V851ProtocolTask();
    assert(sent_count == 1 && sent[0][4] == 0x39 && sent_len[0] == 14);
    assert(crc_16(sent[0] + 4, sent_len[0] - 4) == 0);
    /* Enqueued/started is not completed; repeated task turns cannot erase. */
    for(i = 0; i < 5; ++i)
    {
        V851ProtocolRequestFactoryReset();
        V851ProtocolTask();
        assert(T5lStcServiceFactoryReset() && !flash_writes);
    }
    assert(sent_count == 1);
    Uart4.TxBusy = 0; Uart4.TxHead = 1; Uart4.TxTail = 0;
    V851ProtocolTask();
    assert(T5lStcServiceFactoryReset() && !flash_writes);
    Uart4.TxTail = Uart4.TxHead;
    V851ProtocolTask();
    assert(!V851ProtocolFactoryResetBusy());
    assert(T5lStcServiceFactoryReset());
    assert(flash_writes == 4 && flash_inits == 1);
    assert(!T5lStcServiceFactoryReset());
    TestResetKeyScan(); V851ProtocolTask();
    assert(sent_count == 1);
    hold_uart_busy = 0;
}

static void firmware(const uint8_t *fields, uint16_t length)
{
    uint8_t frame[256] = {0xAA, 0x55, 0, 0, 0x35, 0x09, 0, 0};
    frame[3] = (uint8_t)(length + 4);
    frame[7] = (uint8_t)length;
    memcpy(frame + 8, fields, length);
    V851ProtocolReceiveFrame(frame, length + 8);
}

static void test_versions(void)
{
    const uint8_t both[] = {1,0,5,'1','.','0','.','0',4,0,5,'2','.','0','.','0'};
    const uint8_t shorter[] = {4,0,1,'3'};
    const uint8_t legacy[] = {1,0,1,'X'};
    const uint8_t reversed[] = {4,0,1,'4',1,0,1,'X'};
    const uint8_t empty[] = {4,0,0};
    const uint8_t malformed[] = {1,0,1,'X',4,0,4,'Y'};
    const uint8_t unknown[] = {2,0,1,'H',3,0,4,0,0,0,1,99,0,0};
    uint8_t long_field[43];
    uint8_t bad_second_segment[] = {0xAA,0x55,0,11,0x35,9,0,4,1,0,1,'X',9,0,9};
    unsigned old_writes, i;
    memset(vp, 0xA5, sizeof(vp));
    firmware(both, sizeof(both));
    assert(strcmp((char *)(vp + 0x5290 * 2), "2.0.0") == 0);
    assert(strcmp((char *)(vp + 0x52A0 * 2), "2.0.0") == 0);
    firmware(shorter, sizeof(shorter));
    for(i = 1; i < 32; ++i) assert(vp[0x5290 * 2 + i] == 0);
    assert(vp[0x5290 * 2] == '3');
    assert(memcmp(vp + 0x5290 * 2, vp + 0x52A0 * 2, 32) == 0);
    old_writes = writes;
    firmware(legacy, sizeof(legacy));
    assert(writes == old_writes);
    firmware(reversed, sizeof(reversed));
    assert(vp[0x5290 * 2] == '4');
    assert(memcmp(vp + 0x5290 * 2, vp + 0x52A0 * 2, 32) == 0);
    firmware(empty, sizeof(empty));
    for(i = 0; i < 32; ++i) { assert(vp[0x52A0 * 2 + i] == 0); assert(vp[0x5290 * 2 + i] == 0); }
    old_writes = writes;
    firmware(malformed, sizeof(malformed));
    firmware(unknown, sizeof(unknown));
    V851ProtocolReceiveFrame(bad_second_segment, sizeof(bad_second_segment));
    assert(writes == old_writes);
    /* Direct hook must also reject malformed segments without partial writes. */
    { V851TlvSegment segment = {9, malformed, sizeof(malformed)};
      V851TlvApplicationSegment(0x35, &segment); assert(writes == old_writes); }
    memset(long_field, 'V', sizeof(long_field));
    long_field[0] = 4; long_field[1] = 0;
    for(i = 31; i <= 40; ++i)
    {
        unsigned j;
        long_field[2] = (uint8_t)i;
        firmware(long_field, (uint16_t)(i + 3));
        for(j = 0; j < 31; ++j) assert(vp[0x5290 * 2 + j] == 'V');
        assert(vp[0x5290 * 2 + 31] == 0);
        assert(memcmp(vp + 0x5290 * 2, vp + 0x52A0 * 2, 32) == 0);
    }
    assert(vp[0x528F * 2] == 0xA5);
    assert(vp[0x52B0 * 2] == 0xA5);
}

static void test_reset(void)
{
    const uint8_t reset[] = {0xAA,0x55,0,10,0x39,0x6E,0,4,1,0,1,1,0x3C,0xDF};
    const uint8_t connect[] = {0xAA,0x55,0,1,0xC1};
    const uint8_t scan[] = {0xAA,0x55,0,3,0xC0,0,5};
    V851TlvSegment filler = {0x09, NULL, 0};
    unsigned combination, rotation, i, resets, scans;
    for(rotation = 0; rotation < 2; ++rotation)
    for(combination = 0; combination < 4; ++combination)
    {
        InitAfterStartupHandshake(); V851WifiInit(); sent_count = 0;
        if(rotation) { assert(V851ProtocolSendWifiFrame(scan, sizeof(scan))); V851ProtocolTask(); }
        sent_count = 0;
        Uart4.TxBusy = 1;
        for(i = 0; i < 2; ++i)
            assert(V851ProtocolSendWifiFrame((combination & (1U << i)) ? connect : scan,
                   (combination & (1U << i)) ? sizeof(connect) : sizeof(scan)));
        assert(V851ProtocolSendSegments(0x35, &filler, 1));
        assert(V851ProtocolSendSegments(0x35, &filler, 1));
        memset(vp + V851_WIFI_PASSWORD_ADDR * 2, 'P', 32);
        { uint16_t key = V851_WIFI_KEY_CONNECT;
          memcpy(vp + V851_WIFI_KEY_ADDR * 2, &key, 2); }
        V851ProtocolRequestFactoryReset();
        V851WifiTask();
        for(i = 0; i < 32; ++i) assert(vp[V851_WIFI_PASSWORD_ADDR * 2 + i] == 0);
        for(i = 0; i < 4; ++i) V851ProtocolTask();
        assert(sent_count == 0);
        Uart4.TxBusy = 0;
        for(i = 0; i < 12; ++i) V851ProtocolTask();
        resets = scans = 0;
        for(i = 0; i < sent_count; ++i)
        {
            assert(sent[i][4] != 0xC1);
            if(sent[i][4] == 0xC0) ++scans;
            if(sent[i][4] == 0x39)
            { ++resets; assert(sent_len[i] == sizeof(reset)); assert(memcmp(sent[i], reset, sizeof(reset)) == 0); }
        }
        assert(resets == 1);
        assert(scans == 2 - ((combination & 1) != 0) - ((combination & 2) != 0));
    }
}

static void test_reset_crc_capacity(void)
{
    static uint8_t fields[2039];
    V851TlvSegment segment = {0x6E, fields, sizeof(fields)};
    InitAfterStartupHandshake(); sent_count = 0;
    assert(!V851ProtocolSendSegments(0x39, &segment, 1));
    segment.length = 2038;
    fields[0] = 1; fields[1] = 7; fields[2] = 0xF3;
    assert(V851ProtocolSendSegments(0x39, &segment, 1));
    V851ProtocolTask();
    assert(sent_count == 1 && sent_len[0] == 2048);
    assert(crc_16(sent[0] + 4, sent_len[0] - 4) == 0);
    V851ProtocolTask();
}


#include "v851_handshake_cases.h"

int main(void)
{
    test_versions(); test_reset(); test_reset_crc_capacity(); test_key_to_wire_before_flash();
    test_handshake();
    puts("Production C firmware/reset/handshake behavior passed");
    return 0;
}

#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "v851_protocol.c"
#include "v851_control_info.c"
#include "crc_impl.h"

UART_TYPE Uart4;
DeviceCtrl G_Device_Ctrl;
static uint8_t vp[0x10000 * 2];
static uint16_t command_address[32];
static uint8_t command_value[32];
static unsigned command_count, other_controls, writes, tx_count;
static uint32_t mock_tick;

uint32_t GetSysTick(void) { return mock_tick; }
void read_dgus_vp(uint32_t addr, uint8_t *data, uint16_t words)
{ assert(addr + words <= 0x10000); memcpy(data, vp + addr * 2, words * 2); }
void write_dgus_vp(uint32_t addr, const uint8_t *data, uint16_t words)
{ assert(addr + words <= 0x10000); memcpy(vp + addr * 2, data, words * 2); ++writes; }
void UartSendData(UART_TYPE *uart, const uint8_t *data, uint16_t length)
{ (void)uart; (void)data; (void)length; ++tx_count; }
void V851WifiClearCredentials(void) {}
void V851TlvApplicationFactoryReset(void) {}
void V851TlvApplicationSegment(uint8_t command, const V851TlvSegment *segment)
{ (void)command; (void)segment; }
uint8_t T5lStcSyncMappedControl(T5lStcMappedControl control, uint8_t mask,
                               uint16_t enabled, uint16_t secondary, uint16_t tertiary)
{ (void)control; (void)mask; (void)enabled; (void)secondary; (void)tertiary; return 1; }
void Exhaust_On(void) { ++other_controls; }
void Exhaust_Off(uint8_t automatic) { (void)automatic; ++other_controls; }
void Light_On(uint16_t value) { (void)value; ++other_controls; }
void Light_Off(void) { ++other_controls; }
void UVB_On(uint16_t value) { (void)value; ++other_controls; }
void UVB_Off(uint8_t automatic) { (void)automatic; ++other_controls; }
void Heater_On(int16_t value) { (void)value; ++other_controls; }
void Heater_Off(uint8_t automatic) { (void)automatic; ++other_controls; }
void Humidifier_On(void) { ++other_controls; }
void Humidifier_Off(uint8_t automatic) { (void)automatic; ++other_controls; }
void InWind_On(uint16_t value) { (void)value; ++other_controls; }
void InWind_Off(void) { ++other_controls; }
static void Send_Cmd_Ctrl(uint16_t address, uint8_t value)
{
    assert(command_count < 32);
    command_address[command_count] = address;
    command_value[command_count++] = value;
}
/* Execute the real switch helper, including its inverted enable bookkeeping. */
#include "switch_impl.h"

static void receive(const uint8_t *fields, uint16_t length)
{
    uint8_t frame[128] = {0xAA, 0x55, 0, 0, 0x35, 0x70, 0, 0};
    assert(length <= sizeof(frame) - 8);
    frame[3] = (uint8_t)(length + 4);
    frame[7] = (uint8_t)length;
    memcpy(frame + 8, fields, length);
    V851ProtocolReceiveFrame(frame, length + 8);
}

static void test_partial_updates(void)
{
    const uint8_t both[] = {0xAA,0x55,0,12,0x35,0x70,0,8,1,0,1,1,2,0,1,1};
    const uint8_t mic_off[] = {1,0,1,0};
    const uint8_t camera_off[] = {2,0,1,0};
    const uint8_t reversed[] = {2,0,1,1,1,0,1,1};
    const uint8_t unknown[] = {0x7F,0,2,0xAB,0xCD};
    const uint8_t unknown_and_mic[] = {0x7F,0,0,1,0,1,0};
    const uint8_t no_led_segment[] = {0xAA,0x55,0,4,0x35,0x62,0,0};
    unsigned before;

    G_Device_Ctrl.MicLed.enable = G_Device_Ctrl.CameraLed.enable = 1;
    V851ProtocolReceiveFrame(both, sizeof(both));
    assert(command_count == 2 && !writes && !other_controls && !tx_count);
    assert(command_address[0] == 0x610E && command_value[0] == 1);
    assert(command_address[1] == 0x610F && command_value[1] == 1);
    assert(G_Device_Ctrl.MicLed.enable == 0 && G_Device_Ctrl.CameraLed.enable == 0);

    receive(mic_off, sizeof(mic_off));
    assert(command_count == 3 && command_address[2] == 0x610E && command_value[2] == 0);
    assert(G_Device_Ctrl.MicLed.enable == 1 && G_Device_Ctrl.CameraLed.enable == 0);
    receive(camera_off, sizeof(camera_off));
    assert(command_count == 4 && command_address[3] == 0x610F && command_value[3] == 0);
    assert(G_Device_Ctrl.MicLed.enable == 1 && G_Device_Ctrl.CameraLed.enable == 1);
    receive(reversed, sizeof(reversed));
    assert(command_count == 6 && command_address[4] == 0x610F && command_address[5] == 0x610E);
    assert(G_Device_Ctrl.MicLed.enable == 0 && G_Device_Ctrl.CameraLed.enable == 0);

    before = command_count;
    receive(unknown, 0); /* Empty structure. */
    receive(unknown, sizeof(unknown));
    V851ProtocolReceiveFrame(no_led_segment, sizeof(no_led_segment));
    assert(command_count == before && !writes && !other_controls);
    assert(G_Device_Ctrl.MicLed.enable == 0 && G_Device_Ctrl.CameraLed.enable == 0);
    receive(unknown_and_mic, sizeof(unknown_and_mic));
    assert(command_count == before + 1 && command_address[before] == 0x610E);
    assert(G_Device_Ctrl.MicLed.enable == 1 && G_Device_Ctrl.CameraLed.enable == 0);

    /* An explicit repeated update still commands the requested lamp. */
    receive(mic_off, sizeof(mic_off));
    assert(command_count == before + 2 && command_value[before + 1] == 0);

    /* Initialization/idle/reporting must never infer or send LED controls. */
    before = command_count;
    V851ProtocolInit();
    V851ProtocolTask();
    mock_tick = 500;
    V851ProtocolTask();
    mock_tick = 1000;
    V851ProtocolTask();
    assert(command_count == before && tx_count == 1); /* Startup notice only. */
    assert(G_Device_Ctrl.MicLed.enable == 1 && G_Device_Ctrl.CameraLed.enable == 0);
}

static void test_invalid_frames_are_atomic(void)
{
    const uint8_t bad_value[] = {1,0,1,1,2,0,1,2};
    const uint8_t empty_value[] = {1,0,0};
    const uint8_t long_value[] = {2,0,2,0,1};
    const uint8_t truncated[] = {1,0,1,1,2,0,2,1};
    const uint8_t duplicate[] = {1,0,1,0,1,0,1,1};
    const uint8_t duplicate_camera[] = {2,0,1,0,2,0,1,1};
    uint8_t frame[] = {0xAA,0x55,0,8,0x35,0x70,0,4,1,0,1,1};
    /* A valid LED segment followed by an invalid actuator must apply neither. */
    uint8_t mixed[] = {0xAA,0x55,0,15,0x35,0x70,0,4,1,0,1,1,0x62,0,4,1,0,1,2};
    /* An invalid LED segment must also prevent an earlier valid actuator update. */
    const uint8_t bad_led[] = {0xAA,0x55,0,15,0x35,0x62,0,4,1,0,1,1,0x70,0,4,1,0,1,2};
    unsigned before = command_count;
    unsigned old_writes = writes;
    receive(bad_value, sizeof(bad_value));
    receive(empty_value, sizeof(empty_value));
    receive(long_value, sizeof(long_value));
    receive(truncated, sizeof(truncated));
    receive(duplicate, sizeof(duplicate));
    receive(duplicate_camera, sizeof(duplicate_camera));
    V851ProtocolReceiveFrame(mixed, sizeof(mixed));
    V851ProtocolReceiveFrame(bad_led, sizeof(bad_led));
    frame[3]--; /* Incorrect frame length. */
    V851ProtocolReceiveFrame(frame, sizeof(frame));
    frame[3]++;
    frame[7]++; /* Incorrect segment length. */
    V851ProtocolReceiveFrame(frame, sizeof(frame));
    frame[7]--;
    frame[4] = 0x36;
    V851ProtocolReceiveFrame(frame, sizeof(frame));
    frame[4] = 0x37;
    V851ProtocolReceiveFrame(frame, sizeof(frame));
    assert(!V851ControlInfoApplySegment(0x70, truncated, sizeof(truncated)));
    assert(!V851ControlInfoApplySegment(0x70, NULL, 0));
    assert(command_count == before && writes == old_writes && !other_controls);
    assert(G_Device_Ctrl.MicLed.enable == 1 && G_Device_Ctrl.CameraLed.enable == 0);

    /* Valid mixed frames still apply each supplied control. */
    mixed[18] = 1;
    V851ProtocolReceiveFrame(mixed, sizeof(mixed));
    assert(command_count == before + 1 && other_controls == 1 && writes == old_writes + 1);
    assert(command_address[before] == 0x610E && command_value[before] == 1);
}

int main(void)
{
    test_partial_updates();
    test_invalid_frames_are_atomic();
    puts("indicator LED protocol tests passed");
    return 0;
}

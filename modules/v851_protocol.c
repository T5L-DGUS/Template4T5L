#include "v851_protocol.h"

#if v851PROTOCOL_ENABLED

#include "timer.h"
#include "uart.h"
#include "v851_control_info.h"
#include "v851_wifi.h"

#include <string.h>

#pragma optimize(8, size)

#define V851_TLV_TX_DEPTH                         2U
#define V851_WIFI_TX_DEPTH                        2U
#define V851_WIFI_TX_MAX                          96U
#define V851_STATE_SCAN_INTERVAL_MS               500UL
#define V851_STATE_FULL_INTERVAL_MS               60000UL
// #define V851_STATE_FULL_INTERVAL_MS               20000UL
#define V851_ALARM_SCAN_INTERVAL_MS               500UL
#define V851_ALARM_VP_BASE                        0x3080UL
#define V851_ALARM_VP_COUNT                       10U
#define V851_FACTORY_FIELD_MAX_BYTES              80U
#define V851_EVENT_ALARM_FIELD_MAX_BYTES          48U

typedef char V851FactoryCountryLengthCheck[
    ((sizeof(v851FACTORY_SALES_COUNTRY) - 1U) <= 7U) ? 1 : -1];
typedef char V851FactoryProductKeyLengthCheck[
    ((sizeof(v851FACTORY_PRODUCT_KEY) - 1U) <= 63U) ? 1 : -1];
typedef char V851FactoryModelLengthCheck[
    ((sizeof(v851FACTORY_MODEL) - 1U) <= 63U) ? 1 : -1];
typedef char V851FactoryHardwareLengthCheck[
    ((sizeof(v851FACTORY_HARDWARE_VERSION) - 1U) <= 31U) ? 1 : -1];
typedef char V851FactoryFirmwareLengthCheck[
    ((sizeof(v851FACTORY_FIRMWARE_VERSION) - 1U) <= 31U) ? 1 : -1];

static uint8_t xdata v851_tlv_tx[V851_TLV_TX_DEPTH][V851_TLV_FRAME_MAX];
static uint16_t v851_tlv_tx_len[V851_TLV_TX_DEPTH];
static uint8_t v851_tlv_tx_head;
static uint8_t v851_tlv_tx_tail;
static uint8_t v851_tlv_tx_count;

static uint8_t xdata v851_wifi_tx[V851_WIFI_TX_DEPTH][V851_WIFI_TX_MAX];
static uint16_t v851_wifi_tx_len[V851_WIFI_TX_DEPTH];
static uint8_t v851_wifi_tx_head;
static uint8_t v851_wifi_tx_tail;
static uint8_t v851_wifi_tx_count;

static uint8_t xdata v851_state_fields[V851_CONTROL_COUNT]
                                      [V851_CONTROL_FIELD_MAX_BYTES];
static uint16_t v851_state_dirty_mask;
static uint32_t v851_state_scan_tick;
static uint32_t v851_state_full_tick;

static uint8_t v851_factory_report_pending;
static uint8_t v851_factory_reset_pending;
static uint16_t xdata v851_alarm_current[V851_ALARM_VP_COUNT];
static uint16_t xdata v851_alarm_reported[V851_ALARM_VP_COUNT];
static uint8_t v851_alarm_next_index;
static uint32_t v851_alarm_scan_tick;

static void V851WriteBe16(uint8_t *bytes, uint16_t value)
{
    bytes[0] = (uint8_t)(value >> 8);
    bytes[1] = (uint8_t)value;
}

void V851TlvFieldCursorInit(V851TlvFieldCursor *cursor,
                            const uint8_t *bytes,
                            uint16_t length)
{
    if(cursor != NULL)
    {
        cursor->bytes = bytes;
        cursor->length = length;
        cursor->offset = 0U;
    }
}

V851TlvIterResult V851TlvFieldNext(V851TlvFieldCursor *cursor,
                                   V851TlvField *field)
{
    uint16_t field_length;
    uint16_t remaining;

    if((cursor == NULL) || (field == NULL) ||
       ((cursor->bytes == NULL) && (cursor->length != 0U)))
    {
        return V851_TLV_ITER_INVALID;
    }
    if(cursor->offset == cursor->length)
    {
        return V851_TLV_ITER_END;
    }
    if(cursor->offset > cursor->length)
    {
        return V851_TLV_ITER_INVALID;
    }

    remaining = (uint16_t)(cursor->length - cursor->offset);
    if(remaining < V851_TLV_FIELD_HEADER_SIZE)
    {
        return V851_TLV_ITER_INVALID;
    }
    field_length = (uint16_t)(((uint16_t)cursor->bytes[cursor->offset + 1U] << 8) |
                              cursor->bytes[cursor->offset + 2U]);
    if(field_length > (uint16_t)(remaining - V851_TLV_FIELD_HEADER_SIZE))
    {
        return V851_TLV_ITER_INVALID;
    }

    field->tag = cursor->bytes[cursor->offset];
    field->length = field_length;
    field->value = &cursor->bytes[cursor->offset + V851_TLV_FIELD_HEADER_SIZE];
    cursor->offset = (uint16_t)(cursor->offset +
                               V851_TLV_FIELD_HEADER_SIZE + field_length);
    return V851_TLV_ITER_FIELD;
}

uint8_t V851TlvWriteBytes(uint8_t *buffer, uint16_t capacity,
                          uint16_t *offset, uint8_t tag,
                          const uint8_t *value, uint16_t length)
{
    uint16_t required;

    if((buffer == NULL) || (offset == NULL) ||
       ((value == NULL) && (length != 0U)))
    {
        return 0U;
    }
    required = (uint16_t)(V851_TLV_FIELD_HEADER_SIZE + length);
    if((*offset > capacity) || (required > (uint16_t)(capacity - *offset)))
    {
        return 0U;
    }
    buffer[*offset] = tag;
    V851WriteBe16(&buffer[*offset + 1U], length);
    if(length != 0U)
    {
        memcpy(&buffer[*offset + V851_TLV_FIELD_HEADER_SIZE], value, length);
    }
    *offset = (uint16_t)(*offset + required);
    return 1U;
}

uint8_t V851TlvWriteU8(uint8_t *buffer, uint16_t capacity,
                       uint16_t *offset, uint8_t tag, uint8_t value)
{
    return V851TlvWriteBytes(buffer, capacity, offset, tag, &value, 1U);
}

uint8_t V851TlvWriteU32(uint8_t *buffer, uint16_t capacity,
                        uint16_t *offset, uint8_t tag, uint32_t value)
{
    uint8_t encoded[4];

    encoded[0] = (uint8_t)(value >> 24);
    encoded[1] = (uint8_t)(value >> 16);
    encoded[2] = (uint8_t)(value >> 8);
    encoded[3] = (uint8_t)value;
    return V851TlvWriteBytes(buffer, capacity, offset, tag, encoded, 4U);
}

uint8_t V851TlvWriteBinary64Uint16(uint8_t *buffer, uint16_t capacity,
                                   uint16_t *offset, uint8_t tag,
                                   uint16_t value)
{
    uint8_t encoded[8];
    uint8_t exponent;
    uint8_t bit_index;
    uint8_t mantissa_bit;
    uint32_t high_word;
    uint32_t low_word;

    high_word = 0UL;
    low_word = 0UL;
    if(value != 0U)
    {
        exponent = 0U;
        while((exponent < 15U) &&
              (((uint16_t)1U << (exponent + 1U)) <= value))
        {
            ++exponent;
        }
        high_word = (uint32_t)(1023U + exponent) << 20;
        for(bit_index = 0U; bit_index < exponent; ++bit_index)
        {
            if((value & ((uint16_t)1U << bit_index)) != 0U)
            {
                mantissa_bit = (uint8_t)(52U - exponent + bit_index);
                if(mantissa_bit >= 32U)
                {
                    high_word |= (uint32_t)1UL << (mantissa_bit - 32U);
                }
                else
                {
                    low_word |= (uint32_t)1UL << mantissa_bit;
                }
            }
        }
    }

    encoded[0] = (uint8_t)(high_word >> 24);
    encoded[1] = (uint8_t)(high_word >> 16);
    encoded[2] = (uint8_t)(high_word >> 8);
    encoded[3] = (uint8_t)high_word;
    encoded[4] = (uint8_t)(low_word >> 24);
    encoded[5] = (uint8_t)(low_word >> 16);
    encoded[6] = (uint8_t)(low_word >> 8);
    encoded[7] = (uint8_t)low_word;
    return V851TlvWriteBytes(buffer, capacity, offset, tag, encoded, 8U);
}

uint8_t V851TlvReadBinary64Uint16(const uint8_t *value,
                                  uint16_t length,
                                  uint16_t *result)
{
    uint32_t high_word;
    uint32_t low_word;
    uint32_t integer_value;
    uint16_t exponent_bits;
    int16_t exponent;
    int16_t integer_bit;
    uint8_t mantissa_bit;
    uint8_t is_set;

    if((value == NULL) || (result == NULL) || (length != 8U))
    {
        return 0U;
    }
    high_word = ((uint32_t)value[0] << 24) |
                ((uint32_t)value[1] << 16) |
                ((uint32_t)value[2] << 8) | value[3];
    low_word = ((uint32_t)value[4] << 24) |
               ((uint32_t)value[5] << 16) |
               ((uint32_t)value[6] << 8) | value[7];
    if((high_word & 0x80000000UL) != 0UL)
    {
        return 0U;
    }
    exponent_bits = (uint16_t)((high_word >> 20) & 0x07FFUL);
    if(exponent_bits == 0U)
    {
        if(((high_word & 0x000FFFFFUL) == 0UL) && (low_word == 0UL))
        {
            *result = 0U;
            return 1U;
        }
        return 0U;
    }
    if(exponent_bits == 0x07FFU)
    {
        return 0U;
    }
    exponent = (int16_t)exponent_bits - 1023;
    if((exponent < 0) || (exponent > 15))
    {
        return 0U;
    }

    integer_value = (uint32_t)1UL << (uint8_t)exponent;
    for(mantissa_bit = 0U; mantissa_bit < 52U; ++mantissa_bit)
    {
        if(mantissa_bit >= 32U)
        {
            is_set = (uint8_t)((high_word >> (mantissa_bit - 32U)) & 1UL);
        }
        else
        {
            is_set = (uint8_t)((low_word >> mantissa_bit) & 1UL);
        }
        if(is_set != 0U)
        {
            integer_bit = (int16_t)exponent -
                          (int16_t)(52U - mantissa_bit);
            if((integer_bit < 0) || (integer_bit > 15))
            {
                return 0U;
            }
            integer_value |= (uint32_t)1UL << (uint8_t)integer_bit;
        }
    }
    if(integer_value > 0xFFFFUL)
    {
        return 0U;
    }
    *result = (uint16_t)integer_value;
    return 1U;
}

uint8_t V851ProtocolSendSegments(uint8_t command,
                                 const V851TlvSegment *segments,
                                 uint8_t count)
{
    uint16_t segment_bytes;
    uint16_t frame_length;
    uint16_t offset;
    uint8_t index;
    uint8_t slot;

    if((segments == NULL) || (count == 0U) ||
       ((command != V851_TLV_CMD_PROPERTY) &&
        (command != V851_TLV_CMD_FACTORY) &&
        (command != V851_TLV_CMD_SNAPSHOT) &&
         (command != V851_TLV_CMD_EVENT_ALARM) &&
         (command != V851_TLV_CMD_FACTORY_RESET)) ||
       (v851_tlv_tx_count >= V851_TLV_TX_DEPTH))
    {
        return 0U;
    }

    segment_bytes = 0U;
    for(index = 0U; index < count; ++index)
    {
        if((command == V851_TLV_CMD_SNAPSHOT) &&
           (segments[index].struct_type != V851_TLV_STRUCT_ENVIRONMENT) &&
           ((segments[index].struct_type < V851_TLV_STRUCT_EXHAUST) ||
            (segments[index].struct_type > V851_TLV_STRUCT_FILTER)))
        {
            return 0U;
        }
        if(((command == V851_TLV_CMD_FACTORY) &&
            (segments[index].struct_type != V851_TLV_STRUCT_FACTORY_DEVICE)) ||
           ((command == V851_TLV_CMD_EVENT_ALARM) &&
            (segments[index].struct_type != V851_TLV_STRUCT_EVENT_ALARM)) ||
           ((command == V851_TLV_CMD_FACTORY_RESET) &&
            (segments[index].struct_type != V851_TLV_STRUCT_FACTORY_RESET)))
        {
            return 0U;
        }
        if((segments[index].payload == NULL) && (segments[index].length != 0U))
        {
            return 0U;
        }
        if((segment_bytes > (V851_TLV_FRAME_MAX -
                             V851_TLV_FRAME_FIXED_SIZE -
                             V851_TLV_SEGMENT_HEADER_SIZE)) ||
           (segments[index].length >
           (uint16_t)(V851_TLV_FRAME_MAX - V851_TLV_FRAME_FIXED_SIZE -
                      V851_TLV_SEGMENT_HEADER_SIZE - segment_bytes)))
        {
            return 0U;
        }
        segment_bytes = (uint16_t)(segment_bytes +
                                   V851_TLV_SEGMENT_HEADER_SIZE +
                                   segments[index].length);
    }
    frame_length = (uint16_t)(V851_TLV_FRAME_FIXED_SIZE + segment_bytes);
    if(frame_length > V851_TLV_FRAME_MAX)
    {
        return 0U;
    }

    slot = v851_tlv_tx_head;
    v851_tlv_tx[slot][0] = V851_FRAME_MAGIC_HIGH;
    v851_tlv_tx[slot][1] = V851_FRAME_MAGIC_LOW;
    /* 最新AA55协议的长度包含命令字，总帧长为4 + length。 */
    V851WriteBe16(&v851_tlv_tx[slot][2], (uint16_t)(segment_bytes + 1U));
    v851_tlv_tx[slot][4] = command;
    offset = V851_TLV_FRAME_FIXED_SIZE;
    for(index = 0U; index < count; ++index)
    {
        v851_tlv_tx[slot][offset++] = segments[index].struct_type;
        V851WriteBe16(&v851_tlv_tx[slot][offset], segments[index].length);
        offset += 2U;
        if(segments[index].length != 0U)
        {
            memcpy(&v851_tlv_tx[slot][offset], segments[index].payload,
                   segments[index].length);
            offset = (uint16_t)(offset + segments[index].length);
        }
    }
    v851_tlv_tx_len[slot] = frame_length;
    v851_tlv_tx_head = (uint8_t)((v851_tlv_tx_head + 1U) % V851_TLV_TX_DEPTH);
    ++v851_tlv_tx_count;
    return 1U;
}

uint8_t V851ProtocolSendWifiFrame(const uint8_t *frame, uint16_t len)
{
    uint8_t slot;

    if((frame == NULL) || (len < 5U) || (len > V851_WIFI_TX_MAX) ||
       (v851_wifi_tx_count >= V851_WIFI_TX_DEPTH))
    {
        return 0U;
    }
    slot = v851_wifi_tx_head;
    memcpy(v851_wifi_tx[slot], frame, len);
    v851_wifi_tx_len[slot] = len;
    v851_wifi_tx_head = (uint8_t)((v851_wifi_tx_head + 1U) %
                                  V851_WIFI_TX_DEPTH);
    ++v851_wifi_tx_count;
    return 1U;
}

static uint8_t V851ProtocolValidateFields(const uint8_t *field_bytes,
                                          uint16_t length)
{
    V851TlvFieldCursor cursor;
    V851TlvField field;
    V851TlvIterResult result;

    V851TlvFieldCursorInit(&cursor, field_bytes, length);
    for(;;)
    {
        result = V851TlvFieldNext(&cursor, &field);
        if(result == V851_TLV_ITER_END)
        {
            return 1U;
        }
        if(result != V851_TLV_ITER_FIELD)
        {
            return 0U;
        }
    }
}

void V851ProtocolReceiveFrame(const uint8_t *frame, uint16_t len)
{
    V851TlvSegment segment;
    uint16_t declared_length;
    uint16_t segment_length;
    uint16_t offset;
    uint8_t command;

    if((frame == NULL) || (len < (V851_TLV_FRAME_FIXED_SIZE +
                                  V851_TLV_SEGMENT_HEADER_SIZE)) ||
       (len > V851_TLV_FRAME_MAX) ||
       (frame[0] != V851_FRAME_MAGIC_HIGH) ||
       (frame[1] != V851_FRAME_MAGIC_LOW))
    {
        return;
    }
    declared_length = (uint16_t)(((uint16_t)frame[2] << 8) | frame[3]);
    command = frame[4];
    /* V851 RX length includes the command byte: total = 4 + length. */
    if((declared_length !=
        (uint16_t)(len - V851_TLV_RX_LENGTH_BASE_SIZE)) ||
        ((command != V851_TLV_CMD_PROPERTY) &&
         (command != V851_TLV_CMD_FACTORY) &&
         (command != V851_TLV_CMD_BOOTSTRAP_RESULT) &&
         (command != V851_TLV_CMD_BOOTSTRAP_RESULT_COMPAT)))
    {
        return;
    }

    if(command == V851_TLV_CMD_BOOTSTRAP_RESULT_COMPAT)
    {
        return;
    }

    /* First pass: reject the complete frame before any business write. */
    offset = V851_TLV_FRAME_FIXED_SIZE;
    while(offset < len)
    {
        if((uint16_t)(len - offset) < V851_TLV_SEGMENT_HEADER_SIZE)
        {
            return;
        }
        segment.struct_type = frame[offset];
        segment_length = (uint16_t)(((uint16_t)frame[offset + 1U] << 8) |
                                    frame[offset + 2U]);
        offset = (uint16_t)(offset + V851_TLV_SEGMENT_HEADER_SIZE);
        if(segment_length > (uint16_t)(len - offset))
        {
            return;
        }
        segment.payload = &frame[offset];
        segment.length = segment_length;
        if(V851ProtocolValidateFields(segment.payload, segment.length) == 0U)
        {
            return;
        }
        if((command == V851_TLV_CMD_PROPERTY) &&
           (segment.struct_type >= V851_TLV_STRUCT_EXHAUST) &&
           (segment.struct_type <= V851_TLV_STRUCT_FILTER) &&
           (V851ControlInfoValidateSegment(segment.struct_type,
                                           segment.payload,
                                           segment.length) == 0U))
        {
            return;
        }
        offset = (uint16_t)(offset + segment_length);
    }
    if(offset != len)
    {
        return;
    }

    /* Second pass: the complete frame is known to be well formed. */
    offset = V851_TLV_FRAME_FIXED_SIZE;
    while(offset < len)
    {
        segment.struct_type = frame[offset];
        segment.length = (uint16_t)(((uint16_t)frame[offset + 1U] << 8) |
                                    frame[offset + 2U]);
        segment.payload = &frame[offset + V851_TLV_SEGMENT_HEADER_SIZE];
        if((command == V851_TLV_CMD_PROPERTY) &&
           (segment.struct_type >= V851_TLV_STRUCT_EXHAUST) &&
           (segment.struct_type <= V851_TLV_STRUCT_FILTER))
        {
            (void)V851ControlInfoApplySegment(segment.struct_type,
                                               segment.payload,
                                               segment.length);
        }
        V851TlvApplicationSegment(command, &segment);
        offset = (uint16_t)(offset + V851_TLV_SEGMENT_HEADER_SIZE +
                            segment.length);
    }
}

static uint16_t V851ProtocolQueueStateMask(uint8_t command,
                                           uint16_t requested_mask)
{
    V851TlvSegment segments[V851_CONTROL_COUNT];
    uint16_t included_mask;
    uint16_t length;
    uint8_t index;
    uint8_t count;

    included_mask = 0U;
    count = 0U;
    for(index = 0U; index < V851_CONTROL_COUNT; ++index)
    {
        if((requested_mask & ((uint16_t)1U << index)) != 0U)
        {
            segments[count].struct_type =
                V851ControlInfoReportStructType(index);
            length = V851ControlInfoBuildFields(
                segments[count].struct_type,
                v851_state_fields[index], sizeof(v851_state_fields[index]));
            if(length != 0U)
            {
                segments[count].payload = v851_state_fields[index];
                segments[count].length = length;
                included_mask |= (uint16_t)1U << index;
                ++count;
            }
        }
    }
    if((count == 0U) ||
       ((command == V851_TLV_CMD_SNAPSHOT) &&
        (included_mask != requested_mask)))
    {
        return 0U;
    }
    if(V851ProtocolSendSegments(command, segments, count) != 0U)
    {
        return included_mask;
    }
    return 0U;
}

void V851ProtocolRequestFactoryReset(void)
{
    uint8_t read_slot;
    uint8_t write_slot;
    uint8_t index;
    uint8_t kept;

    /* Compact the ring, retaining scan requests but removing old credentials. */
    read_slot = v851_wifi_tx_tail;
    write_slot = read_slot;
    kept = 0U;
    for(index = 0U; index < v851_wifi_tx_count; ++index)
    {
        if(v851_wifi_tx[read_slot][4] != V851_WIFI_CMD_CONNECT)
        {
            if(write_slot != read_slot)
            {
                memcpy(v851_wifi_tx[write_slot], v851_wifi_tx[read_slot],
                       v851_wifi_tx_len[read_slot]);
            }
            v851_wifi_tx_len[write_slot] = v851_wifi_tx_len[read_slot];
            write_slot = (uint8_t)((write_slot + 1U) % V851_WIFI_TX_DEPTH);
            ++kept;
        }
        read_slot = (uint8_t)((read_slot + 1U) % V851_WIFI_TX_DEPTH);
    }
    v851_wifi_tx_head = write_slot;
    v851_wifi_tx_count = kept;
    for(index = kept; index < V851_WIFI_TX_DEPTH; ++index)
    {
        memset(v851_wifi_tx[write_slot], 0, V851_WIFI_TX_MAX);
        v851_wifi_tx_len[write_slot] = 0U;
        write_slot = (uint8_t)((write_slot + 1U) % V851_WIFI_TX_DEPTH);
    }
    V851WifiClearCredentials();
    v851_factory_reset_pending = 1U;
}

static uint8_t V851ProtocolServiceFactoryReset(void)
{
    V851TlvSegment segment;
    uint8_t fields[4];
    uint16_t offset;

    if(v851_factory_reset_pending == 0U)
    {
        return 0U;
    }
    offset = 0U;
    if(V851TlvWriteU8(fields, sizeof(fields), &offset,
                      V851_TLV_TAG_RESET_ACTION,
                      V851_TLV_RESET_WIFI_AND_UNBIND) != 0U)
    {
        segment.struct_type = V851_TLV_STRUCT_FACTORY_RESET;
        segment.payload = fields;
        segment.length = offset;
        if(V851ProtocolSendSegments(V851_TLV_CMD_FACTORY_RESET,
                                     &segment, 1U) != 0U)
        {
            v851_factory_reset_pending = 0U;
        }
    }
    return 1U;
}

void V851ProtocolRequestFactoryReport(void)
{
    v851_factory_report_pending = 1U;
}

static uint8_t V851ProtocolQueueFactoryReport(void)
{
    V851TlvSegment segment;
    uint8_t fields[V851_FACTORY_FIELD_MAX_BYTES];
    uint16_t offset;

    offset = 0U;
    if((V851TlvWriteBytes(fields, sizeof(fields), &offset,
                          V851_TLV_TAG_FACTORY_SALES_COUNTRY,
                          (const uint8_t *)v851FACTORY_SALES_COUNTRY,
                          (uint16_t)(sizeof(v851FACTORY_SALES_COUNTRY) - 1U)) == 0U) ||
       (V851TlvWriteBytes(fields, sizeof(fields), &offset,
                          V851_TLV_TAG_FACTORY_PRODUCT_KEY,
                          (const uint8_t *)v851FACTORY_PRODUCT_KEY,
                          (uint16_t)(sizeof(v851FACTORY_PRODUCT_KEY) - 1U)) == 0U) ||
       (V851TlvWriteBytes(fields, sizeof(fields), &offset,
                          V851_TLV_TAG_FACTORY_MODEL,
                          (const uint8_t *)v851FACTORY_MODEL,
                          (uint16_t)(sizeof(v851FACTORY_MODEL) - 1U)) == 0U) ||
       (V851TlvWriteBytes(fields, sizeof(fields), &offset,
                          V851_TLV_TAG_FACTORY_HW_VERSION,
                          (const uint8_t *)v851FACTORY_HARDWARE_VERSION,
                          (uint16_t)(sizeof(v851FACTORY_HARDWARE_VERSION) - 1U)) == 0U) ||
       (V851TlvWriteBytes(fields, sizeof(fields), &offset,
                          V851_TLV_TAG_FACTORY_FW_VERSION,
                          (const uint8_t *)v851FACTORY_FIRMWARE_VERSION,
                          (uint16_t)(sizeof(v851FACTORY_FIRMWARE_VERSION) - 1U)) == 0U))
    {
        return 0U;
    }

    segment.struct_type = V851_TLV_STRUCT_FACTORY_DEVICE;
    segment.payload = fields;
    segment.length = offset;
    return V851ProtocolSendSegments(V851_TLV_CMD_FACTORY, &segment, 1U);
}

static uint8_t V851ProtocolServiceFactory(void)
{
    if(v851_factory_report_pending == 0U)
    {
        return 0U;
    }
    if(V851ProtocolQueueFactoryReport() != 0U)
    {
        v851_factory_report_pending = 0U;
    }
    return 1U;
}

static uint8_t V851ProtocolGetAlarmInfo(uint8_t vp_index,
                                        uint16_t value,
                                        const char **alarm_code,
                                        const char **alarm_level)
{
    *alarm_code = NULL;
    *alarm_level = NULL;
    switch(vp_index)
    {
    case 1U:
        *alarm_level = "HIGH";
        switch(value)
        {
        case 1U: *alarm_code = "TEMP_HIGH"; break;
        case 2U: *alarm_code = "TEMP_LOW"; break;
        case 3U: *alarm_code = "TEMP_SENSOR_FAULT"; break;
        case 4U: *alarm_code = "HEATER_FAULT"; break;
        case 5U:
            *alarm_code = "OVERHEAT_PROTECTION";
            *alarm_level = "CRITICAL";
            break;
        default: break;
        }
        break;
    case 2U:
        *alarm_level = "HIGH";
        switch(value)
        {
        case 1U:
            *alarm_code = "LIQUID_LOW_WARNING";
            *alarm_level = "MEDIUM";
            break;
        case 2U:
            *alarm_code = "LOW_LIQUID";
            *alarm_level = "MEDIUM";
            break;
        case 3U: *alarm_code = "NEBULIZER_DRY_BURN"; break;
        case 4U: *alarm_code = "WATER_LEVEL_SENSOR_FAULT"; break;
        case 5U: *alarm_code = "HUMIDITY_SENSOR_FAULT"; break;
        default: break;
        }
        break;
    case 4U:
        if(value == 1U)
        {
            *alarm_code = "EXHAUST_FAN_FAULT";
            *alarm_level = "HIGH";
        }
        break;
    case 5U:
        if(value == 1U)
        {
            *alarm_code = "INLET_FAN_FAULT";
            *alarm_level = "HIGH";
        }
        break;
    case 7U:
        if(value == 1U)
        {
            *alarm_code = "FILTER_LIFE_EXHAUSTED";
            *alarm_level = "MEDIUM";
        }
        break;
    default:
        break;
    }
    return (*alarm_code != NULL) ? 1U : 0U;
}

/* 返回0表示队列忙，1表示成功，2表示该VP值没有协议映射。 */
static uint8_t V851ProtocolQueueEventAlarm(uint8_t vp_index,
                                           uint16_t value,
                                           uint8_t recovered)
{
    V851TlvSegment segment;
    const char *alarm_code;
    const char *alarm_level;
    uint8_t fields[V851_EVENT_ALARM_FIELD_MAX_BYTES];
    uint16_t offset;

    if(V851ProtocolGetAlarmInfo(vp_index, value,
                                &alarm_code, &alarm_level) == 0U)
    {
        return 2U;
    }

    offset = 0U;
    if((V851TlvWriteU8(fields, sizeof(fields), &offset,
                       V851_TLV_TAG_EA_IS_ALARM, 1U) == 0U) ||
       (V851TlvWriteBytes(fields, sizeof(fields), &offset,
                          V851_TLV_TAG_EA_CODE,
                          (const uint8_t *)alarm_code,
                          (uint16_t)strlen(alarm_code)) == 0U) ||
       (V851TlvWriteBytes(fields, sizeof(fields), &offset,
                          V851_TLV_TAG_EA_LEVEL,
                          (const uint8_t *)alarm_level,
                          (uint16_t)strlen(alarm_level)) == 0U) ||
       (V851TlvWriteU8(fields, sizeof(fields), &offset,
                       V851_TLV_TAG_EA_RECOVERED, recovered) == 0U))
    {
        return 0U;
    }

    segment.struct_type = V851_TLV_STRUCT_EVENT_ALARM;
    segment.payload = fields;
    segment.length = offset;
    return V851ProtocolSendSegments(V851_TLV_CMD_EVENT_ALARM, &segment, 1U);
}

static uint8_t V851ProtocolServiceAlarm(uint32_t tick)
{
    uint16_t current;
    uint16_t reported;
    uint8_t index;
    uint8_t queue_result;
    uint8_t scanned;

    if((uint32_t)(tick - v851_alarm_scan_tick) >=
       V851_ALARM_SCAN_INTERVAL_MS)
    {
        v851_alarm_scan_tick = tick;
        read_dgus_vp(V851_ALARM_VP_BASE,
                     (uint8_t *)v851_alarm_current,
                     V851_ALARM_VP_COUNT);
    }

    for(scanned = 0U; scanned < V851_ALARM_VP_COUNT; ++scanned)
    {
        index = (uint8_t)((v851_alarm_next_index + scanned) %
                          V851_ALARM_VP_COUNT);
        current = v851_alarm_current[index];
        reported = v851_alarm_reported[index];

        if((reported != 0U) && (reported != current))
        {
            queue_result = V851ProtocolQueueEventAlarm(index, reported, 1U);
            if(queue_result == 1U)
            {
                v851_alarm_reported[index] = 0U;
                v851_alarm_next_index = (uint8_t)((index + 1U) %
                                                   V851_ALARM_VP_COUNT);
            }
            return 1U;
        }
        if((current != 0U) && (reported != current))
        {
            queue_result = V851ProtocolQueueEventAlarm(index, current, 0U);
            if(queue_result == 1U)
            {
                v851_alarm_reported[index] = current;
                v851_alarm_next_index = (uint8_t)((index + 1U) %
                                                   V851_ALARM_VP_COUNT);
                return 1U;
            }
            if(queue_result == 0U)
            {
                return 1U;
            }
        }
    }
    return 0U;
}

static void V851ProtocolServiceState(uint32_t tick)
{
    uint16_t sent_mask;

    if((uint32_t)(tick - v851_state_scan_tick) >=
       V851_STATE_SCAN_INTERVAL_MS)
    {
        v851_state_scan_tick = tick;
        v851_state_dirty_mask |= V851ControlInfoScanChanged();
    }

    if(v851_state_dirty_mask != 0U)
    {
        sent_mask = V851ProtocolQueueStateMask(V851_TLV_CMD_PROPERTY,
                                                v851_state_dirty_mask);
        if(sent_mask != 0U)
        {
            v851_state_dirty_mask &= (uint16_t)~sent_mask;
        }
        return;
    }

#if v851STATE_FULL_REPORT_ENABLED
    if((uint32_t)(tick - v851_state_full_tick) >=
       V851_STATE_FULL_INTERVAL_MS)
    {
        sent_mask = V851ProtocolQueueStateMask(V851_TLV_CMD_SNAPSHOT,
                                                V851_CONTROL_FULL_MASK);
        if(sent_mask == V851_CONTROL_FULL_MASK)
        {
            v851_state_full_tick = tick;
        }
    }
#endif
}

static void V851ProtocolServiceTx(void)
{
    uint8_t slot;

    if((Uart4.TxBusy != 0U) || (Uart4.TxHead != Uart4.TxTail))
    {
        return;
    }
    if(v851_wifi_tx_count != 0U)
    {
        slot = v851_wifi_tx_tail;
        UartSendData(&Uart4, v851_wifi_tx[slot], v851_wifi_tx_len[slot]);
        v851_wifi_tx_tail = (uint8_t)((v851_wifi_tx_tail + 1U) %
                                      V851_WIFI_TX_DEPTH);
        --v851_wifi_tx_count;
        return;
    }
    if(v851_tlv_tx_count != 0U)
    {
        slot = v851_tlv_tx_tail;
        UartSendData(&Uart4, v851_tlv_tx[slot], v851_tlv_tx_len[slot]);
        v851_tlv_tx_tail = (uint8_t)((v851_tlv_tx_tail + 1U) %
                                     V851_TLV_TX_DEPTH);
        --v851_tlv_tx_count;
    }
}

void V851ProtocolInit(void)
{
    uint32_t tick;

    memset(v851_tlv_tx_len, 0, sizeof(v851_tlv_tx_len));
    memset(v851_wifi_tx_len, 0, sizeof(v851_wifi_tx_len));
    v851_tlv_tx_head = 0U;
    v851_tlv_tx_tail = 0U;
    v851_tlv_tx_count = 0U;
    v851_wifi_tx_head = 0U;
    v851_wifi_tx_tail = 0U;
    v851_wifi_tx_count = 0U;
    v851_state_dirty_mask = 0U;
    v851_factory_report_pending = 0U;
    v851_factory_reset_pending = 0U;
    memset(v851_alarm_current, 0, sizeof(v851_alarm_current));
    memset(v851_alarm_reported, 0, sizeof(v851_alarm_reported));
    v851_alarm_next_index = 0U;
    tick = GetSysTick();
    v851_state_scan_tick = tick;
    v851_state_full_tick = tick;
    v851_alarm_scan_tick = tick;
}

void V851ProtocolTask(void)
{
    uint32_t tick;

    tick = GetSysTick();
    if((V851ProtocolServiceFactoryReset() == 0U) &&
       (V851ProtocolServiceAlarm(tick) == 0U))
    {
        if(V851ProtocolServiceFactory() == 0U)
        {
            V851ProtocolServiceState(tick);
        }
    }
    V851ProtocolServiceTx();
}

#endif /* v851PROTOCOL_ENABLED */

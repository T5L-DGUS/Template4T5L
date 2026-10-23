#include "v851_control_info.h"

#if v851PROTOCOL_ENABLED

#include <string.h>
#include "t5l_stc.h"

#pragma optimize(8, size)

#define V851_CONTROL_MAPPED_ENABLED              0x01U
#define V851_CONTROL_MAPPED_SECONDARY            0x02U
#define V851_CONTROL_MAPPED_TERTIARY             0x04U

#define V851_CONTROL_HOURS_EXHAUST               0U
#define V851_CONTROL_HOURS_HUMI_INTERVAL         1U
#define V851_CONTROL_HOURS_HUMI_RUNNING          2U

static uint8_t xdata v851_control_report_shadow[V851_CONTROL_COUNT]
                                                [V851_CONTROL_REPORT_MAX_BYTES];
static uint16_t v851_control_report_shadow_valid_mask;

static uint16_t V851ControlInfoReadBe16(const uint8_t *bytes)
{
    return (uint16_t)(((uint16_t)bytes[0] << 8) | bytes[1]);
}

static void V851ControlInfoWriteBe16(uint8_t *bytes, uint16_t value)
{
    bytes[0] = (uint8_t)(value >> 8);
    bytes[1] = (uint8_t)value;
}

static uint8_t V851ControlInfoIndex(uint8_t struct_type)
{
    if((struct_type < V851_TLV_STRUCT_EXHAUST) ||
       (struct_type > V851_TLV_STRUCT_FILTER))
    {
        return 0xFFU;
    }
    return (uint8_t)(struct_type - V851_TLV_STRUCT_EXHAUST);
}

uint8_t V851ControlInfoReportStructType(uint8_t report_index)
{
    if(report_index == V851_CONTROL_ENVIRONMENT_INDEX)
    {
        return V851_TLV_STRUCT_ENVIRONMENT;
    }
    if((report_index >= V851_CONTROL_ACTUATOR_BASE_INDEX) &&
       (report_index < V851_CONTROL_COUNT))
    {
        return (uint8_t)(V851_TLV_STRUCT_EXHAUST + report_index -
                         V851_CONTROL_ACTUATOR_BASE_INDEX);
    }
    return 0U;
}

static uint32_t V851ControlInfoCommandAddress(uint8_t struct_type)
{
    switch(struct_type)
    {
        case V851_TLV_STRUCT_EXHAUST:
            return V851_CONTROL_COMMAND_EXHAUST_ADDR;
        case V851_TLV_STRUCT_LIGHT:
            return V851_CONTROL_COMMAND_LIGHT_ADDR;
        case V851_TLV_STRUCT_UVB:
            return V851_CONTROL_COMMAND_UVB_ADDR;
        case V851_TLV_STRUCT_ANION:
            return V851_CONTROL_COMMAND_ANION_ADDR;
        case V851_TLV_STRUCT_PLASMA:
            return V851_CONTROL_COMMAND_PLASMA_ADDR;
        case V851_TLV_STRUCT_CLIMATE:
            return V851_CONTROL_COMMAND_CLIMATE_ADDR;
        case V851_TLV_STRUCT_HUMIDIFIER:
            return V851_CONTROL_COMMAND_HUMIDIFIER_ADDR;
        case V851_TLV_STRUCT_INLET_FAN:
            return V851_CONTROL_COMMAND_INLET_FAN_ADDR;
        default:
            return 0UL;
    }
}

static uint8_t V851ControlInfoCommandWords(uint8_t struct_type)
{
    switch(struct_type)
    {
        case V851_TLV_STRUCT_EXHAUST:
        case V851_TLV_STRUCT_UVB:
        case V851_TLV_STRUCT_HUMIDIFIER:
            return 3U;
        case V851_TLV_STRUCT_LIGHT:
        case V851_TLV_STRUCT_CLIMATE:
        case V851_TLV_STRUCT_INLET_FAN:
            return 2U;
        case V851_TLV_STRUCT_ANION:
        case V851_TLV_STRUCT_PLASMA:
            return 1U;
        default:
            return 0U;
    }
}

static uint32_t V851ControlInfoReportAddress(uint8_t struct_type)
{
    switch(struct_type)
    {
        case V851_TLV_STRUCT_ENVIRONMENT:
            return V851_CONTROL_REPORT_ENVIRONMENT_ADDR;
        case V851_TLV_STRUCT_EXHAUST:
            return V851_CONTROL_REPORT_EXHAUST_ADDR;
        case V851_TLV_STRUCT_LIGHT:
            return V851_CONTROL_REPORT_LIGHT_ADDR;
        case V851_TLV_STRUCT_UVB:
            return V851_CONTROL_REPORT_UVB_ADDR;
        case V851_TLV_STRUCT_ANION:
            return V851_CONTROL_REPORT_ANION_ADDR;
        case V851_TLV_STRUCT_PLASMA:
            return V851_CONTROL_REPORT_PLASMA_ADDR;
        case V851_TLV_STRUCT_CLIMATE:
            return V851_CONTROL_REPORT_CLIMATE_ADDR;
        case V851_TLV_STRUCT_HUMIDIFIER:
            return V851_CONTROL_REPORT_HUMIDIFIER_ADDR;
        case V851_TLV_STRUCT_INLET_FAN:
            return V851_CONTROL_REPORT_INLET_FAN_ADDR;
        case V851_TLV_STRUCT_FILTER:
            return V851_CONTROL_REPORT_FILTER_ADDR;
        default:
            return 0UL;
    }
}

static uint8_t V851ControlInfoReportWords(uint8_t struct_type)
{
    switch(struct_type)
    {
        case V851_TLV_STRUCT_ENVIRONMENT:
            return 3U;
        case V851_TLV_STRUCT_HUMIDIFIER:
            return 5U;
        case V851_TLV_STRUCT_EXHAUST:
        case V851_TLV_STRUCT_UVB:
        case V851_TLV_STRUCT_CLIMATE:
            return 4U;
        case V851_TLV_STRUCT_LIGHT:
        case V851_TLV_STRUCT_INLET_FAN:
            return 3U;
        case V851_TLV_STRUCT_ANION:
        case V851_TLV_STRUCT_PLASMA:
        case V851_TLV_STRUCT_FILTER:
            return 2U;
        default:
            return 0U;
    }
}

static uint8_t V851ControlInfoReadHalfHours(const uint8_t *value,
                                            uint16_t length,
                                            uint16_t *half_hours)
{
    uint16_t integer_hours;
    uint8_t index;

    if((value == NULL) || (half_hours == NULL) || (length != 8U))
    {
        return 0U;
    }
    if((value[0] == 0x3FU) && (value[1] == 0xE0U))
    {
        for(index = 2U; index < 8U; ++index)
        {
            if(value[index] != 0U)
            {
                return 0U;
            }
        }
        *half_hours = 1U;
        return 1U;
    }
    if((V851TlvReadBinary64Uint16(value, length, &integer_hours) == 0U) ||
       (integer_hours > 32767U))
    {
        return 0U;
    }
    *half_hours = (uint16_t)(integer_hours * 2U);
    return 1U;
}

static uint8_t V851ControlInfoHoursToCode(uint8_t hours_type,
                                          const uint8_t *value,
                                          uint16_t length,
                                          uint16_t *code_value)
{
    uint16_t half_hours;

    if(V851ControlInfoReadHalfHours(value, length, &half_hours) == 0U)
    {
        return 0U;
    }
    switch(hours_type)
    {
        case V851_CONTROL_HOURS_EXHAUST:
            switch(half_hours)
            {
                case 1U:  *code_value = 1U; return 1U;
                case 2U:  *code_value = 2U; return 1U;
                case 4U:  *code_value = 3U; return 1U;
                case 8U:  *code_value = 4U; return 1U;
                case 16U: *code_value = 5U; return 1U;
                default: return 0U;
            }
        case V851_CONTROL_HOURS_HUMI_INTERVAL:
            switch(half_hours)
            {
                case 4U:  *code_value = 1U; return 1U;
                case 8U:  *code_value = 2U; return 1U;
                case 16U: *code_value = 3U; return 1U;
                case 24U: *code_value = 4U; return 1U;
                default: return 0U;
            }
        case V851_CONTROL_HOURS_HUMI_RUNNING:
            switch(half_hours)
            {
                case 1U: *code_value = 1U; return 1U;
                case 2U: *code_value = 2U; return 1U;
                case 4U: *code_value = 3U; return 1U;
                default: return 0U;
            }
        default:
            return 0U;
    }
}

static uint8_t V851ControlInfoWriteHours(uint8_t *buffer,
                                         uint16_t capacity,
                                         uint16_t *offset,
                                         uint8_t tag,
                                         uint8_t hours_type,
                                         uint16_t code_value)
{
    uint8_t encoded[8];
    uint16_t half_hours;

    half_hours = 0U;
    switch(hours_type)
    {
        case V851_CONTROL_HOURS_EXHAUST:
            switch(code_value)
            {
                case 1U: half_hours = 1U; break;
                case 2U: half_hours = 2U; break;
                case 3U: half_hours = 4U; break;
                case 4U: half_hours = 8U; break;
                case 5U: half_hours = 16U; break;
                default: return 0U;
            }
            break;
        case V851_CONTROL_HOURS_HUMI_INTERVAL:
            switch(code_value)
            {
                case 1U: half_hours = 4U; break;
                case 2U: half_hours = 8U; break;
                case 3U: half_hours = 16U; break;
                case 4U: half_hours = 24U; break;
                default: return 0U;
            }
            break;
        case V851_CONTROL_HOURS_HUMI_RUNNING:
            switch(code_value)
            {
                case 1U: half_hours = 1U; break;
                case 2U: half_hours = 2U; break;
                case 3U: half_hours = 4U; break;
                default: return 0U;
            }
            break;
        default:
            return 0U;
    }

    if(half_hours == 1U)
    {
        encoded[0] = 0x3FU;
        encoded[1] = 0xE0U;
        memset(&encoded[2], 0, 6U);
        return V851TlvWriteBytes(buffer, capacity, offset, tag, encoded, 8U);
    }
    return V851TlvWriteBinary64Uint16(buffer, capacity, offset, tag,
                                      (uint16_t)(half_hours / 2U));
}

static uint8_t V851ControlInfoMapWritableField(uint8_t struct_type,
                                               const V851TlvField *field,
                                               uint8_t *mapped_bit,
                                               uint16_t *mapped_value)
{
    uint16_t value;

    *mapped_bit = 0U;
    *mapped_value = 0U;
    switch(struct_type)
    {
        case V851_TLV_STRUCT_EXHAUST:
            if(field->tag == V851_TLV_TAG_EXH_ENABLED)
            {
                if((field->length != 1U) || (field->value[0] > 1U)) return 0U;
                *mapped_bit = V851_CONTROL_MAPPED_ENABLED;
                *mapped_value = field->value[0];
            }
            else if(field->tag == V851_TLV_TAG_EXH_SPEED_LEVEL)
            {
                if((field->length != 1U) || (field->value[0] < 1U) ||
                   (field->value[0] > 6U)) return 0U;
                *mapped_bit = V851_CONTROL_MAPPED_SECONDARY;
                *mapped_value = field->value[0];
            }
            else if(field->tag == V851_TLV_TAG_EXH_INTERVAL_HOURS)
            {
                if(V851ControlInfoHoursToCode(V851_CONTROL_HOURS_EXHAUST,
                                               field->value, field->length,
                                               mapped_value) == 0U) return 0U;
                *mapped_bit = V851_CONTROL_MAPPED_TERTIARY;
            }
            else if(field->tag == V851_TLV_TAG_EXH_RUNNING)
            {
                if((field->length != 1U) || (field->value[0] > 1U)) return 0U;
            }
            break;

        case V851_TLV_STRUCT_LIGHT:
            if(field->tag == V851_TLV_TAG_LIGHT_ENABLED)
            {
                if((field->length != 1U) || (field->value[0] > 1U)) return 0U;
                *mapped_bit = V851_CONTROL_MAPPED_ENABLED;
                *mapped_value = field->value[0];
            }
            else if(field->tag == V851_TLV_TAG_LIGHT_BRIGHTNESS)
            {
                if((field->length != 1U) || (field->value[0] < 1U) ||
                   (field->value[0] > 4U)) return 0U;
                *mapped_bit = V851_CONTROL_MAPPED_SECONDARY;
                *mapped_value = field->value[0];
            }
            else if(field->tag == V851_TLV_TAG_LIGHT_RUNNING)
            {
                if((field->length != 1U) || (field->value[0] > 1U)) return 0U;
            }
            break;

        case V851_TLV_STRUCT_UVB:
            if(field->tag == V851_TLV_TAG_UVB_ENABLED)
            {
                if((field->length != 1U) || (field->value[0] > 1U)) return 0U;
                *mapped_bit = V851_CONTROL_MAPPED_ENABLED;
                *mapped_value = field->value[0];
            }
            else if(field->tag == V851_TLV_TAG_UVB_LEVEL)
            {
                if((field->length != 1U) || (field->value[0] < 1U) ||
                   (field->value[0] > 4U)) return 0U;
                *mapped_bit = V851_CONTROL_MAPPED_SECONDARY;
                *mapped_value = field->value[0];
            }
            else if(field->tag == V851_TLV_TAG_UVB_DAILY_HOURS)
            {
                if(field->length != 1U) return 0U;
                switch(field->value[0])
                {
                    case 2U: value = 1U; break;
                    case 4U: value = 2U; break;
                    case 6U: value = 3U; break;
                    case 8U: value = 4U; break;
                    default: return 0U;
                }
                *mapped_bit = V851_CONTROL_MAPPED_TERTIARY;
                *mapped_value = value;
            }
            else if(field->tag == V851_TLV_TAG_UVB_RUNNING)
            {
                if((field->length != 1U) || (field->value[0] > 1U)) return 0U;
            }
            break;

        case V851_TLV_STRUCT_ANION:
        case V851_TLV_STRUCT_PLASMA:
            if(field->tag == 0x01U)
            {
                if((field->length != 1U) || (field->value[0] > 1U)) return 0U;
                *mapped_bit = V851_CONTROL_MAPPED_ENABLED;
                *mapped_value = field->value[0];
            }
            else if(field->tag == 0x02U)
            {
                if((field->length != 1U) || (field->value[0] > 1U)) return 0U;
            }
            break;

        case V851_TLV_STRUCT_CLIMATE:
            if(field->tag == V851_TLV_TAG_CLIMATE_ENABLED)
            {
                if((field->length != 1U) || (field->value[0] > 1U)) return 0U;
                *mapped_bit = V851_CONTROL_MAPPED_ENABLED;
                *mapped_value = field->value[0];
            }
            else if(field->tag == V851_TLV_TAG_CLIMATE_TARGET)
            {
                if((V851TlvReadBinary64Uint16(field->value, field->length,
                                               &value) == 0U) ||
                   (value < 20U) || (value > 35U)) return 0U;
                *mapped_bit = V851_CONTROL_MAPPED_SECONDARY;
                *mapped_value = value;
            }
            else if(field->tag == V851_TLV_TAG_CLIMATE_CTRL_STATUS)
            {
                if((field->length != 1U) || (field->value[0] > 4U)) return 0U;
            }
            else if(field->tag == V851_TLV_TAG_CLIMATE_RUNNING)
            {
                if((field->length != 1U) || (field->value[0] > 1U)) return 0U;
            }
            break;

        case V851_TLV_STRUCT_HUMIDIFIER:
            if(field->tag == V851_TLV_TAG_HUMI_ENABLED)
            {
                if((field->length != 1U) || (field->value[0] > 1U)) return 0U;
                *mapped_bit = V851_CONTROL_MAPPED_ENABLED;
                *mapped_value = field->value[0];
            }
            else if(field->tag == V851_TLV_TAG_HUMI_INTERVAL_HOURS)
            {
                if(V851ControlInfoHoursToCode(V851_CONTROL_HOURS_HUMI_INTERVAL,
                                               field->value, field->length,
                                               mapped_value) == 0U) return 0U;
                *mapped_bit = V851_CONTROL_MAPPED_SECONDARY;
            }
            else if(field->tag == V851_TLV_TAG_HUMI_RUNNING_HOURS)
            {
                if(V851ControlInfoHoursToCode(V851_CONTROL_HOURS_HUMI_RUNNING,
                                               field->value, field->length,
                                               mapped_value) == 0U) return 0U;
                *mapped_bit = V851_CONTROL_MAPPED_TERTIARY;
            }
            else if(field->tag == V851_TLV_TAG_HUMI_RUNNING)
            {
                if((field->length != 1U) || (field->value[0] > 1U)) return 0U;
            }
            else if(field->tag == V851_TLV_TAG_HUMI_LIQUID_STATUS)
            {
                if((field->length != 1U) || (field->value[0] > 3U)) return 0U;
            }
            break;

        case V851_TLV_STRUCT_INLET_FAN:
            if(field->tag == V851_TLV_TAG_INLET_ENABLED)
            {
                if((field->length != 1U) || (field->value[0] > 1U)) return 0U;
                *mapped_bit = V851_CONTROL_MAPPED_ENABLED;
                *mapped_value = field->value[0];
            }
            else if(field->tag == V851_TLV_TAG_INLET_SPEED_LEVEL)
            {
                if((field->length != 1U) || (field->value[0] < 1U) ||
                   (field->value[0] > 4U)) return 0U;
                *mapped_bit = V851_CONTROL_MAPPED_SECONDARY;
                *mapped_value = field->value[0];
            }
            else if(field->tag == V851_TLV_TAG_INLET_RUNNING)
            {
                if((field->length != 1U) || (field->value[0] > 1U)) return 0U;
            }
            break;

        case V851_TLV_STRUCT_FILTER:
            if(field->tag == V851_TLV_TAG_FILTER_LIFE_PERCENT)
            {
                if((field->length != 1U) || (field->value[0] > 100U)) return 0U;
            }
            else if(field->tag == V851_TLV_TAG_FILTER_NEED_REPLACE)
            {
                if((field->length != 1U) || (field->value[0] > 1U)) return 0U;
            }
            break;

        default:
            return 0U;
    }
    return 1U;
}

uint8_t V851ControlInfoValidateSegment(uint8_t struct_type,
                                       const uint8_t *field_bytes,
                                       uint16_t length)
{
    V851TlvFieldCursor cursor;
    V851TlvField field;
    V851TlvIterResult result;
    uint8_t mapped_mask;
    uint8_t mapped_bit;
    uint16_t mapped_value;

    if((field_bytes == NULL) ||
       (V851ControlInfoIndex(struct_type) == 0xFFU))
    {
        return 0U;
    }
    mapped_mask = 0U;
    V851TlvFieldCursorInit(&cursor, field_bytes, length);
    for(;;)
    {
        result = V851TlvFieldNext(&cursor, &field);
        if(result == V851_TLV_ITER_END)
        {
            return 1U;
        }
        if((result != V851_TLV_ITER_FIELD) ||
           (V851ControlInfoMapWritableField(struct_type, &field,
                                             &mapped_bit,
                                             &mapped_value) == 0U))
        {
            return 0U;
        }
        if(mapped_bit != 0U)
        {
            if((mapped_mask & mapped_bit) != 0U)
            {
                return 0U;
            }
            mapped_mask |= mapped_bit;
        }
    }
}

static void V851ControlInfoApplySwitch(uint8_t struct_type,
                                       uint16_t enabled,
                                       const uint8_t *record)
{
    uint16_t setting;

    if(record == NULL)
    {
        return;
    }

    switch(struct_type)
    {
        case V851_TLV_STRUCT_EXHAUST:
            if(enabled != 0U) Exhaust_On();
            else Exhaust_Off(0U);
            break;

        case V851_TLV_STRUCT_LIGHT:
            setting = V851ControlInfoReadBe16(&record[2]);
            if(enabled != 0U) Light_On(setting);
            else Light_Off();
            break;

        case V851_TLV_STRUCT_UVB:
            setting = V851ControlInfoReadBe16(&record[2]);
            if(enabled != 0U) UVB_On(setting);
            else UVB_Off(0U);
            break;

        case V851_TLV_STRUCT_ANION:
            if(enabled != 0U) Dev_Switch_Ctrl(&G_Device_Ctrl.Anion.enable, ANION_CMDWORD, 1U);
            else Dev_Switch_Ctrl(&G_Device_Ctrl.Anion.enable, ANION_CMDWORD, 0U);
            break;

        case V851_TLV_STRUCT_PLASMA:
            if(enabled != 0U) Dev_Switch_Ctrl(&G_Device_Ctrl.Plasma.enable, PLASMA_CMDWORD, 1U);
            else Dev_Switch_Ctrl(&G_Device_Ctrl.Plasma.enable, PLASMA_CMDWORD, 0U);
            break;

        case V851_TLV_STRUCT_CLIMATE:
            setting = V851ControlInfoReadBe16(&record[2]);
            if(enabled != 0U) Heater_On((int16_t)setting);
            else Heater_Off(0U);
            break;

        case V851_TLV_STRUCT_HUMIDIFIER:
            if(enabled != 0U) Humidifier_On();
            else Humidifier_Off(0U);
            break;

        case V851_TLV_STRUCT_INLET_FAN:
            setting = V851ControlInfoReadBe16(&record[2]);
            if(enabled != 0U) InWind_On(setting);
            else InWind_Off();
            break;

        default:
            break;
    }
}

uint8_t V851ControlInfoApplySegment(uint8_t struct_type,
                                    const uint8_t *field_bytes,
                                    uint16_t length)
{
    V851TlvFieldCursor cursor;
    V851TlvField field;
    uint8_t record[V851_CONTROL_COMMAND_MAX_BYTES];
    uint8_t command_words;
    uint8_t changed;
    uint8_t mapped_mask;
    uint8_t mapped_bit;
    uint8_t index;
    uint16_t mapped_value;
    uint16_t enabled;
    uint16_t secondary;
    uint16_t tertiary;
    uint32_t address;

    index = V851ControlInfoIndex(struct_type);
    address = V851ControlInfoCommandAddress(struct_type);
    command_words = V851ControlInfoCommandWords(struct_type);
    if((index == 0xFFU) || (field_bytes == NULL) ||
       (address == 0UL) || (command_words == 0U))
    {
        return 0U;
    }

    read_dgus_vp(address, record, command_words);
    changed = 0U;
    mapped_mask = 0U;
    enabled = 0U;
    secondary = 0U;
    tertiary = 0U;
    V851TlvFieldCursorInit(&cursor, field_bytes, length);
    while(V851TlvFieldNext(&cursor, &field) == V851_TLV_ITER_FIELD)
    {
        if((V851ControlInfoMapWritableField(struct_type, &field,
                                             &mapped_bit,
                                             &mapped_value) != 0U) &&
           (mapped_bit != 0U))
        {
            if(mapped_bit == V851_CONTROL_MAPPED_ENABLED)
            {
                V851ControlInfoWriteBe16(&record[0], mapped_value);
                enabled = mapped_value;
                mapped_mask |= T5L_STC_MAPPED_FIELD_ENABLED;
            }
            else if(mapped_bit == V851_CONTROL_MAPPED_SECONDARY)
            {
                V851ControlInfoWriteBe16(&record[2], mapped_value);
                secondary = mapped_value;
                mapped_mask |= T5L_STC_MAPPED_FIELD_SECONDARY;
            }
            else
            {
                V851ControlInfoWriteBe16(&record[4], mapped_value);
                tertiary = mapped_value;
                mapped_mask |= T5L_STC_MAPPED_FIELD_TERTIARY;
            }
            changed = 1U;
        }
    }

    if(changed != 0U)
    {
        write_dgus_vp(address, record, command_words);
        (void)T5lStcSyncMappedControl((T5lStcMappedControl)index,
                                      mapped_mask, enabled, secondary,
                                      tertiary);
        if((mapped_mask & T5L_STC_MAPPED_FIELD_ENABLED) != 0U)
        {
            V851ControlInfoApplySwitch(struct_type, enabled, record);
        }
    }
    return changed;
}

static uint8_t V851ControlInfoBuildU8(uint8_t *field_buffer,
                                      uint16_t capacity,
                                      uint16_t *offset,
                                      uint8_t tag,
                                      uint16_t value,
                                      uint16_t minimum,
                                      uint16_t maximum)
{
    if((value < minimum) || (value > maximum) || (value > 0xFFU))
    {
        return 0U;
    }
    return V851TlvWriteU8(field_buffer, capacity, offset, tag,
                           (uint8_t)value);
}

/* 将带一位小数的有符号温度转换为IEEE-754 binary64，避免依赖C51的
 * 32位native float/double布局。 */
static uint8_t V851ControlInfoWriteTenths(uint8_t *buffer,
                                          uint16_t capacity,
                                          uint16_t *offset,
                                          uint8_t tag,
                                          int16_t tenths)
{
    uint8_t encoded[8];
    uint8_t bit_index;
    uint8_t mantissa_bit;
    uint8_t negative;
    uint16_t denominator;
    uint16_t magnitude;
    uint16_t remainder;
    uint16_t rounded_remainder;
    int16_t exponent;
    uint32_t high_word;
    uint32_t low_word;

    high_word = 0UL;
    low_word = 0UL;
    if(tenths != 0)
    {
        negative = (tenths < 0) ? 1U : 0U;
        magnitude = negative ? (uint16_t)(-(int32_t)tenths) :
                               (uint16_t)tenths;
        denominator = 10U;
        exponent = 0;
        if(magnitude >= denominator)
        {
            while(((uint32_t)denominator << 1U) <= magnitude)
            {
                denominator = (uint16_t)(denominator << 1U);
                ++exponent;
            }
            remainder = (uint16_t)(magnitude - denominator);
        }
        else
        {
            remainder = (uint16_t)(magnitude << 1U);
            exponent = -1;
            while(remainder < 10U)
            {
                remainder = (uint16_t)(remainder << 1U);
                --exponent;
            }
            remainder = (uint16_t)(remainder - 10U);
            denominator = 10U;
        }

        high_word = (uint32_t)(1023 + exponent) << 20;
        if(negative != 0U)
        {
            high_word |= 0x80000000UL;
        }
        for(bit_index = 0U; bit_index < 52U; ++bit_index)
        {
            remainder = (uint16_t)(remainder << 1U);
            if(remainder >= denominator)
            {
                remainder = (uint16_t)(remainder - denominator);
                mantissa_bit = (uint8_t)(51U - bit_index);
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

        /* IEEE-754 round-to-nearest, ties-to-even. */
        rounded_remainder = (uint16_t)(remainder << 1U);
        if((rounded_remainder > denominator) ||
           ((rounded_remainder == denominator) &&
            ((low_word & 1UL) != 0UL)))
        {
            if(low_word == 0xFFFFFFFFUL)
            {
                low_word = 0UL;
                ++high_word;
            }
            else
            {
                ++low_word;
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

uint16_t V851ControlInfoBuildFields(uint8_t struct_type,
                                    uint8_t *field_buffer,
                                    uint16_t capacity)
{
    uint8_t record[V851_CONTROL_REPORT_MAX_BYTES];
    uint8_t words;
    uint16_t offset;
    uint16_t value;
    uint32_t address;

    if(field_buffer == NULL)
    {
        return 0U;
    }
    if(struct_type == V851_TLV_STRUCT_ENVIRONMENT)
    {
        /* 0x3010占两个word保存温度float，0x3012保存湿度u16。 */
        read_dgus_vp(V851_CONTROL_REPORT_ENVIRONMENT_ADDR, record, 3U);
        value = V851ControlInfoReadBe16(&record[4]);
        offset = 0U;
        if((V851ControlInfoWriteTenths(
                field_buffer, capacity, &offset, V851_TLV_TAG_TEMPERATURE,
                G_Device_Ctrl.environment.temperaturex10) == 0U) ||
           (V851TlvWriteBinary64Uint16(
                field_buffer, capacity, &offset, V851_TLV_TAG_HUMIDITY,
                value) == 0U))
        {
            return 0U;
        }
        return offset;
    }

    address = V851ControlInfoReportAddress(struct_type);
    words = V851ControlInfoReportWords(struct_type);
    if((address == 0UL) || (words == 0U))
    {
        return 0U;
    }
    read_dgus_vp(address, record, words);
    offset = 0U;

#define V851_BUILD_U8(word_index, tag_value, minimum_value, maximum_value) \
    value = V851ControlInfoReadBe16(&record[(word_index) * 2U]); \
    if(V851ControlInfoBuildU8(field_buffer, capacity, &offset, (tag_value), \
                              value, (minimum_value), (maximum_value)) == 0U) \
    { return 0U; }

    switch(struct_type)
    {
        case V851_TLV_STRUCT_EXHAUST:
            V851_BUILD_U8(0U, V851_TLV_TAG_EXH_ENABLED, 0U, 1U);
            V851_BUILD_U8(1U, V851_TLV_TAG_EXH_SPEED_LEVEL, 1U, 6U);
            value = V851ControlInfoReadBe16(&record[4]);
            if(V851ControlInfoWriteHours(field_buffer, capacity, &offset,
                                          V851_TLV_TAG_EXH_INTERVAL_HOURS,
                                          V851_CONTROL_HOURS_EXHAUST,
                                          value) == 0U) return 0U;
            V851_BUILD_U8(3U, V851_TLV_TAG_EXH_RUNNING, 0U, 1U);
            break;

        case V851_TLV_STRUCT_LIGHT:
            V851_BUILD_U8(0U, V851_TLV_TAG_LIGHT_ENABLED, 0U, 1U);
            V851_BUILD_U8(1U, V851_TLV_TAG_LIGHT_BRIGHTNESS, 1U, 4U);
            V851_BUILD_U8(2U, V851_TLV_TAG_LIGHT_RUNNING, 0U, 1U);
            break;

        case V851_TLV_STRUCT_UVB:
            V851_BUILD_U8(0U, V851_TLV_TAG_UVB_ENABLED, 0U, 1U);
            V851_BUILD_U8(1U, V851_TLV_TAG_UVB_LEVEL, 1U, 4U);
            value = V851ControlInfoReadBe16(&record[4]);
            if((value < 1U) || (value > 4U) ||
               (V851TlvWriteU8(field_buffer, capacity, &offset,
                                V851_TLV_TAG_UVB_DAILY_HOURS,
                                (uint8_t)(value * 2U)) == 0U)) return 0U;
            V851_BUILD_U8(3U, V851_TLV_TAG_UVB_RUNNING, 0U, 1U);
            break;

        case V851_TLV_STRUCT_ANION:
            V851_BUILD_U8(0U, V851_TLV_TAG_ANION_ENABLED, 0U, 1U);
            V851_BUILD_U8(1U, V851_TLV_TAG_ANION_RUNNING, 0U, 1U);
            break;

        case V851_TLV_STRUCT_PLASMA:
            V851_BUILD_U8(0U, V851_TLV_TAG_PLASMA_ENABLED, 0U, 1U);
            V851_BUILD_U8(1U, V851_TLV_TAG_PLASMA_RUNNING, 0U, 1U);
            break;

        case V851_TLV_STRUCT_CLIMATE:
            V851_BUILD_U8(0U, V851_TLV_TAG_CLIMATE_ENABLED, 0U, 1U);
            value = V851ControlInfoReadBe16(&record[2]);
            if((value < 20U) || (value > 35U) ||
               (V851TlvWriteBinary64Uint16(field_buffer, capacity, &offset,
                                            V851_TLV_TAG_CLIMATE_TARGET,
                                            value) == 0U)) return 0U;
            V851_BUILD_U8(2U, V851_TLV_TAG_CLIMATE_CTRL_STATUS, 0U, 4U);
            V851_BUILD_U8(3U, V851_TLV_TAG_CLIMATE_RUNNING, 0U, 1U);
            break;

        case V851_TLV_STRUCT_HUMIDIFIER:
            V851_BUILD_U8(0U, V851_TLV_TAG_HUMI_ENABLED, 0U, 1U);
            value = V851ControlInfoReadBe16(&record[2]);
            if(V851ControlInfoWriteHours(field_buffer, capacity, &offset,
                                          V851_TLV_TAG_HUMI_INTERVAL_HOURS,
                                          V851_CONTROL_HOURS_HUMI_INTERVAL,
                                          value) == 0U) return 0U;
            value = V851ControlInfoReadBe16(&record[4]);
            if(V851ControlInfoWriteHours(field_buffer, capacity, &offset,
                                          V851_TLV_TAG_HUMI_RUNNING_HOURS,
                                          V851_CONTROL_HOURS_HUMI_RUNNING,
                                          value) == 0U) return 0U;
            V851_BUILD_U8(3U, V851_TLV_TAG_HUMI_RUNNING, 0U, 1U);
            V851_BUILD_U8(4U, V851_TLV_TAG_HUMI_LIQUID_STATUS, 0U, 3U);
            break;

        case V851_TLV_STRUCT_INLET_FAN:
            V851_BUILD_U8(0U, V851_TLV_TAG_INLET_ENABLED, 0U, 1U);
            V851_BUILD_U8(1U, V851_TLV_TAG_INLET_SPEED_LEVEL, 1U, 4U);
            V851_BUILD_U8(2U, V851_TLV_TAG_INLET_RUNNING, 0U, 1U);
            break;

        case V851_TLV_STRUCT_FILTER:
            V851_BUILD_U8(0U, V851_TLV_TAG_FILTER_LIFE_PERCENT, 0U, 100U);
            V851_BUILD_U8(1U, V851_TLV_TAG_FILTER_NEED_REPLACE, 0U, 1U);
            break;

        default:
            return 0U;
    }
#undef V851_BUILD_U8
    return offset;
}

uint16_t V851ControlInfoScanChanged(void)
{
    uint8_t record[V851_CONTROL_REPORT_MAX_BYTES];
    uint8_t index;
    uint8_t bytes;
    uint8_t words;
    uint8_t struct_type;
    uint16_t valid_bit;
    uint16_t changed_mask;

    changed_mask = 0U;
    for(index = 0U; index < V851_CONTROL_COUNT; ++index)
    {
        struct_type = V851ControlInfoReportStructType(index);
        words = V851ControlInfoReportWords(struct_type);
        bytes = (uint8_t)(words * 2U);
        read_dgus_vp(V851ControlInfoReportAddress(struct_type), record, words);
        valid_bit = (uint16_t)1U << index;
        if((v851_control_report_shadow_valid_mask & valid_bit) == 0U)
        {
            memcpy(v851_control_report_shadow[index], record, bytes);
            v851_control_report_shadow_valid_mask |= valid_bit;
        }
        else if(memcmp(record, v851_control_report_shadow[index], bytes) != 0)
        {
            changed_mask |= valid_bit;
            memcpy(v851_control_report_shadow[index], record, bytes);
        }
    }
    return changed_mask;
}

#endif /* v851PROTOCOL_ENABLED */

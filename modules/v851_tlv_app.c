#include "v851_protocol.h"

#if v851PROTOCOL_ENABLED

#include <string.h>

#define V851_BOOTSTRAP_QR_VP_ADDR                0x5500U
#define V851_BOOTSTRAP_QR_VP_WORDS               64U
#define V851_BOOTSTRAP_QR_BYTES                  \
    (V851_BOOTSTRAP_QR_VP_WORDS * 2U)

static V851BootstrapResult xdata v851_bootstrap_result;
static uint8_t v851_bootstrap_result_valid;
static uint8_t xdata
    v851_bootstrap_qr_buffer[V851_BOOTSTRAP_QR_BYTES];

static void V851BootstrapWriteQr(void)
{
    uint16_t length;

    memset(v851_bootstrap_qr_buffer, 0,
           sizeof(v851_bootstrap_qr_buffer));
    length = 0U;
    while((length < sizeof(v851_bootstrap_qr_buffer)) &&
          (v851_bootstrap_result.qr_url[length] != '\0'))
    {
        ++length;
    }
    if((length != 0U) &&
       (length < sizeof(v851_bootstrap_qr_buffer)))
    {
        memcpy(v851_bootstrap_qr_buffer,
               v851_bootstrap_result.qr_url, length);
    }
    write_dgus_vp(V851_BOOTSTRAP_QR_VP_ADDR,
                  v851_bootstrap_qr_buffer,
                  V851_BOOTSTRAP_QR_VP_WORDS);
}

static void V851BootstrapCopyString(char *destination,
                                    uint16_t capacity,
                                    const V851TlvField *field)
{
    uint16_t length;

    length = field->length;
    if(length >= capacity)
    {
        length = (uint16_t)(capacity - 1U);
    }
    if(length != 0U)
    {
        memcpy(destination, field->value, length);
    }
    destination[length] = '\0';
}

static void V851BootstrapApplySegment(const V851TlvSegment *segment)
{
    V851TlvFieldCursor cursor;
    V851TlvField field;
    V851TlvIterResult result;

    /* Validate again locally so this hook remains atomic if called directly. */
    V851TlvFieldCursorInit(&cursor, segment->payload, segment->length);
    for(;;)
    {
        result = V851TlvFieldNext(&cursor, &field);
        if(result == V851_TLV_ITER_END)
        {
            break;
        }
        if(result != V851_TLV_ITER_FIELD)
        {
            return;
        }
    }

    memset(&v851_bootstrap_result, 0, sizeof(v851_bootstrap_result));
    v851_bootstrap_result.struct_type = V851_TLV_STRUCT_BOOTSTRAP_RESULT;
    V851TlvFieldCursorInit(&cursor, segment->payload, segment->length);
    for(;;)
    {
        result = V851TlvFieldNext(&cursor, &field);
        if(result == V851_TLV_ITER_END)
        {
            V851BootstrapWriteQr();
            v851_bootstrap_result_valid = 1U;
            return;
        }
        if(result != V851_TLV_ITER_FIELD)
        {
            return;
        }
        switch(field.tag)
        {
            case V851_TLV_TAG_BOOT_DEVICE_SN:
                V851BootstrapCopyString(v851_bootstrap_result.device_sn,
                                        V851_BOOTSTRAP_DEVICE_SN_SIZE, &field);
                break;
            case V851_TLV_TAG_BOOT_BLE_ID:
                V851BootstrapCopyString(v851_bootstrap_result.ble_id,
                                        V851_BOOTSTRAP_BLE_ID_SIZE, &field);
                break;
            case V851_TLV_TAG_BOOT_API_ENDPOINT:
                V851BootstrapCopyString(v851_bootstrap_result.api_endpoint,
                                        V851_BOOTSTRAP_API_ENDPOINT_SIZE, &field);
                break;
            case V851_TLV_TAG_BOOT_BIND_STATUS:
                V851BootstrapCopyString(v851_bootstrap_result.bind_status,
                                        V851_BOOTSTRAP_BIND_STATUS_SIZE, &field);
                break;
            case V851_TLV_TAG_BOOT_QR_URL:
                V851BootstrapCopyString(v851_bootstrap_result.qr_url,
                                        V851_BOOTSTRAP_QR_URL_SIZE, &field);
                break;
            default:
                break;
        }
    }
}

const V851BootstrapResult *V851BootstrapResultGet(void)
{
    if(v851_bootstrap_result_valid == 0U)
    {
        return NULL;
    }
    return &v851_bootstrap_result;
}

#define V851_FIRMWARE_CURRENT_VP                 0x5290U
#define V851_FIRMWARE_LATEST_VP                  0x52A0U
#define V851_FIRMWARE_VP_WORDS                   16U

static void V851FirmwareApplySegment(const V851TlvSegment *segment)
{
    V851TlvFieldCursor cursor;
    V851TlvField field;
    V851TlvIterResult result;
    uint16_t address;
    uint16_t length;
    uint8_t text[V851_FIRMWARE_VP_WORDS * 2U];

    /* Validate before writing, including when the application hook is called directly. */
    V851TlvFieldCursorInit(&cursor, segment->payload, segment->length);
    do
    {
        result = V851TlvFieldNext(&cursor, &field);
    } while(result == V851_TLV_ITER_FIELD);
    if(result != V851_TLV_ITER_END)
    {
        return;
    }

    V851TlvFieldCursorInit(&cursor, segment->payload, segment->length);
    while(V851TlvFieldNext(&cursor, &field) == V851_TLV_ITER_FIELD)
    {
        if(field.tag == V851_TLV_TAG_FW_VERSION)
        {
            address = V851_FIRMWARE_CURRENT_VP;
        }
        else if(field.tag == V851_TLV_TAG_LATEST_FW_VERSION)
        {
            address = V851_FIRMWARE_LATEST_VP;
        }
        else
        {
            continue;
        }
        memset(text, 0, sizeof(text));
        length = field.length;
        if(length >= sizeof(text))
        {
            length = sizeof(text) - 1U;
        }
        if(length != 0U)
        {
            memcpy(text, field.value, length);
        }
        write_dgus_vp(address, text, V851_FIRMWARE_VP_WORDS);
    }
}

/* Single project hook for Bootstrap and firmware display. */
void V851TlvApplicationSegment(uint8_t command,
                               const V851TlvSegment *segment)
{
    if((command == V851_TLV_CMD_BOOTSTRAP_RESULT) &&
       (segment != NULL) &&
       (segment->struct_type == V851_TLV_STRUCT_BOOTSTRAP_RESULT))
    {
        V851BootstrapApplySegment(segment);
    }
    else if((command == V851_TLV_CMD_PROPERTY) &&
            (segment != NULL) &&
            (segment->struct_type == V851_TLV_STRUCT_FIRMWARE))
    {
        V851FirmwareApplySegment(segment);
    }
}

#endif /* v851PROTOCOL_ENABLED */

#include "v851_wifi.h"

#if v851PROTOCOL_ENABLED

#include "v851_protocol.h"

#include <string.h>

#pragma optimize(8, size)

#define V851_WIFI_SSID_MAX                       32U
#define V851_WIFI_PAGE_SIZE                      5U

#define V851_WIFI_KEY_LIST_1                     0xAF01U
#define V851_WIFI_KEY_LIST_5                     0xAF05U
#define V851_WIFI_KEY_CONNECT                    0xAA07U
#define V851_WIFI_KEY_SCAN                       0xAA09U
#define V851_WIFI_KEY_PREVIOUS                   0xAA12U
#define V851_WIFI_KEY_NEXT                       0xAA13U

typedef struct
{
    uint8_t scan_flag;
    uint8_t scan_page;
    uint8_t detail_flag;
    uint8_t detail_page;
    uint8_t transit_scan_flag;
    uint8_t transit_scan_page;
    uint8_t transit_detail_flag;
    uint8_t transit_detail_page;
    uint8_t comic_flag;
    uint8_t comic_page;
    uint8_t store_flag;
    uint8_t store_page;
    uint8_t start_flag;
    uint8_t start_page;
    uint16_t wifi_start_addr;
} V851WifiPageConfig;

static V851WifiPageConfig v851_wifi_page;
static uint8_t v851_wifi_page_offset;
static uint8_t xdata v851_wifi_clear_buffer
    [V851_WIFI_SSID_MAX * V851_WIFI_PAGE_SIZE];

static void V851WifiSwitchIfEnabled(uint8_t flag, uint8_t page)
{
    if(flag == 0x5AU)
    {
        SwitchPageById(page);
    }
}

static uint8_t V851WifiTextLength(const uint8_t *text)
{
    uint8_t length;

    length = 0U;
    while((length < V851_WIFI_SSID_MAX) &&
          (text[length] != 0x00U) && (text[length] != 0xFFU))
    {
        ++length;
    }
    return length;
}

static void V851WifiClearList(void)
{
    memset(v851_wifi_clear_buffer, 0, sizeof(v851_wifi_clear_buffer));
    write_dgus_vp(v851_wifi_page.wifi_start_addr,
                  v851_wifi_clear_buffer,
                  sizeof(v851_wifi_clear_buffer) / 2U);
}

static void V851WifiRequestScan(void)
{
    uint8_t frame[7];

    frame[0] = 0xAAU;
    frame[1] = 0x55U;
    frame[2] = 0x00U;
    frame[3] = 0x03U;
    frame[4] = V851_WIFI_CMD_SCAN;
    frame[5] = (uint8_t)(v851_wifi_page_offset * V851_WIFI_PAGE_SIZE);
    frame[6] = V851_WIFI_PAGE_SIZE;
    (void)V851ProtocolSendWifiFrame(frame, sizeof(frame));
}

static void V851WifiRequestConnect(void)
{
    uint8_t frame[74];
    uint8_t text[V851_WIFI_SSID_MAX];
    uint8_t ssid_length;
    uint8_t password_length;
    uint16_t offset;
    uint16_t declared_length;

    memset(frame, 0, sizeof(frame));
    frame[0] = 0xAAU;
    frame[1] = 0x55U;
    frame[4] = V851_WIFI_CMD_CONNECT;
    frame[5] = 0x01U;
    read_dgus_vp(V851_WIFI_SSID_ADDR, text, V851_WIFI_SSID_MAX / 2U);
    ssid_length = V851WifiTextLength(text);
    frame[6] = 0x00U;
    frame[7] = ssid_length;
    offset = 8U;
    if(ssid_length != 0U)
    {
        memcpy(&frame[offset], text, ssid_length);
        offset = (uint16_t)(offset + ssid_length);
    }

    read_dgus_vp(V851_WIFI_PASSWORD_ADDR, text, V851_WIFI_SSID_MAX / 2U);
    password_length = V851WifiTextLength(text);
    frame[offset++] = 0x00U;
    frame[offset++] = password_length;
    if(password_length != 0U)
    {
        memcpy(&frame[offset], text, password_length);
        offset = (uint16_t)(offset + password_length);
    }
    declared_length = (uint16_t)(offset - 4U);
    frame[2] = (uint8_t)(declared_length >> 8);
    frame[3] = (uint8_t)declared_length;
    (void)V851ProtocolSendWifiFrame(frame, offset);
}

static void V851WifiSelect(uint8_t list_index)
{
    uint8_t ssid[V851_WIFI_SSID_MAX];

    read_dgus_vp(v851_wifi_page.wifi_start_addr +
                 ((uint16_t)list_index * 0x10U),
                 ssid, V851_WIFI_SSID_MAX / 2U);
    if((ssid[0] == 0x00U) || (ssid[0] == 0xFFU))
    {
        return;
    }
    write_dgus_vp(V851_WIFI_SSID_ADDR, ssid, V851_WIFI_SSID_MAX / 2U);
    V851WifiClearList();
    V851WifiSwitchIfEnabled(v851_wifi_page.detail_flag,
                            v851_wifi_page.detail_page);
}

static void V851WifiHandleKey(uint16_t key)
{
    if((key >= V851_WIFI_KEY_LIST_1) && (key <= V851_WIFI_KEY_LIST_5))
    {
        V851WifiSelect((uint8_t)(key - V851_WIFI_KEY_LIST_1));
    }
    else if(key == V851_WIFI_KEY_CONNECT)
    {
        V851WifiRequestConnect();
        V851WifiSwitchIfEnabled(v851_wifi_page.transit_detail_flag,
                                v851_wifi_page.transit_detail_page);
    }
    else if(key == V851_WIFI_KEY_SCAN)
    {
        v851_wifi_page_offset = 0U;
        V851WifiSwitchIfEnabled(v851_wifi_page.transit_scan_flag,
                                v851_wifi_page.transit_scan_page);
        V851WifiRequestScan();
    }
    else if(key == V851_WIFI_KEY_PREVIOUS)
    {
        if(v851_wifi_page_offset != 0U)
        {
            --v851_wifi_page_offset;
        }
        V851WifiSwitchIfEnabled(v851_wifi_page.transit_scan_flag,
                                v851_wifi_page.transit_scan_page);
        V851WifiRequestScan();
    }
    else if(key == V851_WIFI_KEY_NEXT)
    {
        if(v851_wifi_page_offset < 51U)
        {
            ++v851_wifi_page_offset;
        }
        V851WifiSwitchIfEnabled(v851_wifi_page.transit_scan_flag,
                                v851_wifi_page.transit_scan_page);
        V851WifiRequestScan();
    }
}

void V851WifiInit(void)
{
    uint8_t page_bytes[14];
//
//    FlashToDgus(flashMAIN_BLOCK_ORDER,V851_WIFI_PAGE_CONFIG_ADDR,V851_WIFI_PAGE_CONFIG_ADDR,8);
    memset(&v851_wifi_page, 0, sizeof(v851_wifi_page));

//    read_dgus_vp(V851_WIFI_PAGE_CONFIG_ADDR, page_bytes, 7U);
//    memcpy(&v851_wifi_page, page_bytes, sizeof(page_bytes));
//    read_dgus_vp(V851_WIFI_LIST_CONFIG_ADDR,
//                 (uint8_t *)&v851_wifi_page.wifi_start_addr, 1U);
	v851_wifi_page.scan_flag=0x5A;
    v851_wifi_page.scan_page=41;
    v851_wifi_page.detail_flag=0x5A;
    v851_wifi_page.detail_page=41;
	
    v851_wifi_page.wifi_start_addr=0x1900;
    v851_wifi_page_offset = 0U;
}

void V851WifiTask(void)
{
    uint16_t key;
    const uint16_t zero = 0U;

    key = 0U;
    read_dgus_vp(V851_WIFI_KEY_ADDR, (uint8_t *)&key, 1U);
    if(key != 0U)
    {
        V851WifiHandleKey(key);
        write_dgus_vp(V851_WIFI_KEY_ADDR, (uint8_t *)&zero, 1U);
    }
}

static void V851WifiStoreSsid(uint8_t list_index,
                              const uint8_t *ssid,
                              uint16_t length)
{
    uint8_t text[V851_WIFI_SSID_MAX];

    memset(text, 0, sizeof(text));
    if(length > V851_WIFI_SSID_MAX)
    {
        length = V851_WIFI_SSID_MAX;
    }
    if(length != 0U)
    {
        memcpy(text, ssid, length);
    }
    write_dgus_vp(v851_wifi_page.wifi_start_addr +
                  ((uint16_t)list_index * 0x10U),
                  text, V851_WIFI_SSID_MAX / 2U);
}

static void V851WifiHandleScanResult(const uint8_t *frame, uint16_t len)
{
    uint16_t start;
    uint16_t offset;
    uint16_t ssid_length;
    uint8_t list_index;

    V851WifiClearList();
    if((len < 6U) || (frame[5] != 0x01U))
    {
        // V851WifiSwitchIfEnabled(v851_wifi_page.scan_flag,
        //                         v851_wifi_page.scan_page);
        return;
    }
    if((len > 6U) && (frame[6] == '\n'))
    {
        if(v851_wifi_page_offset != 0U)
        {
            --v851_wifi_page_offset;
        }
        // V851WifiSwitchIfEnabled(v851_wifi_page.scan_flag,
        //                         v851_wifi_page.scan_page);
        return;
    }

    list_index = 0U;
    start = 6U;
    offset = start;
    while((offset <= len) && (list_index < V851_WIFI_PAGE_SIZE))
    {
        if((offset == len) || (frame[offset] == '\n'))
        {
            ssid_length = (uint16_t)(offset - start);
            if((ssid_length != 0U) &&
               (frame[start + ssid_length - 1U] == '\r'))
            {
                --ssid_length;
            }
            if(ssid_length != 0U)
            {
                V851WifiStoreSsid(list_index, &frame[start], ssid_length);
                ++list_index;
            }
            start = (uint16_t)(offset + 1U);
        }
        ++offset;
    }
    // V851WifiSwitchIfEnabled(v851_wifi_page.scan_flag,
    //                         v851_wifi_page.scan_page);
}

void V851WifiReceiveFrame(const uint8_t *frame, uint16_t len)
{
    uint16_t declared_length;
    uint16_t status_word;

    if((frame == NULL) || (len < 5U) ||
       (frame[0] != 0xAAU) || (frame[1] != 0x55U))
    {
        return;
    }
    declared_length = (uint16_t)(((uint16_t)frame[2] << 8) | frame[3]);
    if((declared_length < 1U) ||
       ((uint16_t)(declared_length + 4U) != len))
    {
        return;
    }
    switch(frame[4])
    {
        case V851_WIFI_CMD_SCAN:
            V851WifiHandleScanResult(frame, len);
            break;
        case V851_WIFI_CMD_CONNECT:
            break;
        case V851_WIFI_CMD_STATUS:
            if(len > 8U)
            {
                status_word = (frame[8] == 2U) ? 2U : 1U;
                write_dgus_vp(V851_WIFI_STATUS_ADDR,
                              (uint8_t *)&status_word, 1U);
            }
            break;
        default:
            break;
    }
}

#endif /* v851PROTOCOL_ENABLED */

#ifndef V851_WIFI_H
#define V851_WIFI_H

#include "sys.h"

#if v851PROTOCOL_ENABLED

#define V851_WIFI_TASK_INTERVAL                  100U

#define V851_WIFI_KEY_ADDR                       0x0600UL
#define V851_WIFI_SSID_ADDR                      0x04B0UL
#define V851_WIFI_PASSWORD_ADDR                  0x04C0UL
#define V851_WIFI_PAGE_CONFIG_ADDR               0x05B8UL
#define V851_WIFI_LIST_CONFIG_ADDR               0x05BEUL
#define V851_WIFI_STATUS_ADDR                    0x06D8UL

#define V851_WIFI_CMD_SCAN                       0xC0U
#define V851_WIFI_CMD_CONNECT                    0xC1U
#define V851_WIFI_CMD_STATUS                     0xC5U

void V851WifiInit(void);
void V851WifiClearCredentials(void);
void V851WifiTask(void);
void V851WifiReceiveFrame(const uint8_t *frame, uint16_t len);

#endif /* v851PROTOCOL_ENABLED */

#endif /* V851_WIFI_H */

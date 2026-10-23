#ifndef V851_PROTOCOL_H
#define V851_PROTOCOL_H

#include "sys.h"

#if v851PROTOCOL_ENABLED

#define V851_PROTOCOL_TASK_INTERVAL              1U

#define V851_FRAME_MAGIC_HIGH                    0xAAU
#define V851_FRAME_MAGIC_LOW                     0x55U
#define V851_TLV_CMD_PROPERTY                    0x35U
#define V851_TLV_CMD_FACTORY                     0x36U
#define V851_TLV_CMD_SNAPSHOT                    0x37U
#define V851_TLV_CMD_BOOTSTRAP_RESULT            0x37U
/* Compatibility form observed with 0x6C in the command byte. */
#define V851_TLV_CMD_BOOTSTRAP_RESULT_COMPAT     0x6CU
#define V851_TLV_CMD_EVENT_ALARM                 0x38U
#define V851_TLV_CMD_FACTORY_RESET               0x39U

#define V851_TLV_FRAME_MAX                       2048U
#define V851_TLV_FRAME_FIXED_SIZE                5U
#define V851_TLV_RX_LENGTH_BASE_SIZE             4U
#define V851_TLV_SEGMENT_HEADER_SIZE             3U
#define V851_TLV_FIELD_HEADER_SIZE               3U

/* Documented structure identifiers. */
#define V851_TLV_STRUCT_DEVICE                   0x01U
#define V851_TLV_STRUCT_NETWORK                  0x02U
#define V851_TLV_STRUCT_POWER                    0x03U
#define V851_TLV_STRUCT_DOOR                     0x04U
#define V851_TLV_STRUCT_ENVIRONMENT              0x05U
#define V851_TLV_STRUCT_ACTUATORS                0x06U
#define V851_TLV_STRUCT_CAMERA                   0x07U
#define V851_TLV_STRUCT_SETTINGS                 0x08U
#define V851_TLV_STRUCT_FIRMWARE                 0x09U
#define V851_TLV_STRUCT_STORAGE                  0x0AU
#define V851_TLV_STRUCT_EXHAUST                  0x61U
#define V851_TLV_STRUCT_LIGHT                    0x62U
#define V851_TLV_STRUCT_UVB                      0x63U
#define V851_TLV_STRUCT_ANION                    0x64U
#define V851_TLV_STRUCT_PLASMA                   0x65U
#define V851_TLV_STRUCT_CLIMATE                  0x66U
#define V851_TLV_STRUCT_HUMIDIFIER               0x67U
#define V851_TLV_STRUCT_INLET_FAN                0x68U
#define V851_TLV_STRUCT_FILTER                   0x69U
#define V851_TLV_STRUCT_FACTORY_DEVICE           0x6AU
#define V851_TLV_STRUCT_FACTORY_REJECTION        0x6BU
#define V851_TLV_STRUCT_BOOTSTRAP_RESULT         0x6CU
#define V851_TLV_STRUCT_EVENT_ALARM              0x6DU
#define V851_TLV_STRUCT_FACTORY_RESET            0x6EU
#define V851_TLV_TAG_RESET_ACTION                0x01U
#define V851_TLV_RESET_WIFI_AND_UNBIND           0x01U

/* PowerState. */
#define V851_TLV_TAG_POWER_MODE                  0x01U
#define V851_TLV_TAG_BATTERY_PERCENT             0x02U
#define V851_TLV_TAG_CHARGING                    0x03U
/* DoorState. */
#define V851_TLV_TAG_DOOR_STATUS                 0x01U
#define V851_TLV_TAG_LOCK_STATUS                 0x02U
#define V851_TLV_TAG_LAST_OPEN_TIME              0x03U
/* EnvironmentState. */
#define V851_TLV_TAG_TEMPERATURE                 0x01U
#define V851_TLV_TAG_HUMIDITY                    0x02U
#define V851_TLV_TAG_PM25                        0x03U
#define V851_TLV_TAG_CO2                         0x04U
/* ActuatorExhaust. */
#define V851_TLV_TAG_EXH_ENABLED                 0x01U
#define V851_TLV_TAG_EXH_SPEED_LEVEL             0x02U
#define V851_TLV_TAG_EXH_INTERVAL_HOURS          0x03U
#define V851_TLV_TAG_EXH_RUNNING                 0x04U
/* ActuatorLight. */
#define V851_TLV_TAG_LIGHT_ENABLED               0x01U
#define V851_TLV_TAG_LIGHT_BRIGHTNESS            0x02U
#define V851_TLV_TAG_LIGHT_RUNNING               0x03U
/* ActuatorUvb. */
#define V851_TLV_TAG_UVB_ENABLED                 0x01U
#define V851_TLV_TAG_UVB_LEVEL                   0x02U
#define V851_TLV_TAG_UVB_DAILY_HOURS             0x03U
#define V851_TLV_TAG_UVB_RUNNING                 0x04U
/* ActuatorAnion. */
#define V851_TLV_TAG_ANION_ENABLED               0x01U
#define V851_TLV_TAG_ANION_RUNNING               0x02U
/* ActuatorPlasma. */
#define V851_TLV_TAG_PLASMA_ENABLED              0x01U
#define V851_TLV_TAG_PLASMA_RUNNING              0x02U
/* ActuatorClimate. */
#define V851_TLV_TAG_CLIMATE_ENABLED             0x01U
#define V851_TLV_TAG_CLIMATE_TARGET              0x02U
#define V851_TLV_TAG_CLIMATE_CTRL_STATUS         0x03U
#define V851_TLV_TAG_CLIMATE_RUNNING             0x04U
/* ActuatorHumidifier. */
#define V851_TLV_TAG_HUMI_ENABLED                0x01U
#define V851_TLV_TAG_HUMI_INTERVAL_HOURS         0x02U
#define V851_TLV_TAG_HUMI_RUNNING_HOURS          0x03U
#define V851_TLV_TAG_HUMI_RUNNING                0x04U
#define V851_TLV_TAG_HUMI_LIQUID_STATUS          0x05U
/* ActuatorInletFan. */
#define V851_TLV_TAG_INLET_ENABLED               0x01U
#define V851_TLV_TAG_INLET_SPEED_LEVEL           0x02U
#define V851_TLV_TAG_INLET_RUNNING               0x03U
/* ActuatorFilter. */
#define V851_TLV_TAG_FILTER_LIFE_PERCENT         0x01U
#define V851_TLV_TAG_FILTER_NEED_REPLACE         0x02U
/* CameraState. */
#define V851_TLV_TAG_CAMERA_STATUS               0x01U
#define V851_TLV_TAG_STREAM_STATUS               0x02U
#define V851_TLV_TAG_PROVIDER                    0x03U
#define V851_TLV_TAG_VIDEO_CODEC                 0x04U
#define V851_TLV_TAG_RESOLUTION                  0x05U
#define V851_TLV_TAG_FPS                         0x06U
#define V851_TLV_TAG_AUDIO_ENABLED               0x07U
#define V851_TLV_TAG_PRIVACY_MODE                0x08U
#define V851_TLV_TAG_LAST_SESSION_ID             0x09U
#define V851_TLV_TAG_LAST_ERROR_CODE             0x0AU
/* DisplaySettings and SettingsState. */
#define V851_TLV_TAG_DISP_BRIGHTNESS             0x01U
#define V851_TLV_TAG_DISP_SCREEN_TIMEOUT         0x02U
#define V851_TLV_TAG_DISP_SCREEN_MODE            0x03U
#define V851_TLV_TAG_TEMP_UNIT                   0x01U
#define V851_TLV_TAG_DISPLAY                     0x02U
#define V851_TLV_TAG_VOLUME_PERCENT              0x03U
#define V851_TLV_TAG_LANGUAGE                    0x04U
#define V851_TLV_TAG_LOCAL_PASSWORD              0x05U
/* FirmwareState and StorageState. */
#define V851_TLV_TAG_FW_VERSION                  0x01U
#define V851_TLV_TAG_HW_VERSION                  0x02U
#define V851_TLV_TAG_CONFIG_VERSION              0x03U
#define V851_TLV_TAG_LATEST_FW_VERSION           0x04U
#define V851_TLV_TAG_STORAGE_TOTAL               0x01U
#define V851_TLV_TAG_STORAGE_FREE                0x02U
/* Factory data. */
#define V851_TLV_TAG_FACTORY_SALES_COUNTRY       0x01U
#define V851_TLV_TAG_FACTORY_PRODUCT_KEY         0x03U
#define V851_TLV_TAG_FACTORY_MODEL               0x04U
#define V851_TLV_TAG_FACTORY_HW_VERSION          0x05U
#define V851_TLV_TAG_FACTORY_FW_VERSION          0x06U
#define V851_TLV_TAG_FACTORY_REJECT_MAC          0x01U
#define V851_TLV_TAG_FACTORY_REJECT_REASON       0x02U
/* BootstrapResult. */
#define V851_TLV_TAG_BOOT_DEVICE_SN              0x01U
#define V851_TLV_TAG_BOOT_BLE_ID                 0x02U
#define V851_TLV_TAG_BOOT_API_ENDPOINT           0x03U
#define V851_TLV_TAG_BOOT_BIND_STATUS            0x04U
#define V851_TLV_TAG_BOOT_QR_URL                 0x05U
/* EventAlarm: T5L -> V851. */
#define V851_TLV_TAG_EA_IS_ALARM                 0x01U
#define V851_TLV_TAG_EA_CODE                     0x02U
#define V851_TLV_TAG_EA_LEVEL                    0x03U
#define V851_TLV_TAG_EA_RECOVERED                0x04U
#define V851_TLV_TAG_EA_PAYLOAD                  0x05U

#define V851_BOOTSTRAP_DEVICE_SN_SIZE            64U
#define V851_BOOTSTRAP_BLE_ID_SIZE               32U
#define V851_BOOTSTRAP_API_ENDPOINT_SIZE         256U
#define V851_BOOTSTRAP_BIND_STATUS_SIZE          32U
#define V851_BOOTSTRAP_QR_URL_SIZE               256U

typedef struct
{
    uint8_t struct_type;
    char device_sn[V851_BOOTSTRAP_DEVICE_SN_SIZE];
    char ble_id[V851_BOOTSTRAP_BLE_ID_SIZE];
    char api_endpoint[V851_BOOTSTRAP_API_ENDPOINT_SIZE];
    char bind_status[V851_BOOTSTRAP_BIND_STATUS_SIZE];
    char qr_url[V851_BOOTSTRAP_QR_URL_SIZE];
} V851BootstrapResult;

typedef struct
{
    uint8_t struct_type;
    const uint8_t *payload;
    uint16_t length;
} V851TlvSegment;

typedef struct
{
    uint8_t tag;
    const uint8_t *value;
    uint16_t length;
} V851TlvField;

typedef struct
{
    const uint8_t *bytes;
    uint16_t length;
    uint16_t offset;
} V851TlvFieldCursor;

typedef enum
{
    V851_TLV_ITER_END = 0,
    V851_TLV_ITER_FIELD = 1,
    V851_TLV_ITER_INVALID = 2
} V851TlvIterResult;

void V851ProtocolInit(void);
void V851ProtocolTask(void);
void V851ProtocolRequestFactoryReport(void);
void V851ProtocolRequestFactoryReset(void);
void V851ProtocolReceiveFrame(const uint8_t *frame, uint16_t len);
uint8_t V851ProtocolSendSegments(uint8_t command,
                                 const V851TlvSegment *segments,
                                 uint8_t count);
uint8_t V851ProtocolSendWifiFrame(const uint8_t *frame, uint16_t len);

void V851TlvFieldCursorInit(V851TlvFieldCursor *cursor,
                            const uint8_t *bytes,
                            uint16_t length);
V851TlvIterResult V851TlvFieldNext(V851TlvFieldCursor *cursor,
                                   V851TlvField *field);
uint8_t V851TlvWriteU8(uint8_t *buffer, uint16_t capacity,
                       uint16_t *offset, uint8_t tag, uint8_t value);
uint8_t V851TlvWriteU32(uint8_t *buffer, uint16_t capacity,
                        uint16_t *offset, uint8_t tag, uint32_t value);
uint8_t V851TlvWriteBytes(uint8_t *buffer, uint16_t capacity,
                          uint16_t *offset, uint8_t tag,
                          const uint8_t *value, uint16_t length);
uint8_t V851TlvWriteBinary64Uint16(uint8_t *buffer, uint16_t capacity,
                                   uint16_t *offset, uint8_t tag,
                                   uint16_t value);
uint8_t V851TlvReadBinary64Uint16(const uint8_t *value,
                                  uint16_t length,
                                  uint16_t *result);

/* Returns NULL until a complete 0x37/0x6C result has been received. */
const V851BootstrapResult *V851BootstrapResultGet(void);

/* Single project-level extension hook; Bootstrap is cached by its implementation. */
void V851TlvApplicationSegment(uint8_t command,
                               const V851TlvSegment *segment);

#endif /* v851PROTOCOL_ENABLED */

#endif /* V851_PROTOCOL_H */

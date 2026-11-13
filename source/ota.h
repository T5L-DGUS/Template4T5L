/**
 * @file    ota.h
 * @brief   OTA升级接口驱动程序头文件
 * @details 本文件提供OTA升级接口的管理和控制功能，支持通过UART等接口进行固件升级
 * @author  yangming
 * @version 1.0.0
 */

#ifndef __OTA_H__
#define __OTA_H__
#include "T5LOSConfig.h"
#include "sys.h"

#define OTA_TASK_INTERVAL      100


void OTAInit(void);
void OTATask(void);



#endif /* __OTA_H__ */
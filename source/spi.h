/**
 * @file spi.h
 * @brief 8051单片机SPI协议驱动头文件
 * @details 提供SPI通信的初始化、数据读写等核心功能接口
 * @author yangming
 * @version 1.0.0
 */

#ifndef SPI_H
#define SPI_H

#include "T5LOSConfig.h"

#if spiSPI_ENABLED
/* SPI引脚位操作定义 */
/**
 * @brief SPI时钟引脚位操作定义
 * @details SCK时钟线的位级操作变量定义
 */
sbit spiSCK = spiGPIO_PORT^spiSCK_PIN;

/**
 * @brief SPI主机输出引脚位操作定义
 * @details MOSI数据线的位级操作变量定义
 */
sbit spiMOSI = spiGPIO_PORT^spiMOSI_PIN;

/**
 * @brief SPI主机输入引脚位操作定义
 * @details MISO数据线的位级操作变量定义
 */
sbit spiMISO = spiGPIO_PORT^spiMISO_PIN;

/**
 * @brief SPI片选引脚位操作定义
 * @details CS片选信号的位级操作变量定义
 */
sbit spiCS = spiGPIO_PORT^spiCS_PIN;

/* SPI时序配置 */
/**
 * @brief SPI时钟延时配置
 * @details SPI通信时钟周期的半周期延时时间，单位为微秒
 */
#define spiCLOCK_DELAY                  5

/* SPI传输模式定义 */
/**
 * @brief SPI模式0定义
 * @details CPOL=0, CPHA=0：时钟空闲时为低电平，第一个时钟边沿采样数据
 * @details CPOL=0, CPHA=1：时钟空闲时为低电平，第二个时钟边沿采样数据
 * @details CPOL=1, CPHA=0：时钟空闲时为高电平，第一个时钟边沿采样数据
 * @details CPOL=1, CPHA=1：时钟空闲时为高电平，第二个时钟边沿采样数据
 */
#define spiMODE_0                       0
#define spiMODE_1                       1
#define spiMODE_2                       2
#define spiMODE_3                       3

/**
 * @brief 当前使用的SPI模式
 * @details 默认使用SPI模式0，可根据外设要求修改
 */
#define spiCURRENT_MODE                 spiMODE_0


/* SPI功能函数声明 */

/**
 * @brief SPI初始化函数
 * @details 初始化SPI接口的GPIO引脚和通信参数
 * @param[in] mode SPI工作模式（0-3）
 */
void SpiInit(uint8_t mode);

/**
 * @brief SPI发送单字节数据
 * @details 通过SPI接口发送一个字节的数据
 * @param[in] data 要发送的数据字节
 */
void SpiSendByte(uint8_t data);

/**
 * @brief SPI接收单字节数据
 * @details 通过SPI接口接收一个字节的数据
 * @param[out] data 接收数据的指针
 */
void SpiReceiveByte(uint8_t *data);

/**
 * @brief SPI传输单字节数据（同时发送和接收）
 * @details 通过SPI接口同时发送和接收一个字节的数据
 * @param[in] send_data 要发送的数据字节
 * @param[out] receive_data 接收数据的指针
 */
void SpiTransferByte(uint8_t send_data, uint8_t *receive_data);

/**
 * @brief SPI发送多字节数据
 * @details 通过SPI接口发送多个字节的数据
 * @param[in] data 要发送的数据缓冲区指针
 * @param[in] length 要发送的数据长度
 */
void SpiSendBuffer(const uint8_t *data, uint16_t length);

/**
 * @brief SPI接收多字节数据
 * @details 通过SPI接口接收多个字节的数据
 * @param[out] data 接收数据的缓冲区指针
 * @param[in] length 要接收的数据长度
 */
void SpiReceiveBuffer(uint8_t *data, uint16_t length);

/**
 * @brief SPI传输多字节数据（同时发送和接收）
 * @details 通过SPI接口同时发送和接收多个字节的数据
 * @param[in] send_data 要发送的数据缓冲区指针
 * @param[out] receive_data 接收数据的缓冲区指针
 * @param[in] length 要传输的数据长度
 */
void SpiTransferBuffer(const uint8_t *send_data, uint8_t *receive_data, uint16_t length);

/**
 * @brief SPI片选信号控制
 * @details 控制SPI片选信号的电平状态
 * @param[in] state 片选信号状态（0：选中设备，1：取消选中）
 */
void SpiChipSelect(uint8_t state);

/**
 * @brief SPI设置传输模式
 * @details 动态设置SPI的传输模式
 * @param[in] mode SPI工作模式（0-3）
 */
void SpiSetMode(uint8_t mode);

#endif /* spiSPI_ENABLED */

#endif /* SPI_H */

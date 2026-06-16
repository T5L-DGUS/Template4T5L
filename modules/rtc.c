/**
 * @file    rtc.c
 * @brief   实时时钟(RTC)驱动程序实现文件
 * @details 本文件实现了RTC芯片的完整驱动功能，支持多种RTC芯片，
 *          包括时间格式转换、芯片通信和周期性维护任务
 * @author  yangming
 * @version 1.0.0
 */

#include "rtc.h"
#include <string.h>

/**
 * @brief BCD码转十六进制宏定义
 * @details 将BCD(Binary Coded Decimal)格式转换为普通十六进制格式
 * @param bcd BCD格式的数值
 * @return 转换后的十六进制数值
 * @note BCD格式：高4位表示十位，低4位表示个位
 */
#define rtcBCD_2_HEX(bcd) ((bcd >> 4) * 10 + (bcd & 0x0F))
#define rtcHEX_2_BCD(hex) (((hex / 10) << 4) | (hex % 10))

/**
 * @brief 计算指定日期的星期值
 * @details 根据年月日计算对应的星期数，使用蔡勒公式
 * @param[in] prtc_set 日期数据指针，格式为[年, 月, 日, ...]
 * @return 星期值 (0=周日, 1=周一, ..., 6=周六)
 * @pre prtc_set必须指向有效的日期数据
 * @pre 年份为20xx格式的后两位
 * @pre 月份范围1-12，日期范围1-31
 * @post 返回计算得出的星期值
 * @note 使用蔡勒公式：w = (d + [(13*(m+1))/5] + y + [y/4] - [y/100] + [y/400]) mod 7
 * @note 1月和2月按上一年的13月和14月计算
 * @warning 输入的日期必须是有效日期，否则结果不准确
 */
static uint8_t RtcCalcWeek(uint8_t *prtc_set)
{
    uint16_t year = (uint16_t)prtc_set[0] + 2000U;
    uint8_t month = prtc_set[1];
    uint8_t day = prtc_set[2];
    uint8_t week;

    if(month < 3)
    {
        month += 12;
        year--;
    }
    week = (day + (13 * (month + 1)) / 5 + year + (year / 4) - (year / 100) + (year / 400)) % 7 + 1;
    return week;
}

/**
 * @brief RTC时间数据格式转换函数
 * @details 将从RTC芯片读取的原始数据转换为标准时间格式
 * @param[in] prtc_get 从RTC芯片读取的原始时间数据指针
 * @param[out] prtc_out 转换后的标准时间数据输出指针
 * @return 无

 * @post prtc_out包含转换后的标准时间格式数据
 * @note 输出格式：[年, 月, 日, 星期, 时, 分, 秒, 保留]
 * @note 自动进行BCD到十六进制的转换
 * @note 星期值会减1以从0开始计数
 */
static void RtcGetTime(uint8_t *prtc_get,uint8_t *prtc_out)
{
    uint8_t i;
    for(i = 0; i < 3; i++)
    {
        prtc_out[i] = rtcBCD_2_HEX(prtc_get[6-i]);
    }
    if((prtc_get[3] & 0x07) == 0U)
    {
        prtc_out[3] = 0U;
    }else
    {
        prtc_out[3] = (prtc_get[3] & 0x07) - 1;
    }
    for(i = 4; i < 7; i++)
    {
        prtc_out[i] = rtcBCD_2_HEX(prtc_get[6-i]);
    }
    prtc_out[7] = 0;
}

/* RX-8130 RTC芯片驱动实现 */
#ifdef rtcRX_8130
void RtcSetTime(uint8_t *prtc_set)
{
    uint8_t write_param[7];
    I2cWriteSingleByte(0x30, 0x00); 
    write_param[0] = 0x48; 
    write_param[1] = 0x00;
    write_param[2] = 0x40;
    write_param[3] = 0x10;
    I2cWriteMultipleBytes(0x1c, write_param, 4);
    write_param[0] = prtc_set[6];
    write_param[1] = prtc_set[5];
    write_param[2] = prtc_set[4];
    write_param[3] = prtc_set[3];
    write_param[4] = prtc_set[2];
    write_param[5] = prtc_set[1];
    write_param[6] = prtc_set[0];
    I2cWriteMultipleBytes(0x10, write_param, 7);
    write_param[0] = 0x00;
    write_param[1] = 0x10;
    I2cWriteMultipleBytes(0x1e, write_param, 2);
}


void RtcInit(void)
{
    uint8_t data reset_status;
    uint8_t write_param[7] = {0x00, 0x00, 0x00, 0x01, 0x01, 0x01, 0x20};
    GPIO_BYTE_SET_OUT(i2cGPIO_SFR_PORTMDOUT, (1 << i2cSDA_GPIO_PIN) | (1 << i2cSCL_GPIO_PIN));
    reset_status = I2cReadSingleByte(0x1d);
    if((reset_status & 0x02) == 0x02)
    {
        RtcSetTime(write_param);
    }
}


void RtcReadTime(void)
{
    uint8_t read_param[8],write_param[8];
    I2cReadMultipleBytes(0x10, read_param, 7);
    RtcGetTime(read_param, write_param);
    write_dgus_vp(0x0010, write_param, 4);
}

/* SD-2058 RTC芯片驱动实现 */
#elif defined(rtcSD_2058)
void RtcSetTime(uint8_t *prtc_set)
{
    uint8_t write_param[7];
    uint8_t read_param[2];
    I2cReadMultipleBytes(0x0f, read_param, 2);
    read_param[0] |= 0x84;
    read_param[1] |= 0x80;
    I2cWriteSingleByte(0x10, read_param[1]);
    I2cWriteSingleByte(0x0f, read_param[0]);
    write_param[0] = prtc_set[6];
    write_param[1] = prtc_set[5];
    write_param[2] = 0x80;
    write_param[3] = prtc_set[3] % 7;
    write_param[4] = prtc_set[2];
    write_param[5] = prtc_set[1];
    write_param[6] = prtc_set[0];
    I2cWriteMultipleBytes(0x00, write_param, 7);
    read_param[0] &= ~0x84;
    read_param[1] &= ~0x80;
    I2cWriteSingleByte(0x10, read_param[1]);
    I2cWriteSingleByte(0x0f, read_param[0]);
}


void RtcInit(void)
{
    uint8_t data read_param[2];
    uint8_t write_param[7] = {0x00, 0x00, 0x00, 0x01, 0x01, 0x01, 0x20};
    GPIO_BYTE_SET_OUT(i2cGPIO_SFR_PORTMDOUT, (1 << i2cSDA_GPIO_PIN) | (1 << i2cSCL_GPIO_PIN));

    I2cReadMultipleBytes(0x0f, read_param, 2);
    if(read_param[0] & 0x01)
    {
        if(read_param[0] & 0x84)
            I2cWriteSingleByte(0x0f, read_param[0] & ~0x84);
        read_param[0] &= ~0x01;
        if(read_param[1] & 0x80)
            I2cWriteSingleByte(0x10, read_param[1] & ~0x80);
        read_param[1] |= 0x80;
        read_param[0] |= 0x84;
        I2cWriteSingleByte(0x10, read_param[1]);
        I2cWriteSingleByte(0x0f, read_param[0]);
    } 
}


void RtcReadTime(void)
{
    uint8_t read_param[8],write_param[8];
    I2cReadMultipleBytes(0x00, read_param, 7);
    RtcGetTime(read_param, write_param);
    write_dgus_vp(0x0010, write_param, 4);
}

/* 空实现 - 未选择具体RTC芯片时的占位函数 */
#else
void RtcInit(void)
{
    __NOP();
}


void RtcSetTime(uint8_t *prtc_set)
{
    __NOP();
}


void RtcReadTime(void)
{
    __NOP();
}
#endif  /* rtcRX_8130 || rtcSD_2058 */

void RtcWriteTime(void)
{
    uint8_t read_param[8],write_param[8];
    uint8_t i;
    read_dgus_vp(0x009c, read_param, 4);
    if(read_param[0] == 0x5a && read_param[1] == 0xa5)
    {
        memcpy(write_param, read_param + 2, 3);
        write_param[3] = RtcCalcWeek(write_param);
        memcpy(write_param + 4, read_param + 5, 3);
        write_param[7] = 0;
        for(i=0;i<8;i++)
        {
            write_param[i] = rtcHEX_2_BCD(write_param[i]);
        }
        RtcSetTime(write_param);
        memset(read_param, 0, 2);
        write_dgus_vp(0x009c, read_param, 1);
    }
}


void RtcTask(void)
{
    RtcReadTime();
    RtcWriteTime();
}

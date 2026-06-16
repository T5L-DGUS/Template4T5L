/**
 * @file spi.c
 * @brief 8051单片机SPI协议驱动实现文件
 * @details 实现SPI通信的初始化、数据读写等核心功能
 * @author yangming
 * @version 1.0.0
 */

#include "spi.h"
#include "sys.h"

#if spiSPI_ENABLED

/**
 * @brief 当前SPI工作模式
 * @details 记录当前SPI接口使用的工作模式
 */
static uint8_t current_spi_mode = spiCURRENT_MODE;

/**
 * @brief SPI延时函数
 * @details 提供SPI时序所需的精确延时
 * @note 这是一个静态函数，仅在本文件内部使用
 */
static void SpiDelay(void)
{
    delay_us(spiCLOCK_DELAY);
}

/**
 * @brief SPI时钟脉冲生成
 * @details 根据当前SPI模式生成一个完整的时钟脉冲
 * @note 这是一个静态函数，仅在本文件内部使用
 */
static void SpiClockPulse(void)
{
    switch(current_spi_mode)
    {
        case spiMODE_0:  /* CPOL=0, CPHA=0 */
        case spiMODE_1:  /* CPOL=0, CPHA=1 */
            spiSCK = 0;
            SpiDelay();
            spiSCK = 1;
            SpiDelay();
            spiSCK = 0;
            break;
            
        case spiMODE_2:  /* CPOL=1, CPHA=0 */
        case spiMODE_3:  /* CPOL=1, CPHA=1 */
            spiSCK = 1;
            SpiDelay();
            spiSCK = 0;
            SpiDelay();
            spiSCK = 1;
            break;
            
        default:
            break;
    }
}


void SpiInit(uint8_t mode)
{
    
    /* 保存当前模式 */
    current_spi_mode = mode;
    
    /* 初始化GPIO引脚 */
    /* 设置SCK为输出引脚 */
    spiSCK = (mode == spiMODE_2 || mode == spiMODE_3) ? 1 : 0;
    
    /* 设置MOSI为输出引脚，初始为高电平 */
    spiMOSI = 1;
    
    /* 设置MISO为输入引脚（通常由硬件配置决定） */
    /* MISO引脚方向由外部上拉电阻或外设控制 */
    
    /* 设置CS为输出引脚，初始为高电平（未选中） */
    spiCS = 1;
    
}


void SpiSendByte(uint8_t data)
{
    uint8_t i;
    uint8_t bit_value;
    
    /* 发送8位数据，MSB优先 */
    for(i = 0; i < 8; i++)
    {
        /* 提取最高位 */
        bit_value = (data & 0x80) ? 1 : 0;
        data <<= 1;
        
        /* 根据SPI模式设置数据和时钟时序 */
        switch(current_spi_mode)
        {
            case spiMODE_0:  /* CPOL=0, CPHA=0 */
                spiMOSI = bit_value;    /* 在时钟上升沿之前设置数据 */
                SpiDelay();
                spiSCK = 1;             /* 时钟上升沿，从设备采样数据 */
                SpiDelay();
                spiSCK = 0;             /* 时钟下降沿 */
                break;
                
            case spiMODE_1:  /* CPOL=0, CPHA=1 */
                spiSCK = 1;             /* 时钟上升沿 */
                SpiDelay();
                spiMOSI = bit_value;    /* 在时钟下降沿之前设置数据 */
                spiSCK = 0;             /* 时钟下降沿，从设备采样数据 */
                SpiDelay();
                break;
                
            case spiMODE_2:  /* CPOL=1, CPHA=0 */
                spiMOSI = bit_value;    /* 在时钟下降沿之前设置数据 */
                SpiDelay();
                spiSCK = 0;             /* 时钟下降沿，从设备采样数据 */
                SpiDelay();
                spiSCK = 1;             /* 时钟上升沿 */
                break;
                
            case spiMODE_3:  /* CPOL=1, CPHA=1 */
                spiSCK = 0;             /* 时钟下降沿 */
                SpiDelay();
                spiMOSI = bit_value;    /* 在时钟上升沿之前设置数据 */
                spiSCK = 1;             /* 时钟上升沿，从设备采样数据 */
                SpiDelay();
                break;
                
            default:
                break;
        }
    }
}


void SpiReceiveByte(uint8_t *data)
{
    uint8_t i;
    uint8_t received_data = 0;
    
    
    /* 接收8位数据，MSB优先 */
    for(i = 0; i < 8; i++)
    {
        received_data <<= 1;
        
        /* 根据SPI模式设置时钟时序并读取数据 */
        switch(current_spi_mode)
        {
            case spiMODE_0:  
                spiSCK = 1;             
                SpiDelay();
                if(spiMISO)             
                {
                    received_data |= 0x01;
                }
                spiSCK = 0;            
                SpiDelay();
                break;
                
            case spiMODE_1:  
                spiSCK = 1;             
                SpiDelay();
                spiSCK = 0;             
                SpiDelay();
                if(spiMISO)            
                {
                    received_data |= 0x01;
                }
                break;
                
            case spiMODE_2: 
                spiSCK = 0;             
                SpiDelay();
                if(spiMISO)            
                {
                    received_data |= 0x01;
                }
                spiSCK = 1;            
                SpiDelay();
                break;
                
            case spiMODE_3:  
                spiSCK = 0;            
                SpiDelay();
                spiSCK = 1;             
                SpiDelay();
                if(spiMISO)             
                {
                    received_data |= 0x01;
                }
                break;
                
            default:
                break;
        }
    }
    
    *data = received_data;
}


void SpiTransferByte(uint8_t send_data, uint8_t *receive_data)
{
    uint8_t i;
    uint8_t bit_value;
    uint8_t received_data = 0;
    
    
    for(i = 0; i < 8; i++)
    {
        bit_value = (send_data & 0x80) ? 1 : 0;
        send_data <<= 1;
        received_data <<= 1;
        
        /* 根据SPI模式设置数据和时钟时序 */
        switch(current_spi_mode)
        {
            case spiMODE_0:  
                spiMOSI = bit_value;    
                SpiDelay();
                spiSCK = 1;             
                SpiDelay();
                if(spiMISO)             
                {
                    received_data |= 0x01;
                }
                spiSCK = 0;             
                break;
                
            case spiMODE_1:  
                spiSCK = 1;             
                SpiDelay();
                spiMOSI = bit_value;   
                spiSCK = 0;             
                SpiDelay();
                if(spiMISO)             
                {
                    received_data |= 0x01;
                }
                break;
                
            case spiMODE_2: 
                spiMOSI = bit_value;    
                SpiDelay();
                spiSCK = 0;            
                SpiDelay();
                if(spiMISO)             
                {
                    received_data |= 0x01;
                }
                spiSCK = 1;             
                break;
                
            case spiMODE_3: 
                spiSCK = 0;            
                SpiDelay();
                spiMOSI = bit_value;    
                spiSCK = 1;             
                SpiDelay();
                if(spiMISO)             
                {
                    received_data |= 0x01;
                }
                break;
                
            default:
                break;
        }
    }
    
    *receive_data = received_data;
}


void SpiSendBuffer(const uint8_t *data, uint16_t length)
{
    uint16_t i;
    
    for(i = 0; i < length; i++)
    {
        SpiSendByte(data[i]);
    }
}


void SpiReceiveBuffer(uint8_t *data, uint16_t length)
{
    uint16_t i;
    
    for(i = 0; i < length; i++)
    {
        SpiReceiveByte(&data[i]);
    }
}


void SpiTransferBuffer(const uint8_t *send_data, uint8_t *receive_data, uint16_t length)
{
    uint16_t i;
    
    for(i = 0; i < length; i++)
    {
        SpiTransferByte(send_data[i], &receive_data[i]);
    }
}


void SpiChipSelect(uint8_t state)
{
    spiCS = state ? 1 : 0;
}


void SpiSetMode(uint8_t mode)
{
    current_spi_mode = mode;
    
    spiSCK = (mode == spiMODE_2 || mode == spiMODE_3) ? 1 : 0;

}

#endif /* spiSPI_ENABLED */

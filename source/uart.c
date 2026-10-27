#include "app_ota.h"
#include "fw_protocol.h"
#include "timer.h"

/**
 * @file    uart.c
 * @brief   UART通信接口驱动程序实现文件
 * @details 本文件实现了多路UART通信接口的完整功能，包括硬件初始化、
 *          中断处理、数据收发和多协议支持
 * @author  yangming
 * @version 1.0.0
 */

#include "uart.h"
#include "T5LOSConfig.h"
#include <string.h>
#include "sys.h"

#if uartMODBUS_PROTOCOL_ENABLED
#include "modbus.h"
#endif /* uartMODBUS_PROTOCOL_ENABLED */

#if uartTA_PROTOCOL_ENABLED
#include "TA_protocal.h"
#endif /* uartTA_PROTOCOL_ENABLED */

#if sysSET_FROM_LIB
uint16_t sys_2k_ratio;
uint32_t sysFOSC;
uint32_t sysFCLK;
#endif /* sysSET_FROM_LIB */

#if sysBEAUTY_MODE_ENABLED
#include "r11_netskinAnalyze.h"
#include "r11_common.h"
#endif /* sysBEAUTY_MODE_ENABLED */

#if sysN5CAMERA_MODE_ENABLED
#include "r11_common.h"
#include "r11_n5camera.h"
#endif /* sysN5CAMERA_MODE_ENABLED */

#if sysADVERTISE_MODE_ENABLED
#include "r11_common.h"
#include "r11_advertise.h"
#endif /* sysADVERTISE_MODE_ENABLED */


#if uartUART2_ENABLED
UART_TYPE Uart2;
static uint8_t xdata uart2_partial[258];
static uint16_t uart2_partial_size;
static uint32_t uart2_last_rx;
uint8_t Uart2TxBuffer[uartUART2_TXBUF_SIZE+1];
uint8_t Uart2RxBuffer[uartUART2_RXBUF_SIZE+1];
void Uart2Init(const uint32_t bdt)
{
uint32_t baud;
    uart2_partial_size = 0;
    uart2_last_rx = 0;
    memset((uint8_t *)&Uart2, 0, sizeof(UART_TYPE));
    memset((uint8_t *)Uart2TxBuffer, 0, uartUART2_TXBUF_SIZE);
    memset((uint8_t *)Uart2RxBuffer, 0, uartUART2_RXBUF_SIZE);

    MUX_SEL |= 0x40;
    P0MDOUT &= 0xCF;
    P0MDOUT |= 0x10;
    P0      |= 0x30;
    ADCON = 0x80;
    SCON0 = 0x50;
    PCON &= 0x7F;
    PCON |= 0x80;

    #if sysSET_FROM_LIB
    if(sys_2k_ratio)
    {
        PCON |= 0x80;
    }
    #else
    #if sys2K_RATIO
    PCON &= ~0x80;
    #endif /* sys2K_RATIO */
    #endif /* sysSET_FROM_LIB */
    
    if(PCON & 0x80)
    {
        baud = 1024- ( uint16_t)(sysFOSC/32/bdt);
    }else
    {
        baud = 1024 - ( uint16_t)(sysFOSC/64/bdt);
    }
    SREL0H = (baud>>8) & 0xff;
    SREL0L = baud & 0xff;

    #if uartUART2_485_ENABLED
    P0MDOUT |= 0x01;
    TR4 = 0;
    #endif  /* uartUART2_485_ENABLED */

    ES0 = 1;
    EA = 1;
}

/**
 * @brief UART2收发中断服务程序
 * @details 处理UART2的接收和发送中断，管理数据缓冲区和传输状态
 * @note 中断号4，处理RI0和TI0中断标志
 * @note 自动管理RS485方向控制（如果启用）
 */
void Uart2TxRxIsr()   interrupt 4
{
    if ( RI0 == 1 )
    {
        Uart2RxBuffer[Uart2.RxHead++] = SBUF0;
        Uart2.RxHead %= uartUART2_RXBUF_SIZE;
        Uart2.RxFlag = UART_RECING;
        #if uartUART2_TIMEOUT_ENABLED
        Uart2.RxTimeout = uartUART2_TIMEOUTSET;
        #else
        Uart2.RxTimeout = 0;
        #endif /* uartUART2_TIMEOUT_ENABLED */
        RI0 = 0; 
    } 
    if( TI0 == 1 )
    {
		TI0 = 0;
        if (Uart2.TxHead != Uart2.TxTail)
        {
            SBUF0 = Uart2TxBuffer[Uart2.TxTail++];
            Uart2.TxTail %= uartUART2_TXBUF_SIZE;
        }else
        {
            Uart2.TxBusy = 0;
            #if uartUART2_485_ENABLED
            {
                TR4 = 0; 
            }
            #endif /* uartUART2_485_ENABLED */
        } 
    }
}
#endif  /* uartUART2_ENABLED */

#if uartUART3_ENABLED
UART_TYPE Uart3;
uint8_t Uart3TxBuffer[uartUART3_TXBUF_SIZE+1];
uint8_t Uart3RxBuffer[uartUART3_RXBUF_SIZE+1];
void Uart3Init(const uint32_t bdt)
{
    uint32_t baud;
    memset((uint8_t *)&Uart3, 0, sizeof(UART_TYPE));
    memset((uint8_t *)Uart3TxBuffer, 0, uartUART3_TXBUF_SIZE);
    memset((uint8_t *)Uart3RxBuffer, 0, uartUART3_RXBUF_SIZE);

    MUX_SEL |= 0x20;
    P0MDOUT &=~0x80;
    P0MDOUT |= 0x40;
    SCON1 = 0x90;
    SCON1 = 0x90;
    
    baud = 1024- ( uint16_t ) (6451200.0f/bdt);
    SREL1H = (baud>>8) & 0xff;
    SREL1L = baud & 0xff;

    IEN2 |= 0x01;
    EA = 1;
}

/**
 * @brief UART3收发中断服务程序
 * @details 处理UART3的接收和发送中断，管理数据缓冲区和传输状态
 * @note 中断号16，处理SCON1的接收和发送标志位
 */
void Uart3TxRxIsr()   interrupt 16
{
    if ( (SCON1&0x01) == 0x01 )
    {
        Uart3RxBuffer[Uart3.RxHead++] = SBUF1;
        Uart3.RxHead %= uartUART3_RXBUF_SIZE;
        Uart3.RxFlag = UART_RECING;
        #if uartUART3_TIMEOUT_ENABLED
        Uart3.RxTimeout = uartUART3_TIMEOUTSET;
        #else
        Uart3.RxTimeout = 0;
        #endif
        SCON1 &= ~0x01;
        SCON1 &= ~0x01;
    } 
    if( (SCON1&0x02) == 0x02 )
    {
		SCON1 &= ~0x02;
        SCON1 &= ~0x02;
        if (Uart3.TxHead != Uart3.TxTail)
        {
            SBUF1 = Uart3TxBuffer[Uart3.TxTail++];
            Uart3.TxTail %= uartUART3_TXBUF_SIZE;
        }else
        {
            Uart3.TxBusy = 0;
        } 
    }
}
#endif /* uartUART3_ENABLED */

#if uartUART4_ENABLED
UART_TYPE Uart4;
static uint8_t xdata uart4_partial[258];
static uint16_t uart4_partial_size;
static uint32_t uart4_last_rx;
uint8_t Uart4TxBuffer[uartUART4_TXBUF_SIZE+1];
uint8_t Uart4RxBuffer[uartUART4_RXBUF_SIZE+1];
void Uart4Init(const uint32_t bdt)
{
    uint32_t baud;
    uart4_partial_size = 0;
    uart4_last_rx = 0;
    memset((uint8_t *)&Uart4, 0, sizeof(UART_TYPE));
    memset((uint8_t *)Uart4TxBuffer, 0, uartUART4_TXBUF_SIZE);
    memset((uint8_t *)Uart4RxBuffer, 0, uartUART4_RXBUF_SIZE);

    P0MDOUT |= 0x01;
    #if uartUART4_485_ENABLED
    TR4 = 0;
    #endif /* uartUART4_485_ENABLED */

    SCON2T=0x80;
    SCON2R=0x80;
    
    #if sysSET_FROM_LIB
    if(sys_2k_ratio)
    {
        baud = (uint16_t)(sysFCLK/16/bdt);
    }else
    {
        baud = (uint16_t)(sysFCLK/8/bdt);
    }
    #else
    #if sys2K_RATIO
    baud = (uint16_t)(sysFCLK/16/bdt);
    #else
    baud = (uint16_t)(sysFCLK/8/bdt);
    #endif /* sys2K_RATIO */
    #endif /* sysSET_FROM_LIB */
    BODE2_DIV_H = (baud>>8) & 0xff;
    BODE2_DIV_L = baud & 0xff;

    ES2T = 1;
    ES2R = 1;
    EA   = 1;
}

/**
 * @brief UART4接收中断服务程序
 * @details 处理UART4的接收中断，管理接收数据缓冲区
 * @note 中断号11，处理SCON2R接收标志位
 */
void Uart4RxIsr()   interrupt 11
{
    if((SCON2R&0x01) == 0x01)
    {
        Uart4RxBuffer[Uart4.RxHead++] = SBUF2_RX;
        Uart4.RxHead %= uartUART4_RXBUF_SIZE;
        Uart4.RxFlag = UART_RECING;
        #if uartUART4_TIMEOUT_ENABLED
        Uart4.RxTimeout = uartUART4_TIMEOUTSET;
        #else
        Uart4.RxTimeout = 0;
        #endif
        SCON2R &= 0xFE;
    } 
}

/**
 * @brief UART4发送中断服务程序
 * @details 处理UART4的发送中断，管理发送数据缓冲区和传输状态
 * @note 中断号10，处理SCON2T发送标志位
 * @note 自动管理RS485方向控制（如果启用）
 */
void Uart4TxIsr()   interrupt 10
{
    if((SCON2T&0x01) == 0x01)
    {
        SCON2T &= 0xFE;
        if (Uart4.TxHead != Uart4.TxTail)
        {
            SBUF2_TX = Uart4TxBuffer[Uart4.TxTail++];
            Uart4.TxTail %= uartUART4_TXBUF_SIZE;
        }else
        {
            Uart4.TxBusy = 0;
            #if uartUART4_485_ENABLED
            {
                TR4 = 0; 
            }
            #endif /* uartUART4_485_ENABLED */
        } 
    }
}
#endif /* uartUART4_ENABLED */

#if uartUART5_ENABLED
UART_TYPE Uart5;
uint8_t Uart5TxBuffer[uartUART5_TXBUF_SIZE+1];
uint8_t Uart5RxBuffer[uartUART5_RXBUF_SIZE+1];
void Uart5Init(const uint32_t bdt)
{
    uint32_t baud;
    memset((uint8_t *)&Uart5, 0, sizeof(UART_TYPE));
    memset((uint8_t *)Uart5TxBuffer, 0, uartUART5_TXBUF_SIZE);
    memset((uint8_t *)Uart5RxBuffer, 0, uartUART5_RXBUF_SIZE);

    #if uartUART5_485_ENABLED
    P0MDOUT |= 0x02;
    TR5 = 0;
    #endif

    SCON3T=0x80;
	SCON3R=0x80;
    
    #if sysSET_FROM_LIB
    if(sys_2k_ratio)
    {
        baud = (uint16_t)(sysFCLK/16/bdt);
    }else
    {
        baud = (uint16_t)(sysFCLK/8/bdt);
    }
    #else
    #if sys2K_RATIO
    baud = (uint16_t)(sysFCLK/16/bdt);
    #else
    baud = (uint16_t)(sysFCLK/8/bdt);
    #endif /* sys2K_RATIO */
    #endif /* sysSET_FROM_LIB */
    BODE3_DIV_H = (baud>>8) & 0xff;
    BODE3_DIV_L = baud & 0xff;

    ES3T = 1;
    ES3R = 1;
    EA   = 1;
}

/**
 * @brief UART5接收中断服务程序
 * @details 处理UART5的接收中断，管理接收数据缓冲区
 * @note 中断号13，处理SCON3R接收标志位
 */
void Uart5RxIsr()   interrupt 13
{
    if((SCON3R&0x01) == 0x01)
    {
        Uart5RxBuffer[Uart5.RxHead++] = SBUF3_RX;
        Uart5.RxHead %= uartUART5_RXBUF_SIZE;
        Uart5.RxFlag = UART_RECING;
        #if uartUART5_TIMEOUT_ENABLED
        Uart5.RxTimeout = uartUART5_TIMEOUTSET;
        #else
        Uart5.RxTimeout = 0;
        #endif
        SCON3R &= 0xFE;
    } 
}

/**
 * @brief UART5发送中断服务程序
 * @details 处理UART5的发送中断，管理发送数据缓冲区和传输状态
 * @note 中断号12，处理SCON3T发送标志位
 * @note 自动管理RS485方向控制（如果启用）
 */
void Uart5TxIsr()   interrupt 12
{
    if((SCON3T&0x01) == 0x01)
    {
        SCON3T &= 0xFE;
        if (Uart5.TxHead != Uart5.TxTail)
        {
            SBUF3_TX = Uart5TxBuffer[Uart5.TxTail++];
            Uart5.TxTail %= uartUART5_TXBUF_SIZE;
        }else
        {
            Uart5.TxBusy = 0;
            #if uartUART5_485_ENABLED
            {
                TR5 = 0; 
            }
            #endif /* uartUART5_485_ENABLED */
        } 
    }
}
#endif /* uartUART5_ENABLED */

void UartInit(void)
{
    #if uartUART2_ENABLED
    Uart2Init(uartUART2_BAUDRATE);
    #endif /* uartUART2_ENABLED */

    #if uartUART3_ENABLED
    Uart3Init(uartUART3_BAUDRATE);
    #endif /* uartUART3_ENABLED */

    #if uartUART4_ENABLED
    Uart4Init(uartUART4_BAUDRATE);
    #endif /* uartUART4_ENABLED */

    #if uartUART5_ENABLED
    Uart5Init(uartUART5_BAUDRATE);
    #endif /* uartUART5_ENABLED */
}


void UartSendData(UART_TYPE *uart, uint8_t *buf, uint16_t len)
{
    uint16_t i;

    for(i=0; i<len; i++)
    {
        #if uartUART2_ENABLED
            if(uart == &Uart2)
            {
                Uart2TxBuffer[uart->TxHead++] = *buf++;
                uart->TxHead %= uartUART2_TXBUF_SIZE;
            }
        #endif

        #if uartUART3_ENABLED
            if(uart == &Uart3)
            {
                Uart3TxBuffer[uart->TxHead++] = *buf++;
                uart->TxHead %= uartUART3_TXBUF_SIZE;
            }
        #endif

        #if uartUART4_ENABLED
            if(uart == &Uart4)
            {
                Uart4TxBuffer[uart->TxHead++] = *buf++;
                uart->TxHead %= uartUART4_TXBUF_SIZE;
            }
        #endif

        #if uartUART5_ENABLED
            if(uart == &Uart5)
            {
                Uart5TxBuffer[uart->TxHead++] = *buf++;
                uart->TxHead %= uartUART5_TXBUF_SIZE;
            }
        #endif
        
    }

    if(uart->TxBusy == 0)
    {
        uart->TxBusy = 1;
        #if uartUART2_ENABLED
        {
            if(uart == &Uart2)
            {
                #if uartUART2_485_ENABLED
                {
                    TR4 = 1; 
                }
                #endif
                TI0 = 1;  
            }
        }
        #endif

        #if uartUART3_ENABLED
        {
            if(uart == &Uart3)
            {
                SCON1 |= 0x02;  
            }
        }
        #endif

        #if uartUART4_ENABLED
        {
            if(uart == &Uart4)
            {
                #if uartUART4_485_ENABLED
                {
                    TR4 = 1; 
                }
                #endif
                SCON2T |= 0x01; 
            }
        }
        #endif

        #if uartUART5_ENABLED
        {
            if(uart == &Uart5)
            {
                #if uartUART5_485_ENABLED
                {
                    TR5 = 1; 
                }
                #endif
                SCON3T |= 0x01; 
            }
        }
        #endif

    }
}



uint8_t prvDwin8283CrcCheck(uint8_t* frame,uint16_t len,uint16_t *CrcFlag)
{
    uint16_t crc16,min_frame_len;
    if(len < 6 || len != (uint16_t)frame[2] + 3) return 0;
    if(frame[3] == 0x83)
    {
        min_frame_len = 7;
    }else if(frame[3] == 0x82)
    {
        min_frame_len = 6;
    }else return 0;
    read_dgus_vp(sysDGUS_SYSTEM_CONFIG,(uint8_t*)CrcFlag,1);
    *CrcFlag = *CrcFlag & 0x0080;
    if(*CrcFlag != 0)
    {
        min_frame_len += 2; 
        if(len < min_frame_len) return 0;
        crc16 = crc_16(&frame[3],frame[2] - 2);
        if(crc16 != ((frame[frame[2] + 2] << 8) | frame[frame[2] + 1]))
        {
            return 0; 
        }
    }
    if(len < min_frame_len)
    {
        return 0; 
    }else
    {
        if(frame[3] == 0x83 && len != min_frame_len) return 0;
        if(frame[3] == 0x82 && (len == min_frame_len || ((len - min_frame_len) & 1))) return 0;
        return 1;
    }
}

/**
 * @brief 标准Dwin8283协议处理函数
 * @details 处理接收到的Dwin8283协议帧，支持读写DGUS变量指针操作
 * @param[in] uart UART通信接口指针
 * @param[in,out] frame 协议帧数据缓冲区指针，会被修改用于响应
 * @param[in] len 帧数据长度
 * @return 无
 * @note 支持0x82写命令和0x83读命令
 * @note 自动处理CRC校验和响应帧生成
 * @note 0x82命令可选择是否返回确认帧
 */
static void UartStandardDwin8283Protocal(UART_TYPE *uart,uint8_t *frame, uint16_t len)
{
    uint16_t i=0,CrcFlag = 0,CrcResult = 0;
    uint8_t send_return_frame[256];
    if(frame[0] == 0x5a && frame[1] == 0xa5 && frame[3] == 0x82)
    {
        if(prvDwin8283CrcCheck(frame,len,&CrcFlag) == 0)
        {
            return;
        }
        if(len < frame[2] + 3) 
        {
            return; 
        }
        if(CrcFlag != 0)
        {
            frame[2] -= 2;
        }
        write_dgus_vp((frame[4] << 8) | frame[5], &frame[6], (frame[2] - 3) >> 1);
        #if uartUART_82CMD_RETURN
        i=0;
        send_return_frame[i++] = 0x5a;
        send_return_frame[i++] = 0xa5;
        send_return_frame[i++] = 0x03;
        send_return_frame[i++] = 0x82;  
        send_return_frame[i++] = 0x4f;
        send_return_frame[i++] = 0x4b;
        if(CrcFlag != 0)
        {
            send_return_frame[2] = 5;
            CrcResult = crc_16(&send_return_frame[3], 3);
            send_return_frame[i++] = (uint8_t)CrcResult;
            send_return_frame[i++] = CrcResult >> 8;
        }
        UartSendData(uart, send_return_frame, i);
        #endif /* uartUART_82CMD_RETURN */
    }else if(frame[0] == 0x5a && frame[1] == 0xa5 && frame[3] == 0x83)
    {      
        if(prvDwin8283CrcCheck(frame,len,&CrcFlag) == 0)
        {
            return;
        }
        if(len < frame[2] + 3) 
        {
            return; 
        }
        /* Keep the complete reply within the 256-byte TX ring (one slot free). */
        if(frame[6] == 0 || frame[6] > (CrcFlag ? 123 : 124)) return;
        read_dgus_vp((frame[4] << 8) | frame[5], &send_return_frame[7], frame[6] >> 0);
        i=0;
        send_return_frame[i++] = 0x5a;  
        send_return_frame[i++] = 0xa5;
        send_return_frame[i++] = (frame[6] << 1) + 4;
        send_return_frame[i++] = 0x83;
        send_return_frame[i++] = frame[4];
        send_return_frame[i++] = frame[5];
        send_return_frame[i++] = frame[6];
        if(CrcFlag != 0)
        {
            send_return_frame[2] += 2;
            CrcResult = crc_16(&send_return_frame[3], send_return_frame[2] - 2);
            send_return_frame[send_return_frame[2]+1] = (uint8_t)CrcResult;
            send_return_frame[send_return_frame[2]+2] = CrcResult >> 8;
        }
        UartSendData(uart, send_return_frame, (send_return_frame[2]+3));
    }
}


#if sysBEAUTY_MODE_ENABLED || sysN5CAMERA_MODE_ENABLED || sysADVERTISE_MODE_ENABLED
/* Retain DGUS fragments as well as FB/BOOT controls. */
static uint8_t xdata r11_partial[258];
static uint16_t r11_partial_size;
#endif

#if uartUART2_ENABLED
/* UART2 retains DGUS fragments independently of the customer board on UART4. */
static void Uart2ProtocolFeed(uint8_t *bytes, uint16_t count)
{
    uint16_t n, length, drop, j;
    uint32_t now;
    uint8_t enabled = ET0;
    ET0 = 0; now = GetSysTick(); ET0 = enabled;
    if(now - uart2_last_rx >= 500UL) uart2_partial_size = 0;
    uart2_last_rx = now;
    for(n = 0; n < count; ++n) {
        if(uart2_partial_size == sizeof(uart2_partial)) uart2_partial_size = 0;
        uart2_partial[uart2_partial_size++] = bytes[n];
        while(uart2_partial_size) {
            drop = 1;
            if(uart2_partial[0] == 0x5A) {
                if(uart2_partial_size < 2) break;
                if(uart2_partial[1] == 0xA5) {
                    if(uart2_partial_size < 4) break;
                    length = (uint16_t)uart2_partial[2] + 3;
                    if(length >= 6 && (uart2_partial[3] == 0x82 || uart2_partial[3] == 0x83)) {
                        if(uart2_partial_size < length) break;
                        UartStandardDwin8283Protocal(&Uart2, uart2_partial, length);
                        drop = length;
                    }
                }
            }
            for(j = drop; j < uart2_partial_size; ++j) uart2_partial[j - drop] = uart2_partial[j];
            uart2_partial_size -= drop;
        }
    }
}
#endif

#if uartUART4_ENABLED
/* Dispatch complete frames: Modbus payload bytes must never become VP writes. */
static void Uart4ProtocolFeed(uint8_t *bytes, uint16_t count)
{
    uint16_t n, length, drop, j, crc;
    uint32_t now;
    uint8_t enabled = ET0;
    ET0 = 0; now = GetSysTick(); ET0 = enabled;
    if(now - uart4_last_rx >= 500UL) uart4_partial_size = 0;
    uart4_last_rx = now;
    for(n = 0; n < count; ++n) {
        if(uart4_partial_size == sizeof(uart4_partial)) uart4_partial_size = 0;
        uart4_partial[uart4_partial_size++] = bytes[n];
        while(uart4_partial_size) {
            drop = 1;
            if(uart4_partial[0] == 0x5A) {
                if(uart4_partial_size < 2) break;
                if(uart4_partial[1] == 0xA5) {
                    if(uart4_partial_size < 4) break;
                    length = (uint16_t)uart4_partial[2] + 3;
                    if(length >= 6 && (uart4_partial[3] == 0x82 || uart4_partial[3] == 0x83)) {
                        if(uart4_partial_size < length) break;
                        UartStandardDwin8283Protocal(&Uart4, uart4_partial, length);
                        drop = length;
                    }
                }
            } else if(uart4_partial[0] == 1) {
                if(uart4_partial_size < 2) break;
                length = 0;
                if(uart4_partial[1] == 3) {
                    if(uart4_partial_size < 3) break;
                    if(uart4_partial[2] && uart4_partial[2] <= 59 && !(uart4_partial[2] & 1))
                        length = uart4_partial[2] + 5;
                } else if(uart4_partial[1] == 6) length = 8;
                else if(uart4_partial[1] == 0x83 || uart4_partial[1] == 0x86) length = 5;
                if(length) {
                    if(uart4_partial_size < length) break;
                    crc = crc_16(uart4_partial, length - 2);
                    if(uart4_partial[length - 2] == (uint8_t)crc && uart4_partial[length - 1] == (uint8_t)(crc >> 8)) {
                        FwProtocolFeed(uart4_partial, length);
                    }
                    /* Even a corrupt RTU payload is not a DGUS command. */
                    drop = length;
                }
            }
            for(j = drop; j < uart4_partial_size; ++j) uart4_partial[j - drop] = uart4_partial[j];
            uart4_partial_size -= drop;
        }
    }
}
#endif

void UartReadFrame(UART_TYPE *uart)
{
    uint8_t frame[uartUART_COMMON_FRAME_SIZE];
    uint16_t i,rx_head_bak,one_frame_len,total_frame_len;
    if(uart->RxFlag == UART_NON_REC)
        return;
    if(uart->RxTimeout == 0)
    {
        rx_head_bak = uart->RxHead;
        uart->RxFlag = UART_NON_REC;
        /**
         * @note:不再关闭中断，改为静态变量进行备份
         */
        i=0;
        #if sysBEAUTY_MODE_ENABLED || sysN5CAMERA_MODE_ENABLED || sysADVERTISE_MODE_ENABLED
        if(uart == &Uart_R11) {
            i = r11_partial_size;
            memcpy(frame, r11_partial, i);
            r11_partial_size = 0;
        }
        #endif
        while(rx_head_bak != uart->RxTail)
        {
            if(i == sizeof(frame)) { uart->RxFlag = UART_RECING; break; }
            #if uartUART2_ENABLED
            if(uart == &Uart2)
            {
                frame[i++] = Uart2RxBuffer[uart->RxTail++];
                uart->RxTail %= uartUART2_RXBUF_SIZE;
            }
            #endif /* uartUART2_ENABLED */

            #if uartUART3_ENABLED
            if(uart == &Uart3)
            {
                frame[i++] = Uart3RxBuffer[uart->RxTail++];
                uart->RxTail %= uartUART3_RXBUF_SIZE;
            }
            #endif /* uartUART3_ENABLED */

            #if uartUART4_ENABLED
            if(uart == &Uart4)
            {
                frame[i++] = Uart4RxBuffer[uart->RxTail++];
                uart->RxTail %= uartUART4_RXBUF_SIZE;
            }
            #endif /* uartUART4_ENABLED */
            #if uartUART5_ENABLED
            if(uart == &Uart5)
            {
                frame[i++] = Uart5RxBuffer[uart->RxTail++];
                uart->RxTail %= uartUART5_RXBUF_SIZE;
            }
            #endif /* uartUART5_ENABLED */
        }   
        #if uartUART2_ENABLED
        if(uart == &Uart2)
        {
            Uart2ProtocolFeed(frame, i);
            return;
        }
        #endif
        #if uartUART4_ENABLED
        if(uart == &Uart4)
        {
            Uart4ProtocolFeed(frame, i);
            return;
        }
        #endif
        total_frame_len = i;
        while(1)
        {
            if(i < 4) break;
            if(frame[total_frame_len - i] == 0x5a && frame[total_frame_len - i + 1] == 0xa5)
            {
                one_frame_len = frame[total_frame_len - i + 2] + 3;
                if(i < one_frame_len)
                {
                    break;
                }
                if(!AppOtaControl(uart, &frame[total_frame_len - i], one_frame_len))
                    UartStandardDwin8283Protocal(uart, &frame[total_frame_len - i], one_frame_len);
                #if sysBEAUTY_MODE_ENABLED
                UartR11UserBeautyProtocol(uart, &frame[total_frame_len - i], one_frame_len);
                #endif /* sysBEAUTY_MODE_ENABLED */
                #if sysN5CAMERA_MODE_ENABLED
                UartR11UserN5CameraProtocol(uart, &frame[total_frame_len - i], one_frame_len);
                #endif /* sysN5CAMERA_MODE_ENABLED */
                i -= one_frame_len;
            }else if(frame[total_frame_len - i] == 0xaa && frame[total_frame_len - i + 1] == 0x55)
            {
                one_frame_len = (frame[total_frame_len - i + 2] << 8 | frame[total_frame_len - i + 3]) + 4;
                if(one_frame_len < 5 || one_frame_len > sizeof(frame)) { --i; continue; }
                if(i < one_frame_len)
                {
                    break;
                }
                FwUsbProtocol(uart, &frame[total_frame_len - i], one_frame_len);
                #if R11_WIFI_ENABLED
                UartR11UserWifiProtocol(uart, &frame[total_frame_len - i], one_frame_len);
                #endif /* R11_WIFI_ENABLED */
                #if sysBEAUTY_MODE_ENABLED
                UartR11UserVideoProtocol(uart, &frame[total_frame_len - i], one_frame_len);
                UartR11UserBeautyProtocol(uart, &frame[total_frame_len - i], one_frame_len);
                #endif /* sysBEAUTY_MODE_ENABLED */
                #if sysN5CAMERA_MODE_ENABLED
                UartR11UserVideoProtocol(uart, &frame[total_frame_len - i], one_frame_len);
                UartR11UserN5CameraProtocol(uart, &frame[total_frame_len - i], one_frame_len);
                #endif /* sysN5CAMERA_MODE_ENABLED */
                #if sysADVERTISE_MODE_ENABLED
                UartR11UserVideoProtocol(uart, &frame[total_frame_len - i], one_frame_len);
                UartR11UserAdvertiseProtocol(uart, &frame[total_frame_len - i], one_frame_len);
                #endif /* sysADVERTISE_MODE_ENABLED */
                i -= one_frame_len;
            }else if(frame[total_frame_len - i] == 0xaa && frame[total_frame_len - i + 1] == 0xCC)
            {
                one_frame_len = (frame[total_frame_len - i + 2] << 8 | frame[total_frame_len - i + 3]) + 4;
                if(i < one_frame_len)
                {
                    break;
                }
                #if sysBEAUTY_MODE_ENABLED
                UartR11UserBeautyProtocol(uart, &frame[total_frame_len - i], one_frame_len);
                #endif /* sysBEAUTY_MODE_ENABLED */
                i -= one_frame_len;
            }
            #if uartMODBUS_PROTOCOL_ENABLED
            else if(frame[total_frame_len - i] == modbusSLAVE_ADDRESS)
            {
                one_frame_len = frame[total_frame_len - i + 2] + 5;
                if(i < one_frame_len)
                {
                    break;
                }
                UartStandardModbusRTUProtocal(uart, &frame[total_frame_len - i], one_frame_len);
                i -= one_frame_len;
            }
            #endif /* uartMODBUS_PROTOCOL_ENABLED */
            #if uartTA_PROTOCOL_ENABLED
            else if(frame[total_frame_len - i] == 0xAA)
            {

                if(i<6)
                {
                    break;
                }
                for(one_frame_len = 0;one_frame_len<(i-3);one_frame_len++)
                {
                    if(frame[total_frame_len - i + one_frame_len] == 0xcc && frame[total_frame_len - i + one_frame_len + 1] == 0x33
                    && frame[total_frame_len - i + one_frame_len + 2] == 0xc3 && frame[total_frame_len - i + one_frame_len + 3] == 0x3c)
                    {
                        break;
                    }
                }
                if(one_frame_len == (i-3))
                {
                    break;
                }
                /* 加上帧尾 cc 33 c3 3c*/
                UartStandardTAProtocol(uart, &frame[total_frame_len - i], one_frame_len+4);
                i -= (one_frame_len+4);
            }
            #endif /* uartTA_PROTOCOL_ENABLED */
            else
            {
                if(i>0)
                {
                    i--;
                }else{
                    break;
                }
            }
        }
        #if sysBEAUTY_MODE_ENABLED || sysN5CAMERA_MODE_ENABLED || sysADVERTISE_MODE_ENABLED
        if(uart == &Uart_R11 && i && i < sizeof(r11_partial)) {
            memcpy(r11_partial, &frame[total_frame_len - i], i);
            r11_partial_size = i;
        }
        #endif
    }
}


void UartProtocalHandleTask(void)
{
    UartReadFrame(&Uart2);
    UartReadFrame(&Uart4);
    #if sysBEAUTY_MODE_ENABLED || sysN5CAMERA_MODE_ENABLED || sysADVERTISE_MODE_ENABLED
    UartReadFrame(&Uart_R11);
    #endif /* sysBEAUTY_MODE_ENABLED || sysN5CAMERA_MODE_ENABLED || sysADVERTISE_MODE_ENABLED */
    #if uartTA_PROTOCOL_ENABLED
    TAProtocolUpload(&Uart2);
    #endif /* uartTA_PROTOCOL_ENABLED */
}


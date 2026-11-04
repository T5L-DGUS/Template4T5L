
/**
 * @file    uart.c
 * @brief   UART通信接口驱动程序实现文件
 * @details 本文件实现了多路UART通信接口的完整功能，包括硬件初始化、
 *          中断处理、数据收发和多协议支持
 * @author  yangming
 * @version 1.0.0
 */

#include "uart.h"
#include <string.h>
#include "sys.h"
#include "t5l_stc.h"
#if uartMODBUS_PROTOCOL_ENABLED
#include "modbus.h"
#endif /* uartMODBUS_PROTOCOL_ENABLED */

#if uartTA_PROTOCOL_ENABLED
#include "TA_protocal.h"
#endif /* uartTA_PROTOCOL_ENABLED */

#if v851PROTOCOL_ENABLED
#include "v851_protocol.h"
#include "v851_wifi.h"
#endif /* v851PROTOCOL_ENABLED */


#if uartUART2_ENABLED
UART_TYPE Uart2;
uint8_t Uart2TxBuffer[uartUART2_TXBUF_SIZE];
uint8_t Uart2RxBuffer[uartUART2_RXBUF_SIZE];
void Uart2Init(const uint32_t bdt)
{
    uint32_t baud;
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

    #if sys2K_RATIO
    PCON &= ~0x80;
    #endif /* sys2K_RATIO */
    
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

#if uartUART4_ENABLED
UART_TYPE Uart4;
uint8_t Uart4TxBuffer[uartUART4_TXBUF_SIZE];
uint8_t Uart4RxBuffer[uartUART4_RXBUF_SIZE];
void Uart4Init(const uint32_t bdt)
{
    uint32_t baud;
    memset((uint8_t *)&Uart4, 0, sizeof(UART_TYPE));
    memset((uint8_t *)Uart4TxBuffer, 0, uartUART4_TXBUF_SIZE);
    memset((uint8_t *)Uart4RxBuffer, 0, uartUART4_RXBUF_SIZE);

    P0MDOUT |= 0x01;
    #if uartUART4_485_ENABLED
    TR4 = 0;
    #endif /* uartUART4_485_ENABLED */

    #if CPU_TYPE==T5F0
    MUX_SEL1 |= 0x10; 
	P0MDOUT |= 0x01; 
	P0MDOUT &= 0xFD; 
    #endif /* CPU_TYPE==T5F0*/
    SCON2T=0x80;
    SCON2R=0x80;
    
    #if sys2K_RATIO
    baud = (uint16_t)(sysFCLK/16/bdt);
    #else
    baud = (uint16_t)(sysFCLK/8/bdt);
    #endif /* sys2K_RATIO */
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
    uint16_t next_head;

    if((SCON2R&0x01) == 0x01)
    {
        next_head = (uint16_t)(Uart4.RxHead + 1U);
        next_head %= uartUART4_RXBUF_SIZE;
        if(next_head == Uart4.RxTail)
        {
            Uart4.RxOverflow = 1U;
        }
        else
        {
            Uart4RxBuffer[Uart4.RxHead] = SBUF2_RX;
            Uart4.RxHead = next_head;
        }
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

void UartInit(void)
{
    #if uartUART2_ENABLED
    Uart2Init(uartUART2_BAUDRATE);
    #endif /* uartUART2_ENABLED */

    #if uartUART4_ENABLED
    Uart4Init(uartUART4_BAUDRATE);
    #endif /* uartUART4_ENABLED */

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

        #if uartUART4_ENABLED
            if(uart == &Uart4)
            {
                Uart4TxBuffer[uart->TxHead++] = *buf++;
                uart->TxHead %= uartUART4_TXBUF_SIZE;
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

    }
}



uint8_t prvDwin8283CrcCheck(uint8_t* frame,uint16_t len,uint16_t *CrcFlag)
{
    uint16_t crc16,min_frame_len;
    if((frame == NULL) || (CrcFlag == NULL) || (len < 4U))
    {
        return 0;
    }

    *CrcFlag = 0;
    if(frame[3] == 0x83)
    {
        if(frame[2] < 4U)
        {
            return 0;
        }
        min_frame_len = 7;
    }else if(frame[3] == 0x82)
    {
        if(frame[2] < 3U)
        {
            return 0;
        }
        min_frame_len = 6;
    }else
    {
        return 0;
    }

    if((len < min_frame_len) || (len < ((uint16_t)frame[2] + 3U)))
    {
        return 0;
    }

    read_dgus_vp(sysDGUS_SYSTEM_CONFIG,(uint8_t*)CrcFlag,1);
    *CrcFlag = *CrcFlag & 0x0080;
    if(*CrcFlag != 0)
    {
        min_frame_len += 2; 
        if(len < min_frame_len)
        {
            return 0;
        }
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
        if(frame[2] < 3U)
        {
            return;
        }

        if(frame[6]==0x4F&&frame[7]==0x4B){
            T5l_Stc_UartRxProcess(((uint32_t)frame[4] << 8) | frame[5]);
            return;
        }
        write_dgus_vp(((uint32_t)frame[4] << 8) | frame[5], &frame[6], (frame[2] - 3) >> 1);
        
        STC_ReporData_Procese(((uint32_t)frame[4] << 8) | frame[5]);

       
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
            send_return_frame[2] += 2;
            CrcResult = crc_16(&send_return_frame[3], send_return_frame[2] - 2);
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
        if(frame[6] > ((sizeof(send_return_frame) - 9U) / 2U))
        {
            return;
        }
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

void UartReadFrame(UART_TYPE *uart)
{
    static uint8_t xdata frame[uartUART_COMMON_FRAME_SIZE];
    uint16_t i,rx_head_bak,one_frame_len,total_frame_len,frame_offset;
    #if v851PROTOCOL_ENABLED
    uint16_t body_len;
    uint8_t command;
    #endif /* v851PROTOCOL_ENABLED */
    if(uart->RxFlag == UART_NON_REC)
        return;
    if(uart->RxTimeout == 0)
    {
        if(uart->RxOverflow != 0U)
        {
            uart->RxTail = uart->RxHead;
            uart->RxOverflow = 0U;
            uart->RxFlag = UART_NON_REC;
            return;
        }
        rx_head_bak = uart->RxHead;
        uart->RxFlag = UART_NON_REC;
        /**
         * @note:不再关闭中断，改为静态变量进行备份
         */
        i=0;
        while(rx_head_bak != uart->RxTail)
        {
            #if uartUART2_ENABLED
            if(uart == &Uart2)
            {
                frame[i++] = Uart2RxBuffer[uart->RxTail++];
                uart->RxTail %= uartUART2_RXBUF_SIZE;
            }
            #endif /* uartUART2_ENABLED */

            #if uartUART4_ENABLED
            if(uart == &Uart4)
            {
                frame[i++] = Uart4RxBuffer[uart->RxTail++];
                uart->RxTail %= uartUART4_RXBUF_SIZE;
            }
            #endif /* uartUART4_ENABLED */
        }   
        total_frame_len = i;

        while(i > 0)
        {
            frame_offset = total_frame_len - i;
            if(i < 2U)
            {
                break;
            }

            if(frame[frame_offset] == 0x5a && frame[frame_offset + 1] == 0xa5)
            {
                if(i < 3U)
                {
                    break;
                }
                one_frame_len = frame[frame_offset + 2] + 3;
                if(i < one_frame_len)
                {
                    break;
                }
                UartStandardDwin8283Protocal(uart, &frame[frame_offset], one_frame_len);
                i -= one_frame_len;
            }
            else if(frame[frame_offset] == 0xaa && frame[frame_offset + 1] == 0x55)
            {
                if(i < 4U)
                {
                    break;
                }

                #if v851PROTOCOL_ENABLED
                if(uart == &Uart4)
                {
                    if(i < 5U)
                    {
                        break;
                    }
                    body_len = ((uint16_t)frame[frame_offset + 2U] << 8) |
                               frame[frame_offset + 3U];
                    command = frame[frame_offset + 4U];
                    if((command == V851_TLV_CMD_PROPERTY) ||
                       (command == V851_TLV_CMD_FACTORY) ||
                       (command == V851_TLV_CMD_BOOTSTRAP_RESULT) ||
                       (command == V851_TLV_CMD_BOOTSTRAP_RESULT_COMPAT))
                    {
                        /* V851 RX length includes the command byte. */
                        if((body_len < (V851_TLV_SEGMENT_HEADER_SIZE + 1U)) ||
                           (body_len > (V851_TLV_FRAME_MAX -
                                        V851_TLV_RX_LENGTH_BASE_SIZE)))
                        {
                            i--;
                            continue;
                        }
                        one_frame_len =
                            (uint16_t)(body_len +
                                       V851_TLV_RX_LENGTH_BASE_SIZE);
                    }
                    else if((command == V851_WIFI_CMD_SCAN) ||
                            (command == V851_WIFI_CMD_CONNECT) ||
                            (command == V851_WIFI_CMD_STATUS))
                    {
                        if((body_len < 1U) ||
                           (body_len > (V851_TLV_FRAME_MAX - 4U)))
                        {
                            i--;
                            continue;
                        }
                        one_frame_len = (uint16_t)(body_len + 4U);
                    }
                    else
                    {
                        i--;
                        continue;
                    }
                    if(i < one_frame_len)
                    {
                        break;
                    }
                    if((command == V851_TLV_CMD_PROPERTY) ||
                       (command == V851_TLV_CMD_FACTORY) ||
                       (command == V851_TLV_CMD_BOOTSTRAP_RESULT) ||
                       (command == V851_TLV_CMD_BOOTSTRAP_RESULT_COMPAT))
                    {
                        V851ProtocolReceiveFrame(&frame[frame_offset],
                                                 one_frame_len);
                    }
                    else
                    {
                        V851WifiReceiveFrame(&frame[frame_offset],
                                             one_frame_len);
                    }
                    i -= one_frame_len;
                }
                else
                #endif /* v851PROTOCOL_ENABLED */
                {
                    i--;
                }
            }
            #if uartMODBUS_PROTOCOL_ENABLED
            else if(frame[frame_offset] == modbusSLAVE_ADDRESS)
            {
                if(i < 3U)
                {
                    break;
                }
                one_frame_len = frame[frame_offset + 2] + 5;
                if(i < one_frame_len)
                {
                    break;
                }
                UartStandardModbusRTUProtocal(uart, &frame[frame_offset], one_frame_len);
                i -= one_frame_len;
            }
            #endif /* uartMODBUS_PROTOCOL_ENABLED */
            #if uartTA_PROTOCOL_ENABLED
            else if(frame[frame_offset] == 0xAA)
            {

                if(i<6)
                {
                    break;
                }
                for(one_frame_len = 0;one_frame_len<(i-3);one_frame_len++)
                {
                    if(frame[frame_offset + one_frame_len] == 0xcc && frame[frame_offset + one_frame_len + 1] == 0x33
                    && frame[frame_offset + one_frame_len + 2] == 0xc3 && frame[frame_offset + one_frame_len + 3] == 0x3c)
                    {
                        break;
                    }
                }
                if(one_frame_len == (i-3))
                {
                    break;
                }
                /* 加上帧尾 cc 33 c3 3c*/
                UartStandardTAProtocol(uart, &frame[frame_offset], one_frame_len+4);
                i -= (one_frame_len+4);
            }
            #endif /* uartTA_PROTOCOL_ENABLED */
            else
            {
                i--;
            }
        }
    }
}


void UartProtocalHandleTask(void)
{
    #if uartUART2_ENABLED
    UartReadFrame(&Uart2);
    #endif /* uartUART2_ENABLED */
    #if uartUART4_ENABLED
    UartReadFrame(&Uart4);
    #endif /* uartUART4_ENABLED */
    #if uartTA_PROTOCOL_ENABLED
    TAProtocolUpload(&Uart2);
    #endif /* uartTA_PROTOCOL_ENABLED */
}


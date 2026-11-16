/*
智能舱鸟舱产品说明书

1.产品功能参数介绍

排风：
    A.app 小程序 控制屏可控制风扇1-6风速调节
      可控制开关时间（定时0.5-8小时后自动开启通风时长10-15分钟）


进风：
    A.app 小程序 控制屏可控制风扇1-4档风速可调节（预留后期可以用负离子替代）
    C.自动开启特殊情况
        1.	开启加热，温度传感器反馈有温差，自动开启加热强风。
        2.	负离子，等离子开启自动开启小风（一档）
        3.	开启加热时，负离子，等离子以加热的进风，优先考虑
        4.	开启加热自动开启，加热时进风不可控制关闭

uvb：（单独控制或单独连接端子）
    A.app 小程序 控制屏可控制UVB 1-4档亮度调节、(1档：0-18/2档：18-25/3档：25-64/4档：64-87.5按照弗格森四区间控制）时间控制（2小时/4小时/6小时/8小时）
   


雾化：雾化片+雾化液检测
    A.	app 小程序 控制屏可控制开关雾化片，能够根据舱内湿度传感器需要加湿量调节，水位传感器检测雾化液，加液提醒。
    B.	单次运行0.5/1/2小时，间隔时间为2/4/8/12小时
                    

照明：（单独控制或单独连接端子）
    A.app 小程序 控制屏可控制灯光1-4档亮度调节

加热模块:
    A.app 小程序 控制屏可控制温度20-35度(开启时快速升温达到指定温度，
     传感器反馈温度达到了停止加热，温度没达到差-5°正常加热
    （进风风扇和加热模块同步开起，关闭时进风风扇比加热模块晚5秒关闭）
 

紫外线消杀（uvc）：（单独控制或单独连接端子）
    A.	app 小程序 控制屏可控制开关时间（15分钟/30分钟/1小时）
红外线：（单独控制或单独连接端子）
    A.光线较暗自动开启/光线强自动关闭（开启后 摄像头LRcat模式自动开启）
    

负离子：
    A.app 小程序 控制屏可控制开关(开启时进风自动开启）
    

等离子：
    A.app 小程序 控制屏可控制开关(开启时进风自动开启）
  


电磁锁：
    A.只可控制屏输入电磁锁开启（后台可一键同步开启多台电磁门）
 


*/
#include "sys.h"
#include "t5l_stc.h"
#include "uart.h"
#include "timer.h"
#include "string.h"
#if v851PROTOCOL_ENABLED
#include "v851_protocol.h"
#endif
DeviceCtrl G_Device_Ctrl;
DeviceCtrl G_Back_Device_Ctrl;
static uint8_t t5l_factory_reset_pending;
/* STC 复位重同步：收到 0x611D 后逐步重发已开启设备的参数 */
static uint8_t stc_resync_pending;
static uint8_t stc_resync_step;

#define WAIT_ACK_TIMEOUT 1000 //等待应答超时时间
#define WAIT_ACK_ERROR_CNT 2  //等待应答错误次数

uint8_t g_showsta = 0;
//密码输入
uint8_t input_1on[8];
uint8_t input_1off[8];
uint8_t g_page_id = 0;
uint8_t page_cnt1 = 0;

uint8_t input_2on[8];
uint8_t input_2off[8];
uint8_t page_cnt2 = 0;

uint8_t input_3on[8];
uint8_t input_3off[8];
uint8_t page_cnt3 = 0;

uint8_t input_sel = 0;

/**
 * @brief  modbus主机状态
 */
typedef enum
{
    STATE_IDLE = 0X00,      // 主机空闲状态
    STATE_WAIT_ACK,         // 等待接收
    STATE_WAIT_ACK_TIMEOUT, // 等待接收超时
    STATE_WAIT_ACK_FAILURE, // 等待接收错误
    STATE_WAIT_ACK_SUCCESS, // 等待接收成功
    STATE_EXEC,             // 回调处理

} Uart_State;

typedef struct _data_node
{
    uint8_t buf[20];
    uint8_t len;
} DATA_NODE;

typedef struct _queue_node
{
    DATA_NODE Data[16];
    uint8_t head;
    uint8_t tail;
    uint8_t last_head;
    uint8_t err_cnt;
    Uart_State sta;
    uint16_t wait_ack_time; //等待应答时间
    uint16_t showsuccse_time_ms;
    uint8_t showsuccse_sta;
} QUEUE_NODE;

QUEUE_NODE G_Queue; //环形队列
void nor_flash_write(uint32_t addr, uint8_t* buf, uint16_t len);
void Dev_Init(void)
{
    memset(&G_Device_Ctrl, 0, sizeof(G_Device_Ctrl));
    G_Device_Ctrl.dev_enable=1;
}

void Set_Input(uint8_t pageid, uint8_t sel)
{
    g_page_id = pageid;
    input_sel = sel;
    if (sel == 1)
    {
        memset(input_1off, 0xFF, 8);
        memset(input_1on, 0xFF, 8);

        input_1on[0] = 0x7C;
        page_cnt1 = 0;
    }
    else if (sel == 2)
    {
        memset(input_2off, 0xFF, 8);
        memset(input_2on, 0xFF, 8);

        input_2on[0] = 0x7C;
        page_cnt2 = 0;
    }
    else if (sel == 3)
    {
        memset(input_3off, 0xFF, 8);
        memset(input_3on, 0xFF, 8);

        input_3on[0] = 0x7C;
        page_cnt3 = 0;
    }
    switch (pageid)
    {
    case 4:
        write_dgus_vp(0x5380, (uint8_t *)&input_1on, 3);
        break;
    case 14:

        break;
    case 15:
        write_dgus_vp(0x5360, (uint8_t *)&input_1on, 3);

        break;
    case 28:
        write_dgus_vp(0x5366, (uint8_t *)&input_1on, 3);

        break;
    case 26:
        if (input_sel == 1)
        {
            write_dgus_vp(PASWD26_VP1, (uint8_t *)&input_1on, 3);
            write_dgus_vp(PASWD26_VP2, (uint8_t *)&input_2off, 3);
            write_dgus_vp(PASWD26_VP3, (uint8_t *)&input_3off, 3);
        }
        else if (input_sel == 2)
        {
            write_dgus_vp(PASWD26_VP1, (uint8_t *)&input_1off, 3);
            write_dgus_vp(PASWD26_VP2, (uint8_t *)&input_2on, 3);
            write_dgus_vp(PASWD26_VP3, (uint8_t *)&input_3off, 3);
        }
        else if (input_sel == 3)
        {
            write_dgus_vp(PASWD26_VP1, (uint8_t *)&input_1off, 3);
            write_dgus_vp(PASWD26_VP2, (uint8_t *)&input_2off, 3);
            write_dgus_vp(PASWD26_VP3, (uint8_t *)&input_3on, 3);
        }
        break;
    default:
        break;
    }
}

void Clear_Input(uint8_t pageid)
{
    switch (pageid)
    {
    case 4:
    case 14:
    case 15:
        memset(input_1off, 0xFF, 8);
        memset(input_1on, 0xFF, 8);

        input_1on[0] = 0x7C;

        g_page_id = pageid;
        page_cnt1 = 0;
        break;
        break;
    default:
        break;
    }
}

/* Keep G_Device_Ctrl and the mapped DGUS words in sync. */
static uint8_t T5lStcSyncMappedControlInternal(
    T5lStcMappedControl control,
    uint8_t field_mask,
    uint16_t enabled,
    uint16_t secondary,
    uint16_t tertiary,
    uint8_t sync_command_vp)
{
    uint16_t report[3];
    uint16_t command[3];
    uint32_t report_vp;
    uint32_t command_vp;
    uint8_t valid_mask;
    uint8_t report_words;
    uint8_t save_changed;

    if ((control >= T5L_STC_MAPPED_CONTROL_COUNT) ||
        (((field_mask & T5L_STC_MAPPED_FIELD_ENABLED) != 0U) &&
         (enabled > 1U)))
    {
        return 0U;
    }

    valid_mask = T5L_STC_MAPPED_FIELD_ENABLED;
    report_words = 1U;
    save_changed = 0U;
    switch (control)
    {
    case T5L_STC_MAPPED_EXHAUST:
    case T5L_STC_MAPPED_UVB:
    case T5L_STC_MAPPED_HUMIDIFIER:
        valid_mask |= T5L_STC_MAPPED_FIELD_SECONDARY |
                      T5L_STC_MAPPED_FIELD_TERTIARY;
        report_words = 3U;
        break;
    case T5L_STC_MAPPED_LIGHT:
    case T5L_STC_MAPPED_CLIMATE:
    case T5L_STC_MAPPED_INLET_FAN:
        valid_mask |= T5L_STC_MAPPED_FIELD_SECONDARY;
        report_words = 2U;
        break;
    default:
        break;
    }
    field_mask &= valid_mask;
    if (field_mask == 0U)
    {
        return 0U;
    }

    if ((((field_mask & T5L_STC_MAPPED_FIELD_SECONDARY) != 0U) &&
         (((control == T5L_STC_MAPPED_EXHAUST) &&
           ((secondary < 1U) || (secondary > 6U))) ||
          ((control == T5L_STC_MAPPED_LIGHT) &&
           ((secondary < 1U) || (secondary > 4U))) ||
          ((control == T5L_STC_MAPPED_UVB) &&
           ((secondary < 1U) || (secondary > 4U))) ||
          ((control == T5L_STC_MAPPED_CLIMATE) &&
           ((secondary < 20U) || (secondary > 35U))) ||
          ((control == T5L_STC_MAPPED_HUMIDIFIER) &&
           ((secondary < 1U) || (secondary > 4U))) ||
          ((control == T5L_STC_MAPPED_INLET_FAN) &&
           ((secondary < 1U) || (secondary > 4U))))) ||
        (((field_mask & T5L_STC_MAPPED_FIELD_TERTIARY) != 0U) &&
         (((control == T5L_STC_MAPPED_EXHAUST) &&
           ((tertiary < 1U) || (tertiary > 5U))) ||
          ((control == T5L_STC_MAPPED_UVB) &&
           ((tertiary < 1U) || (tertiary > 4U))) ||
          ((control == T5L_STC_MAPPED_HUMIDIFIER) &&
           ((tertiary < 1U) || (tertiary > 3U))))))
    {
        return 0U;
    }

    switch (control)
    {
    case T5L_STC_MAPPED_EXHAUST:
        if ((field_mask & T5L_STC_MAPPED_FIELD_ENABLED) != 0U)
        {
            G_Device_Ctrl.Exhaust.enable = enabled;
        }
        if ((field_mask & T5L_STC_MAPPED_FIELD_SECONDARY) != 0U)
        {
            if (G_Device_Ctrl.Exhaust.speed != secondary)
                save_changed = 1U;
            G_Device_Ctrl.Exhaust.speed = secondary;
            G_Device_Ctrl.Exhaust.target_speed = secondary;
        }
        if ((field_mask & T5L_STC_MAPPED_FIELD_TERTIARY) != 0U)
        {
            if (G_Device_Ctrl.Exhaust.interval_time_h != tertiary)
                save_changed = 1U;
            G_Device_Ctrl.Exhaust.interval_time_h = tertiary;
        }
        report[0] = G_Device_Ctrl.Exhaust.enable;
        report[1] = G_Device_Ctrl.Exhaust.speed;
        report[2] = G_Device_Ctrl.Exhaust.interval_time_h;
        command_vp = OUTWIND_VP;
        report_vp = T5L_STC_REPORT_EXHAUST_VP;
        break;

    case T5L_STC_MAPPED_LIGHT:
        if ((field_mask & T5L_STC_MAPPED_FIELD_ENABLED) != 0U)
        {
            G_Device_Ctrl.Light.enable = enabled;
        }
        if ((field_mask & T5L_STC_MAPPED_FIELD_SECONDARY) != 0U)
        {
            if (G_Device_Ctrl.Light.brightness != secondary)
                save_changed = 1U;
            G_Device_Ctrl.Light.brightness = secondary;
            G_Device_Ctrl.Light.target_brightness = secondary;
        }
        report[0] = G_Device_Ctrl.Light.enable;
        report[1] = G_Device_Ctrl.Light.brightness;
        command_vp = LIGHT_VP;
        report_vp = T5L_STC_REPORT_LIGHT_VP;
        break;

    case T5L_STC_MAPPED_UVB:
        if ((field_mask & T5L_STC_MAPPED_FIELD_ENABLED) != 0U)
        {
            G_Device_Ctrl.UVB.enable = enabled;
        }
        if ((field_mask & T5L_STC_MAPPED_FIELD_SECONDARY) != 0U)
        {
            if (G_Device_Ctrl.UVB.brightness != secondary)
                save_changed = 1U;
            G_Device_Ctrl.UVB.brightness = secondary;
            G_Device_Ctrl.UVB.target_brightness = secondary;
        }
        if ((field_mask & T5L_STC_MAPPED_FIELD_TERTIARY) != 0U)
        {
            if (G_Device_Ctrl.UVB.running_time_h != tertiary)
                save_changed = 1U;
            G_Device_Ctrl.UVB.running_time_h = tertiary;
        }
        report[0] = G_Device_Ctrl.UVB.enable;
        report[1] = G_Device_Ctrl.UVB.brightness;
        report[2] = G_Device_Ctrl.UVB.running_time_h;
        command_vp = UVB_VP;
        report_vp = T5L_STC_REPORT_UVB_VP;
        break;

    case T5L_STC_MAPPED_ANION:
        if ((field_mask & T5L_STC_MAPPED_FIELD_ENABLED) != 0U)
        {
            G_Device_Ctrl.Anion.enable = enabled;
        }
        report[0] = G_Device_Ctrl.Anion.enable;
        command_vp = ANION_VP;
        report_vp = T5L_STC_REPORT_ANION_VP;
        break;

    case T5L_STC_MAPPED_PLASMA:
        if ((field_mask & T5L_STC_MAPPED_FIELD_ENABLED) != 0U)
        {
            G_Device_Ctrl.Plasma.enable = enabled;
        }
        report[0] = G_Device_Ctrl.Plasma.enable;
        command_vp = PLASMA_VP;
        report_vp = T5L_STC_REPORT_PLASMA_VP;
        break;

    case T5L_STC_MAPPED_CLIMATE:
        if ((field_mask & T5L_STC_MAPPED_FIELD_ENABLED) != 0U)
        {
            G_Device_Ctrl.Heater.enable = enabled;
        }
        if ((field_mask & T5L_STC_MAPPED_FIELD_SECONDARY) != 0U)
        {
            if (G_Device_Ctrl.Heater.set_temp != secondary)
                save_changed = 1U;
            G_Device_Ctrl.Heater.set_temp = secondary;
            G_Device_Ctrl.Heater.target_temp = secondary;
        }
        report[0] = G_Device_Ctrl.Heater.enable;
        report[1] = G_Device_Ctrl.Heater.set_temp;
        command_vp = HEATER_VP;
        report_vp = T5L_STC_REPORT_CLIMATE_VP;
        break;

    case T5L_STC_MAPPED_HUMIDIFIER:
        if ((field_mask & T5L_STC_MAPPED_FIELD_ENABLED) != 0U)
        {
            G_Device_Ctrl.Humidifier.enable = enabled;
        }
        if ((field_mask & T5L_STC_MAPPED_FIELD_SECONDARY) != 0U)
        {
            if (G_Device_Ctrl.Humidifier.interval_time_h != secondary)
                save_changed = 1U;
            G_Device_Ctrl.Humidifier.interval_time_h = secondary;
        }
        if ((field_mask & T5L_STC_MAPPED_FIELD_TERTIARY) != 0U)
        {
            if (G_Device_Ctrl.Humidifier.running_time_h != tertiary)
                save_changed = 1U;
            G_Device_Ctrl.Humidifier.running_time_h = tertiary;
        }
        report[0] = G_Device_Ctrl.Humidifier.enable;
        report[1] = G_Device_Ctrl.Humidifier.interval_time_h;
        report[2] = G_Device_Ctrl.Humidifier.running_time_h;
        command_vp = MIST_VP;
        report_vp = T5L_STC_REPORT_HUMIDIFIER_VP;
        break;

    case T5L_STC_MAPPED_INLET_FAN:
        if ((field_mask & T5L_STC_MAPPED_FIELD_ENABLED) != 0U)
        {
            G_Device_Ctrl.Inlet_Fan.enable = enabled;
        }
        if ((field_mask & T5L_STC_MAPPED_FIELD_SECONDARY) != 0U)
        {
            if (G_Device_Ctrl.Inlet_Fan.speed != secondary)
                save_changed = 1U;
            G_Device_Ctrl.Inlet_Fan.speed = secondary;
            G_Device_Ctrl.Inlet_Fan.target_speed = secondary;
        }
        report[0] = G_Device_Ctrl.Inlet_Fan.enable;
        report[1] = G_Device_Ctrl.Inlet_Fan.speed;
        command_vp = INWIND_VP;
        report_vp = T5L_STC_REPORT_INLET_FAN_VP;
        break;

    default:
        return 0U;
    }

    if (sync_command_vp != 0U)
    {
        read_dgus_vp(command_vp, (uint8_t *)command, report_words);
        if ((field_mask & T5L_STC_MAPPED_FIELD_ENABLED) != 0U)
        {
            command[0] = report[0];
        }
        if ((field_mask & T5L_STC_MAPPED_FIELD_SECONDARY) != 0U)
        {
            command[1] = report[1];
        }
        if ((field_mask & T5L_STC_MAPPED_FIELD_TERTIARY) != 0U)
        {
            command[2] = report[2];
        }
        write_dgus_vp(command_vp, (uint8_t *)command, report_words);
    }
    write_dgus_vp(report_vp, (uint8_t *)report, report_words);
    if (save_changed != 0U)
    {
        Start_Once_SaveData();
    }
    return 1U;
}

uint8_t T5lStcSyncMappedControl(T5lStcMappedControl control,
                                uint8_t field_mask,
                                uint16_t enabled,
                                uint16_t secondary,
                                uint16_t tertiary)
{
    return T5lStcSyncMappedControlInternal(control, field_mask, enabled,
                                           secondary, tertiary, 0U);
}

uint8_t T5lStcSyncLocalMappedControl(T5lStcMappedControl control,
                                     uint8_t field_mask,
                                     uint16_t enabled,
                                     uint16_t secondary,
                                     uint16_t tertiary)
{
    return T5lStcSyncMappedControlInternal(control, field_mask, enabled,
                                           secondary, tertiary, 1U);
}

void Queue_Init(void)
{
    G_Queue.head = 0;
    G_Queue.tail = 0;
    G_Queue.err_cnt = 0;
    G_Queue.sta = STATE_IDLE;
    G_Queue.wait_ack_time = 0;
}

void DeRun_Auto(void)
{
    uint16_t addr_VP = 0;
    uint16_t dataChangeLen;
    uint8_t upLoadF00[4];
    uint8_t buf[16];
    read_dgus_vp(0x0f00, upLoadF00, 2);
    if (upLoadF00[0] == 0x5a)
    {
        addr_VP = *(uint16_t *)&upLoadF00[1];
        dataChangeLen = upLoadF00[3];
        *(uint16_t *)&buf[0] = addr_VP;
        // read_dgus_vp(addr_VP, (uint8_t *)&buf[2], dataChangeLen);
        // if (addr_VP == 0x4C0)
        // {
        //     delay_ms(50);
        //     write_dgus_vp(0xE8, "\x5A\x00\x00\03", 2);
        // }

        // UartSendData(&Uart2, buf, (dataChangeLen * 2) + 2);
        upLoadF00[0] = 0;
        upLoadF00[0] = 0;
        write_dgus_vp(0x0f00, (uint8_t *)&upLoadF00, 1);
    }
}
void T5l_Stc_Init(void)
{
    Dev_Init();
    Queue_Init();
    SysCfg_Init();
}

void Queue_Push(uint8_t *buf, uint8_t len)
{
    uint8_t next;
    if (len > 20)
        return;
    /* 队列满（tail 追上 head）时丢弃本条：否则会覆盖正在等待应答的条目，
       Queue_Sta_Poll 用 last_head 重发时就会把错误的数据发给下位机 */
    next = G_Queue.tail + 1;
    if (next >= 16)
        next = 0;
    if (next == G_Queue.head)
        return;
    memcpy(G_Queue.Data[G_Queue.tail].buf, buf, len);
    G_Queue.Data[G_Queue.tail].len = len;
    G_Queue.tail++;
    if (G_Queue.tail >= 16)
    {
        G_Queue.tail = 0;
    }
}

/**
 * @brief 队列出队函数
 * @details 从队列中取出一个元素，通过移动队列头指针实现
 *          队列采用循环队列实现，当头指针到达队列末尾时会回到起始位置
 */
void Queue_Pop_Poll(void)
{
    if (G_Queue.sta != STATE_IDLE)
    {
        return;
    }
    // 检查队列是否为空（头指针和尾指针不相等表示队列不为空）
    if (G_Queue.head != G_Queue.tail)
    {
        G_Queue.err_cnt = 0;
        G_Queue.sta = STATE_WAIT_ACK;
        G_Queue.wait_ack_time = WAIT_ACK_TIMEOUT;
        G_Queue.last_head = G_Queue.head;

        UartSendData(&Uart2, G_Queue.Data[G_Queue.head].buf, G_Queue.Data[G_Queue.head].len);
        // 头指针后移，实现出队操作
        G_Queue.head++;
        // 循环队列处理：当头指针达到队列容量上限时，回到起始位置
        if (G_Queue.head >= 16)
        {
            G_Queue.head = 0;
        }
    }
}

void Queue_Sta_Poll(void)
{
    switch (G_Queue.sta)
    {
    case STATE_WAIT_ACK_TIMEOUT: //超时重传
        G_Queue.err_cnt++;
        if (G_Queue.err_cnt > WAIT_ACK_ERROR_CNT)
        {
            G_Queue.sta = STATE_WAIT_ACK_FAILURE;
            break;
        }

        G_Queue.sta = STATE_WAIT_ACK;
        G_Queue.wait_ack_time = WAIT_ACK_TIMEOUT;
        UartSendData(&Uart2, G_Queue.Data[G_Queue.last_head].buf, G_Queue.Data[G_Queue.last_head].len);
        break;
    case STATE_WAIT_ACK_FAILURE: //无响应判断失败
        G_Queue.sta = STATE_IDLE;
        G_Queue.err_cnt = 0;
        break;
    case STATE_WAIT_ACK_SUCCESS: //成功
        G_Queue.sta = STATE_IDLE;
        G_Queue.err_cnt = 0;
        break;
    default:
        break;
    }

    if (G_Queue.showsuccse_sta == 1)
    {
        G_Queue.showsuccse_sta = 0;
        write_dgus_vp(0x535C, "\x00\x00", 1);
    }
}

//将时间间隔档位转换为秒数 time_tpye 0=间隔时间  1=单次运行时间
//注：排风/雾化/UVB/UVC 的运行与间隔定时任务已全部移除，本函数当前无调用点，
//    保留以便后续恢复定时功能（不调用时不会链接进固件）
uint32_t Get_Interval_Run_Sec(DEV_TYPE dev_type, uint8_t time_tpye, uint16_t th)
{

    uint32_t ts = 0UL;
    if (dev_type < Type_OutWind || dev_type > Type_LOCK)
        return 0; //设备类型错误
    switch (dev_type)
    {
    case Type_OutWind:
        if (time_tpye == 0)
        {

            if (th == 1)
            {
                ts = 30 * 60;
                // ts = 10;
            }
            else if (th == 2)
            {
                ts = 1 * 3600UL;
            }
            else if (th == 3)
            {
                ts = 2 * 3600UL;
            }
            else if (th == 4)
            {
                ts = 4 * 3600UL;
            }
            else if (th == 5)
            {
                ts = 8 * 3600UL;
            }
        }
        break;
    case Type_InWind:

        break;
    case Type_UVC:
        if (time_tpye == 1)
        {
            if (th == 1)
            {
                ts = 15 * 60;
                // ts = 5;
            }
            else if (th == 2)
            {
                ts = 30 * 60;
            }
            else if (th == 3)
            {
                ts = 60 * 60;
            }
        }
        break;
    case Type_UVB:
        if (time_tpye == 0)
        {
            if (th == 1)
            {
                ts = (24 - 2) * 3600UL;
            }
            else if (th == 2)
            {
                ts = (24 - 4) * 3600UL;
            }
            else if (th == 3)
            {
                ts = (24 - 6) * 3600UL;
            }
            else if (th == 4)
            {
                ts = (24 - 8) * 3600UL;
            }
        }
        else if (time_tpye == 1)
        {
            if (th == 1)
            {
                ts = 2 * 3600UL;
            }
            else if (th == 2)
            {
                ts = 4 * 3600UL;
            }
            else if (th == 3)
            {
                ts = 6 * 3600UL;
            }
            else if (th == 4)
            {
                ts = 8 * 3600UL;
            }
        }

        break;
    case Type_MIST:
        if (time_tpye == 0) /* 间隔：1=2h 2=4h 3=8h 4=12h */
        {
            if (th == 1)
            {
                ts = 2 * 3600UL;
            }
            else if (th == 2)
            {
                ts = 4 * 3600UL;
            }
            else if (th == 3)
            {
                ts = 8 * 3600UL;
            }
            else if (th == 4)
            {
                ts = 12 * 3600UL;
            }
        }
        else if (time_tpye == 1) /* 单次运行：1=0.5h 2=1h 3=2h */
        {
            if (th == 1)
            {
                ts = 30 * 60UL;
            }
            else if (th == 2)
            {
                ts = 1 * 3600UL;
            }
            else if (th == 3)
            {
                ts = 2 * 3600UL;
            }
        }
        break;
    case Type_LIGHT:

        break;
    case Type_HEATER:

        break;
    case Type_IR:

        break;
    case Type_PLASMA1:

        break;
    case Type_ANION:

        break;
    case Type_LOCK:

        break;
    default:
        break;
    }
    return ts;
}

static uint8_t T5lStcAcknowledgedRunState(uint16_t addr)
{
    DATA_NODE *node;
    uint16_t queued_addr;

    node = &G_Queue.Data[G_Queue.last_head];
    if (node->len >= 8U)
    {
        queued_addr = (uint16_t)(((uint16_t)node->buf[4] << 8) |
                                 node->buf[5]);
        if (queued_addr == addr)
        {
            return node->buf[7];
        }
    }

    switch (addr)
    {
    case OUTWIND_CMDWORD:
        return (uint8_t)G_Device_Ctrl.Exhaust.target_speed;
    case INWIND_CMDWORD:
    case INWIND2_CMDWORD:
        return (uint8_t)G_Device_Ctrl.Inlet_Fan.target_speed;
    case LIGHT_CMDWORD:
        return (uint8_t)G_Device_Ctrl.Light.target_brightness;
    case UVB_CMDWORD:
        return (uint8_t)G_Device_Ctrl.UVB.target_brightness;
    case HEATER_CMDWORD:
        return (uint8_t)G_Device_Ctrl.Heater.enable;
    case ANION_CMDWORD:
        return (uint8_t)G_Device_Ctrl.Anion.enable;
    case PLASMA_CMDWORD:
        return (uint8_t)G_Device_Ctrl.Plasma.enable;
    case HUMIDIFIER_CMDWORD:
        return (uint8_t)G_Device_Ctrl.Humidifier.enable;
    default:
        return 0U;
    }
}

//应答成功回调
void T5l_Stc_UartRxProcess(uint16_t addr, uint8_t result)
{
    uint16_t tmp;
    uint16_t time_value;
    uint16_t queued_addr;
    DATA_NODE *node;
    if (addr >= 0x6100U && addr <= 0x61FFU)
    {
        /* 1) 应答地址必须等于"在途帧"的地址, 否则是重发/串台的迟到帧, 丢弃。
           若 STC 按交换字节回显地址，这里归一化为在途帧地址以兼容两种字节序 */
        if (G_Queue.sta == STATE_WAIT_ACK)
        {
            node = &G_Queue.Data[G_Queue.last_head];
            if (node->len >= 8U)
            {
                queued_addr = (uint16_t)(((uint16_t)node->buf[4] << 8) | node->buf[5]);
                if (addr != queued_addr)
                {
                    if (addr == (uint16_t)((queued_addr >> 8) | (queued_addr << 8)))
                    {
                        addr = queued_addr;
                    }
                    else
                    {
                        return;
                    }
                }
            }
        }

        /* 2) STC 说没执行 → 不弹"设定成功", 也不做乐观状态更新 */
        if (result == 0U)
        {
            G_Queue.sta = STATE_WAIT_ACK_FAILURE;
            G_Queue.err_cnt = 0;
            write_dgus_vp(0x535C, "\x00\x01", 1); //设定成功
            G_Queue.showsuccse_time_ms = 1000;
            return;
        }

        G_Queue.sta = STATE_WAIT_ACK_SUCCESS;
        write_dgus_vp(0x535C, "\x00\x02", 1); //设定成功
        G_Queue.showsuccse_time_ms = 1000;
      
        switch (addr)
        {
        case OUTWIND_CMDWORD:
           //排风
            if (G_Device_Ctrl.Exhaust.target_speed == 0)
            {
                G_Device_Ctrl.Exhaust.running = FALSE;
                if (G_Device_Ctrl.Exhaust.auto_vent_en == 0)
                {
                    G_Device_Ctrl.Exhaust.enable = 0;
                    write_dgus_vp(OUTWIND_VP, (uint8_t *)&G_Device_Ctrl.Exhaust.enable, 1); //更新设置页图标开关状态
                }
            }
            else
            {
                G_Device_Ctrl.Exhaust.auto_vent_en = 1;
                G_Device_Ctrl.Exhaust.enable = 1;
                G_Device_Ctrl.Exhaust.running = TRUE;
                /* 排风运行时/间隔时长仅保存用于显示，不再启动到时自动关闭/间隔自动重开计时 */
                read_dgus_vp(OUTWIND_VP + 2, (uint8_t *)&time_value, 1);
                if (time_value != G_Device_Ctrl.Exhaust.interval_time_h)
                {
                    G_Device_Ctrl.Exhaust.interval_time_h = time_value;
                    Start_Once_SaveData();
                }
                if (G_Device_Ctrl.Exhaust.speed != G_Device_Ctrl.Exhaust.target_speed)
                {
                    G_Device_Ctrl.Exhaust.speed = G_Device_Ctrl.Exhaust.target_speed;
                    Start_Once_SaveData();
                }

                write_dgus_vp(OUTWIND_VP, (uint8_t *)&G_Device_Ctrl.Exhaust.enable, 1); //更新设置页图标开关状态
            }
            break;
        case INWIND_CMDWORD:
        case INWIND2_CMDWORD:
          if (G_Device_Ctrl.Inlet_Fan.target_speed == 0)
            {
                G_Device_Ctrl.Inlet_Fan.enable = 0;
                G_Device_Ctrl.Inlet_Fan.running = FALSE;
            }
            else
            {
                G_Device_Ctrl.Inlet_Fan.enable = 1;

                G_Device_Ctrl.Inlet_Fan.running = TRUE;
                G_Device_Ctrl.Inlet_Fan.speed = G_Device_Ctrl.Inlet_Fan.target_speed;
            }
            break;
        case LIGHT_CMDWORD:
            //照明灯
            if (G_Device_Ctrl.Light.target_brightness == 0)
            {
                G_Device_Ctrl.Light.enable = 0;
                G_Device_Ctrl.Light.running = FALSE;
            }
            else
            {
                G_Device_Ctrl.Light.enable = 1;
                G_Device_Ctrl.Light.running = TRUE;
                if (G_Device_Ctrl.Light.brightness != G_Device_Ctrl.Light.target_brightness)
                {
                    G_Device_Ctrl.Light.brightness = G_Device_Ctrl.Light.target_brightness;
                    Start_Once_SaveData();
                }
            }

            write_dgus_vp(LIGHT_VP, (uint8_t *)&G_Device_Ctrl.Light.enable, 1); //更新图标状态
            break;
        case IR_CMDWORD:

            break;
        case UVC_CMDWORD:
            // UVC
            if (G_Device_Ctrl.UVC.enable == 1)
            {
                G_Device_Ctrl.UVC.enable = 0;
                read_dgus_vp(0x14, (uint8_t *)&tmp, 1);
                if (tmp == 13)
                {
                    SwitchPageById(5);
                }
            }
            else
            {
                G_Device_Ctrl.UVC.enable = 1;
                SwitchPageById(13);
                /* 仅读取并保存消杀时长档位用于显示/记忆；不再启动到时自动关闭计时 */
                read_dgus_vp(UVC_VP + 1, (uint8_t *)&G_Device_Ctrl.UVC.running_time_h, 1);
            }

            Start_Once_SaveData();
            break;
        case UVB_CMDWORD:
           // UVB
            if (G_Device_Ctrl.UVB.target_brightness == 0)
            {
                G_Device_Ctrl.UVB.running = FALSE;
                if (G_Device_Ctrl.UVB.auto_UVB_en == 0)
                {
                    G_Device_Ctrl.UVB.enable = 0;
                    write_dgus_vp(UVB_VP, (uint8_t *)&G_Device_Ctrl.UVB.enable, 1); //更新设置页图标开关状态
                }
            }
            else
            {
                G_Device_Ctrl.UVB.auto_UVB_en = 1;
                G_Device_Ctrl.UVB.enable = 1;
                G_Device_Ctrl.UVB.running = TRUE;
                G_Device_Ctrl.UVB.run_status = 0; //清除遗留的运行到点标志，避免重新使能时被立即误关

                write_dgus_vp(UVB_VP, (uint8_t *)&G_Device_Ctrl.UVB.enable, 1); //更新设置页图标开关状态
            }

            if (G_Device_Ctrl.UVB.target_brightness != 0)
            {
                if (G_Device_Ctrl.UVB.brightness != G_Device_Ctrl.UVB.target_brightness)
                {
                    G_Device_Ctrl.UVB.brightness = G_Device_Ctrl.UVB.target_brightness;
                    Start_Once_SaveData();
                }
            }
            read_dgus_vp(UVB_VP + 2, (uint8_t *)&time_value, 1);
            if (time_value != G_Device_Ctrl.UVB.running_time_h)
            {
                G_Device_Ctrl.UVB.running_time_h = time_value;
                Start_Once_SaveData();
            }
            /* 运行时长档位仅保存用于显示；不再启动到时自动关闭/间隔自动重开计时 */
            break;
        case HEATER_CMDWORD:
            if (G_Device_Ctrl.Heater.enable == 1)
            {
                G_Device_Ctrl.Heater.running = FALSE;

                G_Device_Ctrl.Heater.enable = 0;
                write_dgus_vp(HEATER_VP, (uint8_t *)&G_Device_Ctrl.Heater.enable, 1); //更新设置页图标开关状态
                
            }
            else
            {
                G_Device_Ctrl.Heater.enable = 1;
                G_Device_Ctrl.Heater.run_status = 2;
                G_Device_Ctrl.Heater.running = TRUE;

                write_dgus_vp(HEATER_VP, (uint8_t *)&G_Device_Ctrl.Heater.enable, 1); //更新设置页图标开关状态
            }

            if (G_Device_Ctrl.Heater.set_temp != G_Device_Ctrl.Heater.target_temp)
            {
                G_Device_Ctrl.Heater.set_temp = G_Device_Ctrl.Heater.target_temp;
                Start_Once_SaveData();
            }

            break;
        case ANION_CMDWORD: // NAI
             if (G_Device_Ctrl.Anion.enable == 0)
            {
                G_Device_Ctrl.Anion.enable = 1;
                G_Device_Ctrl.Anion.running = TRUE;
            }
            else
            {
                G_Device_Ctrl.Anion.enable = 0;
                G_Device_Ctrl.Anion.running = FALSE;
            }
            write_dgus_vp(ANION_VP, (uint8_t *)&G_Device_Ctrl.Anion.enable, 1);
            break;
        case PLASMA_CMDWORD: // PLASMA
            if (G_Device_Ctrl.Plasma.enable == 0)
            {
                G_Device_Ctrl.Plasma.enable = 1;
                G_Device_Ctrl.Plasma.running = TRUE;
            }
            else
            {
                G_Device_Ctrl.Plasma.enable = 0;
                G_Device_Ctrl.Plasma.running = FALSE;
            }

            write_dgus_vp(PLASMA_VP, (uint8_t *)&G_Device_Ctrl.Plasma.enable, 1);
            break;
        case HUMIDIFIER_CMDWORD: // Humidifier
            //雾化
            if (G_Device_Ctrl.Humidifier.enable == 1)
            {
                G_Device_Ctrl.Humidifier.running = FALSE;
                // G_Device_Ctrl.Humidifier
                if (G_Device_Ctrl.Humidifier.auto_mist_en == 0)
                {
                    G_Device_Ctrl.Humidifier.enable = 0;
                    write_dgus_vp(MIST_VP, (uint8_t *)&G_Device_Ctrl.Humidifier.enable, 1); //更新设置页图标开关状态
                }
            }
            else
            {
                G_Device_Ctrl.Humidifier.auto_mist_en = 1;
                G_Device_Ctrl.Humidifier.enable = 1;
                G_Device_Ctrl.Humidifier.running = TRUE;
                G_Device_Ctrl.Humidifier.run_status = 0; //清除遗留的运行到点标志，避免重新使能时被立即误关

                write_dgus_vp(MIST_VP, (uint8_t *)&G_Device_Ctrl.Humidifier.enable, 1); //更新设置页图标开关状态
            }

            read_dgus_vp(MIST_VP + 1, (uint8_t *)&time_value, 1);
            if (time_value != G_Device_Ctrl.Humidifier.interval_time_h)
            {
                G_Device_Ctrl.Humidifier.interval_time_h = time_value;
                Start_Once_SaveData();
            }

            read_dgus_vp(MIST_VP + 2, (uint8_t *)&time_value, 1);
            if (time_value != G_Device_Ctrl.Humidifier.running_time_h)
            {
                G_Device_Ctrl.Humidifier.running_time_h = time_value;
                Start_Once_SaveData();
            }
            /* 运行/间隔档位仅保存用于显示；不再启动到时自动关闭/间隔自动重开计时 */
            break;
        case LOCK_CMDWORD:
            break;
        case SM_FAN_CMDWORD: 
            if (G_Device_Ctrl.Small_Fan.enable == 0)
            {
                G_Device_Ctrl.Small_Fan.enable = 1;
                // G_Device_Ctrl.Small_Fan.running = TRUE;
            }
            else
            {
                G_Device_Ctrl.Small_Fan.enable = 0;
                // G_Device_Ctrl.Small_Fan.running = FALSE;
            }
            break;
        case MIC_LED_CMDWORD: 
            if (G_Device_Ctrl.MicLed.enable == 0)
            {
                G_Device_Ctrl.MicLed.enable = 1;
                // G_Device_Ctrl.Small_Fan.running = TRUE;
            }
            else
            {
                G_Device_Ctrl.MicLed.enable = 0;
                // G_Device_Ctrl.Small_Fan.running = FALSE;
            }
            break;
        case CAMERA_LED_CMDWORD: 
            if (G_Device_Ctrl.CameraLed.enable == 0)
            {
                G_Device_Ctrl.CameraLed.enable = 1;
                // G_Device_Ctrl.Small_Fan.running = TRUE;
            }
            else
            {
                G_Device_Ctrl.CameraLed.enable = 0;
                // G_Device_Ctrl.Small_Fan.running = FALSE;
            }
            break;    
        default:
            break;
        }
    }
}

//状态flash双备份（预留）
void Stc_FlashBackup(void)
{
    // TODO
}

/*
cmd（依据 Docs/功能.docx 指令表，数据统一 2 字节）
0x6110 加热功能(0/1)     0x6111 加热设定温度(℃)
0x6112 雾化功能(0/1)     0x6113 雾化运行时间(min)  0x6114 雾化间隔时间(min)
0x6115 排风功能(0/1)     0x6116 排风档位(0~6)      0x6117 排风间隔时间(min)
0x6118 UVB功能(0/1)      0x6119 UVB档位(0~4)       0x611A UVB间隔时间(min)
0x611B 紫外消杀功能(0/1) 0x611C 紫外消杀时间(min)
0x611D STC复位通知(STC→T5L，1=已复位)
文档未定义新地址的设备沿用原地址：
0x6101/0x6102 进风、0x6103 照明、0x6104 红外、0x6109 负离子、
0x610A 等离子、0x610C 锁、0x610D 小风扇、0x610E/0x610F 麦克/摄像灯

run_sta:
-只用开关的设备：0=OFF，1=ON
-有档位的设备：0=OFF，1~n=档位
*/

void Send_Cmd_Ctrl(uint16_t cmdword, uint16_t value)
{
    uint8_t cmdbuf[16];
    uint8_t t_len = 0;
    uint16_t crc;
    if (cmdword < 0x6100U || cmdword > 0x61FFU)
        return;
    cmdbuf[t_len++] = 0x5A;
    cmdbuf[t_len++] = 0xA5;

    cmdbuf[t_len++] = 0x00; //长度
    cmdbuf[t_len++] = 0x82;

    *(uint16_t *)&cmdbuf[t_len] = cmdword;
    t_len += 2;

    /* 数据统一 2 字节（1 个字），高字节在前 */
    cmdbuf[t_len++] = (uint8_t)(value >> 8);
    cmdbuf[t_len++] = (uint8_t)value;

    //校验
    crc = crc_16((uint8_t *)&cmdbuf[3], t_len - 3);
    cmdbuf[t_len++] = (uint8_t)(crc & 0x00FF);
    cmdbuf[t_len++] = (uint8_t)(crc >> 8);
    // t_len += 2;
    cmdbuf[2] = t_len - 3; // 长度
    //添加到发送队列
    Queue_Push(cmdbuf, t_len);
}

/**
 * @brief 按"功能块"下发：从 cmdword 起连续写 count 个字（一条 82 命令）
 * @details 依据 Docs/功能.docx 的指令表，一个功能的开关+参数占用连续地址，
 *          一次写入可保证 STC 侧拿到成套参数，且只产生一次应答。
 * @param[in] cmdword 功能块起始地址（0x6100~0x61FF）
 * @param[in] words 字数组（每项 16 位，高字节在前）
 * @param[in] count 字数 1~4
 */
void Send_Cmd_Block(uint16_t cmdword, uint16_t *words, uint8_t count)
{
    uint8_t cmdbuf[20];
    uint8_t t_len = 0;
    uint8_t i;
    uint16_t crc;

    if ((cmdword < 0x6100U) || (cmdword > 0x61FFU) ||
        (words == NULL) || (count == 0U) || (count > 4U))
    {
        return;
    }

    cmdbuf[t_len++] = 0x5A;
    cmdbuf[t_len++] = 0xA5;
    cmdbuf[t_len++] = 0x00; //长度
    cmdbuf[t_len++] = 0x82;

    *(uint16_t *)&cmdbuf[t_len] = cmdword;
    t_len += 2;

    for (i = 0; i < count; i++)
    {
        cmdbuf[t_len++] = (uint8_t)(words[i] >> 8);
        cmdbuf[t_len++] = (uint8_t)words[i];
    }

    crc = crc_16((uint8_t *)&cmdbuf[3], t_len - 3);
    cmdbuf[t_len++] = (uint8_t)(crc & 0x00FF);
    cmdbuf[t_len++] = (uint8_t)(crc >> 8);
    cmdbuf[2] = t_len - 3; // 长度
    Queue_Push(cmdbuf, t_len);
}
/**
 * @brief 队列超时检查函数 放入1ms定时器中
 * @details 该函数用于检查队列等待应答的状态，并在等待超时后更新队列状态
 */
void Queue_Time_Check(void)
{
    // 检查队列当前状态是否为等待应答状态
    if (G_Queue.sta == STATE_WAIT_ACK)
    {
        // 如果存在等待应答的计时器
        if (G_Queue.wait_ack_time)
        {
            // 计时器递减
            G_Queue.wait_ack_time--;
            // 如果计时器减到0，表示应答超时
            if (G_Queue.wait_ack_time == 0)
            {
                // 更新队列状态为等待应答超时状态
                G_Queue.sta = STATE_WAIT_ACK_TIMEOUT;
            }
        }
    }
    if (G_Queue.showsuccse_time_ms)
    {
        G_Queue.showsuccse_time_ms--;
        if (G_Queue.showsuccse_time_ms == 0)
        {
            G_Queue.showsuccse_sta = 1;
        }
    }
}

static uint8_t T5lStcServiceFactoryReset(void)
{
    uint8_t clear_buffer[DATALEN];

    if(t5l_factory_reset_pending == 0U)
    {
        return 0U;
    }
#if v851PROTOCOL_ENABLED
    if(V851ProtocolFactoryResetBusy() != 0U)
    {
        return 1U;
    }
#endif
    /* NOR operations mask interrupts: only start after UART4 has drained. */
    t5l_factory_reset_pending = 0U;
    memset(clear_buffer, 0, sizeof(clear_buffer));
    nor_flash_write(MAIN_ADDR, clear_buffer, 8U / 2U);
    nor_flash_write(BACK_ADDR, clear_buffer, 8U / 2U);
    nor_flash_write(MAIN_DATA_ADDR, clear_buffer, DATALEN / 2U);
    nor_flash_write(BACK_DATA_ADDR, clear_buffer, DATALEN / 2U);
    Flash_DataInit();
    return 1U;
}

/* STC 复位（0x611D=1）后重同步：按屏端现有设备状态逐条重发指令，让外设重新运行。
   仅重发"当前为开启"的设备（STC 复位后外设默认关闭，关闭项无需再发关命令）。
   顺序：加热 → 雾化 → 灯光 → 排风 → 负离子 → UVB → 等离子 → 紫外消杀 */
static void StcResyncStep(void)
{
    uint16_t words[2];

    if (stc_resync_pending == 0U)
    {
        return;
    }
    /* 等队列彻底空（含正在等待应答的那条）再发下一条 */
    if ((G_Queue.sta != STATE_IDLE) || (G_Queue.head != G_Queue.tail))
    {
        return;
    }

    switch (stc_resync_step)
    {
    case 0U: /* 加热 */
        if (G_Device_Ctrl.Heater.enable == 1U)
        {
            Heater_On(G_Device_Ctrl.Heater.target_temp);
        }
        break;
    case 1U: /* 雾化 */
        if ((G_Device_Ctrl.Humidifier.enable == 1U) &&
            (G_Device_Ctrl.Humidifier.liquid_status != 2U))
        {
            Humidifier_On();
        }
        break;
    case 2U: /* 灯光 */
        if (G_Device_Ctrl.Light.enable == 1U)
        {
            Light_On(G_Device_Ctrl.Light.brightness);
        }
        break;
    case 3U: /* 排风 */
        if (G_Device_Ctrl.Exhaust.enable == 1U)
        {
            Exhaust_On();
        }
        break;
    case 4U: /* 负离子 */
        if (G_Device_Ctrl.Anion.enable == 1U)
        {
            Dev_Switch_Ctrl(&G_Device_Ctrl.Anion.enable, ANION_CMDWORD, 1U);
        }
        break;
    case 5U: /* UVB */
        if (G_Device_Ctrl.UVB.enable == 1U)
        {
            UVB_On(G_Device_Ctrl.UVB.brightness);
        }
        break;
    case 6U: /* 等离子 */
        if (G_Device_Ctrl.Plasma.enable == 1U)
        {
            Dev_Switch_Ctrl(&G_Device_Ctrl.Plasma.enable, PLASMA_CMDWORD, 1U);
        }
        break;
    case 7U: /* 紫外消杀：直接下发开关+时长，不能走 UVC_On（它会先关闭所有设备） */
        if (G_Device_Ctrl.UVC.enable == 1U)
        {
            read_dgus_vp(UVC_VP + 1, (uint8_t *)&G_Device_Ctrl.UVC.running_time_h, 1);
            words[0] = 1U;
            words[1] = (uint16_t)(Get_Interval_Run_Sec(Type_UVC, 1, G_Device_Ctrl.UVC.running_time_h) / 60UL);
            Send_Cmd_Block(UVC_CMDWORD, words, 2U);
        }
        break;
    default:
        break;
    }

    stc_resync_step++;
    if (stc_resync_step >= 8U)
    {
        stc_resync_step = 0U;
        stc_resync_pending = 0U;
    }
}

void T5L_Stc_Poll(void)
{

    if(T5lStcServiceFactoryReset() != 0U)
    {
        return;
    }

    Queue_Pop_Poll();
    Queue_Sta_Poll();
    // DeRun_Auto();
    Sys_Cfg();
    Flash_SaveData();
    Dev_Aotu_Procese();
    StcResyncStep();
    key_scanf();
}
//系统配置,此方式配置的参数,是掉电不保存的
// is_beep:是否开启触摸屏提示音
// is_sleep:是否开启触摸屏自动待机休眠的功能
void sys_config(uint8_t is_beep, uint8_t is_sleep)
{
#define CONFIG_ADDR 0x80
    uint8_t config_cmd[4];

    //先把之前的设置先读取出来
    read_dgus_vp(CONFIG_ADDR, config_cmd, 2);

    //是否开启提示音
    if (is_beep)
        config_cmd[3] |= 0x08;
    else
        config_cmd[3] &= 0xf7;
    //自动休眠控制
    if (is_sleep)
        config_cmd[3] |= 0x04;
    else
        config_cmd[3] &= 0xfb;

    //再写回去
    config_cmd[0] = 0x5a; //启动写操作
    write_dgus_vp(CONFIG_ADDR, config_cmd, 2);
}

/* 排风：一次下发 [功能 0/1, 档位 0~6, 间隔分钟] 三个字（0x6115~0x6117） */
void Exhaust_On()
{
    uint16_t words[3];
    uint16_t interval_code;

    G_Device_Ctrl.Exhaust.enable = 0;
    read_dgus_vp(OUTWIND_VP + 1, (uint8_t *)&G_Device_Ctrl.Exhaust.target_speed, 1);
    read_dgus_vp(OUTWIND_VP + 2, (uint8_t *)&interval_code, 1);

    words[0] = 1U;                                                                         /* 功能开 */
    words[1] = G_Device_Ctrl.Exhaust.target_speed;                                         /* 档位 1~6 */
    words[2] = (uint16_t)(Get_Interval_Run_Sec(Type_OutWind, 0, interval_code) / 60UL);     /* 间隔分钟 */
    Send_Cmd_Block(OUTWIND_CMDWORD, words, 3U);
}

//正常关闭auto_flag=0; 定时间隔关闭auto_flag=1;0更新开关状态  1不更新开关状态
void Exhaust_Off(uint8_t auto_flag)
{
    uint16_t words[1];

    G_Device_Ctrl.Exhaust.enable = 1;

    G_Device_Ctrl.Exhaust.auto_vent_en = auto_flag;
    if (auto_flag == 0)
    {
        /* 总开关关闭：停止自动循环，清运行/间隔状态，避免残留状态导致再次开启时误动作 */
        G_Device_Ctrl.Exhaust.run_status = 0;
        G_Device_Ctrl.Exhaust.running_time_sec = 0;
        G_Device_Ctrl.Exhaust.interval_time_sec = 0;
    }
    G_Device_Ctrl.Exhaust.target_speed = 0;

    /* 只关闭功能位，档位/间隔保留在 STC 侧 */
    words[0] = 0U;
    Send_Cmd_Block(OUTWIND_CMDWORD, words, 1U);
}

/* 雾化：一次下发 [功能 0/1, 运行分钟, 间隔分钟] 三个字（0x6112~0x6114） */
void Humidifier_On()
{
    uint16_t words[3];
    uint16_t run_code;
    uint16_t interval_code;

    if(G_Device_Ctrl.Humidifier.liquid_status!=2 ){
        G_Device_Ctrl.Humidifier.enable = 0;
        read_dgus_vp(MIST_VP + 2, (uint8_t *)&run_code, 1);      /* 运行时长档位 */
        read_dgus_vp(MIST_VP + 1, (uint8_t *)&interval_code, 1); /* 间隔时长档位 */

        words[0] = 1U;
        words[1] = (uint16_t)(Get_Interval_Run_Sec(Type_MIST, 1, run_code) / 60UL);      /* 运行分钟 */
        words[2] = (uint16_t)(Get_Interval_Run_Sec(Type_MIST, 0, interval_code) / 60UL); /* 间隔分钟 */
        Send_Cmd_Block(HUMIDIFIER_CMDWORD, words, 3U);
    }
}

//正常关闭auto_flag=0; 定时间隔关闭auto_flag=1;0更新开关状态  1不更新开关状态
void Humidifier_Off(uint8_t auto_flag)
{
    uint16_t words[1];

    G_Device_Ctrl.Humidifier.auto_mist_en = auto_flag;
    G_Device_Ctrl.Humidifier.enable = 1;
    if (auto_flag == 0)
    {
        /* 总开关关闭：停止自动循环，清运行/间隔状态 */
        G_Device_Ctrl.Humidifier.run_status = 0;
        G_Device_Ctrl.Humidifier.running_time_s = 0;
        G_Device_Ctrl.Humidifier.interval_time_s = 0;
    }
    /* 只关闭功能位，运行/间隔保留在 STC 侧 */
    words[0] = 0U;
    Send_Cmd_Block(HUMIDIFIER_CMDWORD, words, 1U);
}

/* UVB：一次下发 [功能 0/1, 档位 0~4, 间隔分钟] 三个字（0x6118~0x611A）
   间隔按"24h 周期扣掉运行段"给出，与文档"运行时间"档位配合 */
void UVB_On(uint16_t target_brightness)
{
    uint16_t words[3];
    uint16_t run_min;

    G_Device_Ctrl.UVB.enable = 0;
    G_Device_Ctrl.UVB.target_brightness = target_brightness;
    read_dgus_vp(UVB_VP + 2, (uint8_t *)&G_Device_Ctrl.UVB.running_time_h, 1);

    run_min = (uint16_t)(Get_Interval_Run_Sec(Type_UVB, 1, G_Device_Ctrl.UVB.running_time_h) / 60UL);

    words[0] = 1U;
    words[1] = target_brightness;                                          /* 档位 1~4 */
    words[2] = (uint16_t)((24UL * 60UL) - run_min);                        /* 间隔分钟 */
    Send_Cmd_Block(UVB_CMDWORD, words, 3U);
}

//正常关闭auto_flag=0; 定时间隔关闭auto_flag=1;0更新开关状态  1不更新开关状态
void UVB_Off(uint8_t auto_flag)
{
    uint16_t words[1];

    G_Device_Ctrl.UVB.auto_UVB_en = auto_flag;
    G_Device_Ctrl.UVB.enable = 1;
    G_Device_Ctrl.UVB.target_brightness = 0;
    if (auto_flag == 0)
    {
        /* 总开关关闭：停止自动循环，清运行/间隔状态 */
        G_Device_Ctrl.UVB.run_status = 0;
        G_Device_Ctrl.UVB.running_time_s = 0;
        G_Device_Ctrl.UVB.interval_time_s = 0;
    }
    /* 只关闭功能位，档位/间隔保留在 STC 侧 */
    words[0] = 0U;
    Send_Cmd_Block(UVB_CMDWORD, words, 1U);
}







void Close_All_Dev(){
    if (G_Device_Ctrl.Exhaust.enable == 1)
    {
        Exhaust_Off(0);
    }

    /* 进风：联动与延时关闭逻辑已移除，关机/消杀前必须显式关闭 */
    if ((G_Device_Ctrl.Inlet_Fan.enable == 1) ||
        (G_Device_Ctrl.Inlet_Fan.target_speed != 0))
    {
        InWind_Off();
    }

    if (G_Device_Ctrl.Heater.enable == 1)
    {
        Heater_Off();
    }

    if (G_Device_Ctrl.Light.enable == 1)
    {
        Light_Off();
    }

    if (G_Device_Ctrl.Humidifier.enable == 1)
    {
        Humidifier_Off(0);
    }

    if (G_Device_Ctrl.UVB.enable==1)
    {
        UVB_Off(0);
    }
    if (G_Device_Ctrl.Anion.enable==1)
    {
        Dev_Switch_Ctrl(&G_Device_Ctrl.Anion.enable,ANION_CMDWORD,0);
    }
    if (G_Device_Ctrl.Plasma.enable==1)
    {
        Dev_Switch_Ctrl(&G_Device_Ctrl.Plasma.enable,PLASMA_CMDWORD,0);
    }
    if (G_Device_Ctrl.Small_Fan.enable==1)
    {
        // SMALL_FAN_Off();
        Dev_Switch_Ctrl(&G_Device_Ctrl.Small_Fan.enable,SM_FAN_CMDWORD,0);
    }
}



/* 紫外消杀：一次下发 [功能 0/1, 消杀分钟] 两个字（0x611B~0x611C） */
void UVC_On()
{
    uint16_t words[2];

    memcpy(&G_Back_Device_Ctrl, &G_Device_Ctrl, sizeof(DeviceCtrl));
    Close_All_Dev();

    G_Device_Ctrl.UVC.enable = 0;
    read_dgus_vp(UVC_VP + 1, (uint8_t *)&G_Device_Ctrl.UVC.running_time_h, 1); /* 消杀时长档位 */
    words[0] = 1U;
    words[1] = (uint16_t)(Get_Interval_Run_Sec(Type_UVC, 1, G_Device_Ctrl.UVC.running_time_h) / 60UL);
    Send_Cmd_Block(UVC_CMDWORD, words, 2U);
}



void UVC_Off()
{
    uint16_t words[1];

    G_Device_Ctrl.UVC.enable = 1;
    words[0] = 0U;
    Send_Cmd_Block(UVC_CMDWORD, words, 1U);

    if (G_Back_Device_Ctrl.Exhaust.enable == 1)
    {
        Exhaust_On();
    }

    if (G_Back_Device_Ctrl.Heater.enable == 1)
    {
        Heater_On(G_Back_Device_Ctrl.Heater.target_temp);
    }

    if (G_Back_Device_Ctrl.Light.enable == 1)
    {
        Light_On(G_Back_Device_Ctrl.Light.brightness);
    }

    if (G_Back_Device_Ctrl.Humidifier.enable == 1)
    {
        Humidifier_On();
    }

    if (G_Back_Device_Ctrl.UVB.enable==1)
    {
        UVB_On(G_Back_Device_Ctrl.UVB.brightness);
    }
    if (G_Back_Device_Ctrl.Anion.enable==1)
    {
        Dev_Switch_Ctrl(&G_Device_Ctrl.Anion.enable,ANION_CMDWORD,1);
    }
    if (G_Back_Device_Ctrl.Plasma.enable==1)
    {
        Dev_Switch_Ctrl(&G_Device_Ctrl.Plasma.enable,PLASMA_CMDWORD,1);
    }
    if (G_Back_Device_Ctrl.Small_Fan.enable==1)
    {
        // SMALL_FAN_On();
        Dev_Switch_Ctrl(&G_Device_Ctrl.Small_Fan.enable,SM_FAN_CMDWORD,1);
    }
}





/* 加热：一次下发 [功能 0/1, 设定温度℃] 两个字（0x6110~0x6111）
   恒温判断由 STC 主控完成，屏端只负责下发开关与目标温度 */
void Heater_On(int16_t target_tmp)
{
    uint16_t words[2];

    G_Device_Ctrl.Heater.target_temp = (uint16_t)target_tmp;

    /* 保护：温湿度与 NTC 双传感器都故障时不开启加热 */
    // if (G_Device_Ctrl.environment.GXHTC3_err_sta != 0 && G_Device_Ctrl.environment.NTC_err_sta != 0)
    // {
    //     Heater_Off();
    //     return;
    // }

    G_Device_Ctrl.Heater.enable = 0;
    words[0] = (uint16_t)target_tmp;
    words[1] = 1U;
    Send_Cmd_Block(HEATER_CMDWORD, words, 2U);
}

//加热自动温控任务已移除，改为纯手动开关，故不再需要 auto_flag
void Heater_Off(void)
{
    uint16_t words[1];

    G_Device_Ctrl.Heater.run_status = 1;
    G_Device_Ctrl.Heater.enable = 1;
    words[0] = 0U;
    Send_Cmd_Block(HEATER_CMDWORD, words, 1U);
}

void InWind_On(uint16_t speed)
{
    G_Device_Ctrl.Inlet_Fan.target_speed = speed;
    G_Device_Ctrl.Inlet_Fan.enable = 0;
    Send_Cmd_Ctrl(INWIND_CMDWORD, (uint8_t)G_Device_Ctrl.Inlet_Fan.target_speed);
    Send_Cmd_Ctrl(INWIND2_CMDWORD, (uint8_t)G_Device_Ctrl.Inlet_Fan.target_speed);
}
void InWind_Off()
{
    G_Device_Ctrl.Inlet_Fan.target_speed = 0;
    G_Device_Ctrl.Inlet_Fan.enable = 1;
    Send_Cmd_Ctrl(INWIND_CMDWORD, (uint8_t)G_Device_Ctrl.Inlet_Fan.target_speed);
    Send_Cmd_Ctrl(INWIND2_CMDWORD, (uint8_t)G_Device_Ctrl.Inlet_Fan.target_speed);
}

void Light_On(uint16_t target_brightness)
{
    G_Device_Ctrl.Light.enable = 0;
    G_Device_Ctrl.Light.target_brightness = target_brightness;
    Send_Cmd_Ctrl(LIGHT_CMDWORD, (uint8_t)target_brightness);
}

void Light_Off()
{
    G_Device_Ctrl.Light.target_brightness = 0;
    G_Device_Ctrl.Light.enable = 1;
    Send_Cmd_Ctrl(LIGHT_CMDWORD, (uint8_t)G_Device_Ctrl.Light.target_brightness);
}

// void Anion_On(void)
// {
//     G_Device_Ctrl.Anion.enable=0;
//     Send_Cmd_Ctrl(ANION_CMDWORD, 1U);
// }

// void Anion_Off(void)
// {
//     G_Device_Ctrl.Anion.enable=1;
//     Send_Cmd_Ctrl(ANION_CMDWORD, 0U);
// }

// void Plasma_On(void)
// {
//     G_Device_Ctrl.Plasma.enable=0;
//     Send_Cmd_Ctrl(PLASMA_CMDWORD, 1U);
// }

// void Plasma_Off(void)
// {
//     G_Device_Ctrl.Plasma.enable=1;
//     Send_Cmd_Ctrl(PLASMA_CMDWORD, 0U);
// }


//小风扇
// void SMALL_FAN_On()
// {
//     G_Device_Ctrl.Small_Fan.enable=0;
//     Send_Cmd_Ctrl(SM_FAN_CMDWORD, 1U);
// }
// void SMALL_FAN_Off()
// {
//     G_Device_Ctrl.Small_Fan.enable=1;
//     Send_Cmd_Ctrl(SM_FAN_CMDWORD, 0U);
// }

// void MicLed_On()
// {
//     G_Device_Ctrl.MicLed.enable=0;
//     Send_Cmd_Ctrl(MIC_LED_CMDWORD, 1U);
// }
// void MicLed_Off()
// {
//     G_Device_Ctrl.MicLed.enable=1;
//     Send_Cmd_Ctrl(MIC_LED_CMDWORD, 0U);
// }

// void CameraLed_On()
// {
//     G_Device_Ctrl.CameraLed.enable=0;
//     Send_Cmd_Ctrl(CAMERA_LED_CMDWORD, 1U);
// }
// void CameraLed_Off()
// {
//     G_Device_Ctrl.CameraLed.enable=1;
//     Send_Cmd_Ctrl(CAMERA_LED_CMDWORD, 0U);
// }

/**
 * @brief 通用设备开关控制
 * @param pEnable 结构体enable成员指针
 * @param cmdWord 设备命令字
 * @param bOn true=开启，false=关闭
 * 等离子、负离子、小风扇、麦克led、摄像头led
 */
void Dev_Switch_Ctrl(uint16_t *pEnable, uint16_t cmdWord, uint8_t bOn)
{
    if(bOn)
    {
        *pEnable = 0;
        Send_Cmd_Ctrl(cmdWord, 1U);
    }
    else
    {
        *pEnable = 1;
        Send_Cmd_Ctrl(cmdWord, 0U);
    }
}



void Dev_Off(){

}


uint8_t check_passwd_format(uint8_t *u8buf)
{
    uint8_t i;
    for (i = 0; i < PASSWD_BTYELEN; i++)
    {
        if ((u8buf[i] < '0') || (u8buf[i] > '9'))
        {
            if(i<PASSWD_BTYELEN){
               
                write_dgus_vp(0x535A, "\x00\x02", 1); //输入4位密码
                G_Queue.showsuccse_time_ms = 1500;
                return 0;
            }
        }
    }
    return 1;
    // if (i >= 4)
    // {
    //     return 1;
    // }
    // else
    // {
    //     write_dgus_vp(0x535A, "\x00\x02", 1); //输入4位密码
    //     G_Queue.showsuccse_time_ms = 1500;
    //     return 0;
    // }
}

uint8_t compare_passwd(uint8_t *buf)
{
    uint8_t ret;

    ret = check_passwd_format(buf);
    if (ret == 1)
    {
        if (0 == memcmp(buf, G_Device_Ctrl.Cfg.passwd, PASSWD_BTYELEN))
        {
            return 1; //密码正确
        }
        else
        {
            write_dgus_vp(0x535A, "\x00\x03", 1); //密码错误
            G_Queue.showsuccse_time_ms = 1500;
        }
    }
    return 0;
}

void touch_ctrl(uint16_t keyvalue)
{
    uint16_t u16buf[4];
    u16buf[0] = 0x5AA5;
    u16buf[1] = 0x0004;
    u16buf[2] = 0xFF00 | (keyvalue);
    u16buf[3] = 0x0001;
    write_dgus_vp(0x00D4, (uint8_t *)&u16buf[0], 4);
}

void PageFunction(void)
{

    uint16_t pageid;
    // EA = 0;
    ADR_H = 0x00;
    ADR_M = 0x00;
    ADR_L = 0x0a;
    ADR_INC = 1;
    RAMMODE = 0xAF;
    /* 有界等待：OS 核无应答时放弃本次读页号，避免 500ms 任务永久卡死 */
    if (!SysDgusWaitAck())
    {
        RAMMODE = 0;
        return;
    }
    APP_EN = 1;
    if (!SysDgusWaitEn())
    {
        RAMMODE = 0;
        return;
    }
    pageid = DATA3;
    pageid <<= 8;
    pageid |= DATA2;
    RAMMODE = 0;
    // EA = 1;

    g_showsta = !g_showsta;
    switch (pageid)
    {
    case 3:
    case 4:

        if (g_showsta)
        {
            write_dgus_vp(0x5380, (uint8_t *)&input_1on, 4);
        }
        else
        {
            write_dgus_vp(0x5380, (uint8_t *)&input_1off, 4);
        }

        break;
    case 15:
        if (g_showsta)
        {
            write_dgus_vp(0x5360, (uint8_t *)&input_1on, 4);
        }
        else
        {
            write_dgus_vp(0x5360, (uint8_t *)&input_1off, 4);
        }
        break;
    case 28:
        if (g_showsta)
        {
            write_dgus_vp(0x5366, (uint8_t *)&input_1on, 4);
        }
        else
        {
            write_dgus_vp(0x5366, (uint8_t *)&input_1off, 4);
        }
        break;
    case 26:
        if (g_showsta)
        {
            if (input_sel == 1)
            {
                write_dgus_vp(PASWD26_VP1, (uint8_t *)&input_1on, 4);
            }
            else if (input_sel == 2)
            {
                write_dgus_vp(PASWD26_VP2, (uint8_t *)&input_2on, 4);
            }
            else if (input_sel == 3)
            {
                write_dgus_vp(PASWD26_VP3, (uint8_t *)&input_3on, 4);
            }
        }
        else
        {
            if (input_sel == 1)
            {
                write_dgus_vp(PASWD26_VP1, (uint8_t *)&input_1off, 4);
            }
            else if (input_sel == 2)
            {
                write_dgus_vp(PASWD26_VP2, (uint8_t *)&input_2off, 4);
            }
            else if (input_sel == 3)
            {
                write_dgus_vp(PASWD26_VP3, (uint8_t *)&input_3off, 4);
            }
        }
        break;
    default:
        break;
    }
}

// Program Size: data=144.0 xdata=5779 const=412 code=15418
void UVC_Ctrl()
{
    if(G_Device_Ctrl.UVC.enable==1){
        
    }
}



void key_scanf(void)
{
    uint8_t u8buf[64];
    uint16_t key_value, tmp;
    // int16_t temp;
    read_dgus_vp(0x5300, (uint8_t *)&key_value, 1);
    if (key_value != 0)
    {
        /* 屏保态下 DGUS 可能不执行页面切换，导致"点了没反应"。
           任何按键到来时先退出屏保、恢复背光并清屏保图，保证切页类按键生效 */
        if (G_Device_Ctrl.Cfg.off_display_flag != 0U)
        {
            u8buf[0] = G_Device_Ctrl.Cfg.ligth_value;
            u8buf[1] = G_Device_Ctrl.Cfg.ligth_value;
            write_dgus_vp(0x82, (uint8_t *)&u8buf, 1);      //恢复背光
            write_dgus_vp(0x603, "\x00\x00", 1);            //退出屏保图
            G_Device_Ctrl.Cfg.off_display_flag &= 0x0F;     //回到"未进屏保"
            G_Device_Ctrl.Cfg.off_display_time_s =
                (uint16_t)(G_Device_Ctrl.Cfg.off_display_time_min * 5U * 60U);
        }

        // Send_Cmd_Ctrl(Exhaust,1,1,3);
        // UartSendData(&Uart2,"\xAA\xFF\n",3);

        switch (key_value)
        {
        case 0x30:
        case 0x31:
        case 0x32:
        case 0x33:
        case 0x34:
        case 0x35:
        case 0x36:
        case 0x37:
        case 0x38:
        case 0x39:

            if (input_sel == 1)
            {
                if (page_cnt1 <= 3)
                {
                    input_1on[page_cnt1] = (uint8_t)key_value;
                    input_1on[page_cnt1 + 1] = 0x7C;
                    input_1on[page_cnt1 + 2] = 0xFF;

                    input_1off[page_cnt1] = (uint8_t)key_value;
                    input_1off[page_cnt1 + 1] = 0xFF;
                    input_1off[page_cnt1 + 2] = 0xFF;
                    page_cnt1++;
                }
            }
            else if (input_sel == 2)
            {
                if (page_cnt2 <= 3)
                {

                    input_2on[page_cnt2] = (uint8_t)key_value;
                    input_2on[page_cnt2 + 1] = 0x7C;
                    input_2on[page_cnt2 + 2] = 0xFF;

                    input_2off[page_cnt2] = (uint8_t)key_value;
                    input_2off[page_cnt2 + 1] = 0xFF;
                    input_2off[page_cnt2 + 2] = 0xFF;
                    page_cnt2++;
                }
            }
            else if (input_sel == 3)
            {
                if (page_cnt3 <= 3)
                {

                    input_3on[page_cnt3] = (uint8_t)key_value;
                    input_3on[page_cnt3 + 1] = 0x7C;
                    input_3on[page_cnt3 + 2] = 0xFF;

                    input_3off[page_cnt3] = (uint8_t)key_value;
                    input_3off[page_cnt3 + 1] = 0xFF;
                    input_3off[page_cnt3 + 2] = 0xFF;
                    page_cnt3++;
                }
            }

            if (g_page_id == 4)
            {
                write_dgus_vp(0x5380, (uint8_t *)&input_1on, 3);
            }
            else if (g_page_id == 15)
            {
                write_dgus_vp(0x5360, (uint8_t *)&input_1on, 3);
            }
            else if (g_page_id == 28)
            {
                write_dgus_vp(0x5366, (uint8_t *)&input_1on, 3);
            }
            else if (g_page_id == 26)
            {
                if (input_sel == 1)
                {
                    write_dgus_vp(PASWD26_VP1, (uint8_t *)&input_1on, 3);
                }
                else if (input_sel == 2)
                {
                    write_dgus_vp(PASWD26_VP2, (uint8_t *)&input_2on, 3);
                }
                else if (input_sel == 3)
                {
                    write_dgus_vp(PASWD26_VP3, (uint8_t *)&input_3on, 3);
                }
            }

            g_showsta = 0;
            break;
        case 0xF2: //删除

            if (input_sel == 1)
            {
                if (page_cnt1 > 0)
                {
                    page_cnt1--;
                    input_1on[page_cnt1] = 0x7C;
                    input_1on[page_cnt1 + 1] = 0xFF;

                    input_1off[page_cnt1] = 0xFF;
                    input_1off[page_cnt1 + 1] = 0xFF;
                }
            }
            else if (input_sel == 2)
            {
                if (page_cnt2 > 0)
                {
                    page_cnt2--;
                    input_2on[page_cnt2] = 0x7C;
                    input_2on[page_cnt2 + 1] = 0xFF;

                    input_2off[page_cnt2] = 0xFF;
                    input_2off[page_cnt2 + 1] = 0xFF;
                }
            }
            else if (input_sel == 3)
            {
                if (page_cnt3 > 0)
                {
                    page_cnt3--;
                    input_3on[page_cnt3] = 0x7C;
                    input_3on[page_cnt3 + 1] = 0xFF;

                    input_3off[page_cnt3] = 0xFF;
                    input_3off[page_cnt3 + 1] = 0xFF;
                }
            }

            if (g_page_id == 4)
            {
                write_dgus_vp(0x5380, (uint8_t *)&input_1on, 3);
            }
            else if (g_page_id == 15)
            {
                write_dgus_vp(0x5360, (uint8_t *)&input_1on, 3);
            }
            else if (g_page_id == 28)
            {
                write_dgus_vp(0x5366, (uint8_t *)&input_1on, 3);
            }
            else if (g_page_id == 26)
            {
                if (input_sel == 1)
                {
                    write_dgus_vp(PASWD26_VP1, (uint8_t *)&input_1on, 3);
                }
                else if (input_sel == 2)
                {
                    write_dgus_vp(PASWD26_VP2, (uint8_t *)&input_2on, 3);
                }
                else if (input_sel == 3)
                {
                    write_dgus_vp(PASWD26_VP3, (uint8_t *)&input_3on, 3);
                }
            }

            g_showsta = 0;
            break;

        case 0x301:
            //进入设置-密码
            Set_Input(4, 1);
            SwitchPageById(4);
            break;
        case 0x402: //取消
            // Clear_Input(4);
            break;
        case 0x403:
            // touch_ctrl(4);
            break;
        case 0x401:
            read_dgus_vp(0x5380, u8buf, 3);
            if (compare_passwd(&u8buf[0]))
            {
                SwitchPageById(5);
            }
            else
            {
                Set_Input(4, 1);
            }
            break;
        case 0x1401:
            //锁密码-确认
            break;
            //===============================加热start================================//
        case 0x500: //加热-开关
            //需要同步检测温度是否到达目标温度
            if (G_Device_Ctrl.Heater.enable == FALSE)
            {
                read_dgus_vp(HEATER_VP + 1, (uint8_t *)&G_Device_Ctrl.Heater.target_temp, 1);
                Heater_On((int16_t)G_Device_Ctrl.Heater.target_temp);
            }
            else
            {
                Heater_Off();
            }
            break;
        case 0x601: //加热-确定
            read_dgus_vp(HEATER_VP + 1, (uint8_t *)&G_Device_Ctrl.Heater.target_temp, 1);
            Heater_On((int16_t)G_Device_Ctrl.Heater.target_temp);

            break;
        case 0x602: //加热-取消
            write_dgus_vp(HEATER_VP + 1, (uint8_t *)&G_Device_Ctrl.Heater.set_temp, 1);

            break;
            //===============================加热end================================//
            //===============================雾化start================================//
        case 0x501: //雾化-开关
            //读取时间，设置定时间隔
            if (G_Device_Ctrl.Humidifier.enable == 0)
            {
                Humidifier_On();
            }
            else
            {
                Humidifier_Off(0);
            }
            break;
        case 0x701: //雾化-确定
            Humidifier_On();
            break;
        case 0x702: //雾化-取消
            write_dgus_vp(MIST_VP + 1, (uint8_t *)&G_Device_Ctrl.Humidifier.interval_time_h, 1);
            write_dgus_vp(MIST_VP + 2, (uint8_t *)&G_Device_Ctrl.Humidifier.running_time_h, 1);
            break;
            //===============================雾化end================================//
            //===============================照明灯start================================//
        case 0x502: //照明灯-开关
            //读取档位
            if (G_Device_Ctrl.Light.enable == 0)
            {
                read_dgus_vp(LIGHT_VP + 1, (uint8_t *)&tmp, 1);
                Light_On(tmp);
            }
            else if (G_Device_Ctrl.Light.enable == 1)
            {
                // G_Device_Ctrl.Light.target_brightness = 0;
                Light_Off();
            }
            break;
        case 0x801: //照明灯-确定
            //只执行打开操作
            read_dgus_vp(LIGHT_VP + 1, (uint8_t *)&G_Device_Ctrl.Light.target_brightness, 1);
            Light_On(G_Device_Ctrl.Light.target_brightness);
            break;
        case 0x802: //照明灯-取消
            write_dgus_vp(LIGHT_VP + 1, (uint8_t *)&G_Device_Ctrl.Light.brightness, 1);
            break;
            //===============================照明灯end================================//
            //===============================排风start================================//
        case 0x503: //排风-开关
            //读取档位，设置定时间隔
            if (G_Device_Ctrl.Exhaust.enable == 0)
            {
                Exhaust_On();
            }
            else
            {
                Exhaust_Off(0);
            }
            break;
        case 0x901: //排风-确定
            //只执行打开操作
            Exhaust_On();
            break;
        case 0x902: //排风-取消
            write_dgus_vp(OUTWIND_VP + 1, (uint8_t *)&G_Device_Ctrl.Exhaust.speed, 1);
            write_dgus_vp(OUTWIND_VP + 2, (uint8_t *)&G_Device_Ctrl.Exhaust.interval_time_h, 1);
            break;
            //===============================排风end================================//
            //===============================负离子 start================================//

        case 0x504: //负离子开关
            if (G_Device_Ctrl.Anion.enable == 0U)
            {
                // Dev_Switch_Ctrl(&G_Device_Ctrl.Anion.enable,ANION_CMDWORD,1);
                Dev_Switch_Ctrl(&G_Device_Ctrl.Anion.enable,ANION_CMDWORD,1);

            }
            else
            {
                // Dev_Switch_Ctrl(&G_Device_Ctrl.Anion.enable,ANION_CMDWORD,0);
                Dev_Switch_Ctrl(&G_Device_Ctrl.Anion.enable,ANION_CMDWORD,0);
            }
            break;
            //===============================负离子 end================================//

            //===============================UVB Start================================//
        case 0x505: // UVB-开关
            //读取档位，设置定时间隔
            if (G_Device_Ctrl.UVB.enable == FALSE)
            {
                read_dgus_vp(UVB_VP + 1, (uint8_t *)&tmp, 1);
                UVB_On(tmp);
            }
            else
            {
                UVB_Off(0);
            }
            break;
        case 0x1001: // UVB-确定
            //只执行打开操作
            read_dgus_vp(UVB_VP + 1, (uint8_t *)&tmp, 1);
            UVB_On(tmp);
            break;
        case 0x1002: // UVB-取消
            write_dgus_vp(UVB_VP + 1, (uint8_t *)&G_Device_Ctrl.UVB.brightness, 1);
            write_dgus_vp(UVB_VP + 2, (uint8_t *)&G_Device_Ctrl.UVB.running_time_h, 1);
            break;
            //===============================UVB End================================//
            //===============================等离子 start================================//
        case 0x506: //等离子开关
            if (G_Device_Ctrl.Plasma.enable == 0U)
            {
                // Dev_Switch_Ctrl(&G_Device_Ctrl.Plasma.enable,PLASMA_CMDWORD,1);
                Dev_Switch_Ctrl(&G_Device_Ctrl.Plasma.enable,PLASMA_CMDWORD,1);
            }
            else
            {
                // Dev_Switch_Ctrl(&G_Device_Ctrl.Plasma.enable,PLASMA_CMDWORD,0);
                Dev_Switch_Ctrl(&G_Device_Ctrl.Plasma.enable,PLASMA_CMDWORD,0);
            }
            break;
            //===============================等离子 end================================//
            //===============================紫外消杀 start================================//
        case 0x507:
            
            SwitchPageById(11);
            break;
        case 0x1101:
            SwitchPageById(12);

            break;
        case 0x1503:
            break;

        case 0x1201: //紫外消杀-确认

            Set_Input(15, 1);
            SwitchPageById(15);
            break;
        case 0x1202: //紫外消杀-取消
            // Clear_Input();
            
            write_dgus_vp(UVC_VP + 1, (uint8_t *)&G_Device_Ctrl.UVC.running_time_h, 1);
            break;
        case 0x1301: //紫外消杀-中断
            if(G_Device_Ctrl.UVC.lock_enable==1){
                    write_dgus_vp(0xE8,"\x5A\x01\x00\x2D",2);//弹窗断锁
                    G_Device_Ctrl.UVC.remind_flag=2;
                }

            break;
        case 0x1501: //紫外消杀-密码确认
                     //密码比对
            read_dgus_vp(0x5360, u8buf, 3);
            if (compare_passwd(u8buf))
            {
                //先跳提示锁确认
                if(G_Device_Ctrl.UVC.lock_enable==0){
                    write_dgus_vp(0xE8,"\x5A\x01\x00\x2C",2);//弹窗闭锁
                    G_Device_Ctrl.UVC.remind_flag=1;
                }
                // UVC_On();
                // 
                //
            }
            else
            {
                Set_Input(15, 1);
            }

            break;
            case 0x4401:
                G_Device_Ctrl.UVC.remind_flag=0;
                write_dgus_vp(0xE8,"\x00\x00",1);//弹窗断锁
            break;
            //===============================紫外消杀 end================================//
            //===============================滤芯 start================================//
        case 0x508:

            SwitchPageById(34);
            break;
        case 0x3403:
            //暂时忽略
            if(G_Device_Ctrl.Humidifier.filter_need_replace==1){
                ET0=0;
                G_Device_Ctrl.Humidifier.hour=24;//24小时后提醒
                G_Device_Ctrl.Humidifier.sec=3600;
                ET0=1;
                G_Device_Ctrl.Humidifier.filter_need_replace=0;
                write_dgus_vp(0x5348,(uint8_t*)&G_Device_Ctrl.Humidifier.filter_need_replace,1);
                Start_Once_SaveData();
            }
            break;
        case 0x3404:
            //立即更新
            if(G_Device_Ctrl.Humidifier.filter_need_replace==1){
                ET0=0;
                G_Device_Ctrl.Humidifier.hour=G_Device_Ctrl.Humidifier.filter_remind_month*30*24;//n小时后提醒
                G_Device_Ctrl.Humidifier.sec=3600;
                ET0=1;
                G_Device_Ctrl.Humidifier.filter_need_replace=0;
                write_dgus_vp(0x5348,(uint8_t*)&G_Device_Ctrl.Humidifier.filter_need_replace,1);
                Start_Once_SaveData();
            }
            
            break;
        case 0x3402:
            //取消
            write_dgus_vp(0x5349, (uint8_t *)&G_Device_Ctrl.Humidifier.filter_remind_month, 1);
            break;
        case 0x3401:
            //滤芯设定
            read_dgus_vp(0x5349, (uint8_t *)&tmp, 1);
            if (G_Device_Ctrl.Humidifier.filter_remind_month != tmp)
            {
                ET0=0;
                G_Device_Ctrl.Humidifier.filter_remind_month=tmp;
                G_Device_Ctrl.Humidifier.hour=G_Device_Ctrl.Humidifier.filter_remind_month*30*24;//n小时后提醒
                G_Device_Ctrl.Humidifier.sec=3600;
                ET0=1;
                Start_Once_SaveData();
            }
            break;
            //===============================滤芯 end================================//
        case 0x2101: // 21页-确认
            read_dgus_vp(0x5126, (uint8_t *)&G_Device_Ctrl.Cfg.temp_uint, 1);
            Set_Temperature_Unit(G_Device_Ctrl.Cfg.temp_uint);
            break;
        case 0x2102: // 21页-取消
            write_dgus_vp(0x5126, (uint8_t *)&G_Device_Ctrl.Cfg.temp_uint, 1);
            break;
        case 0x2201: // 22页-显示-确认
            read_dgus_vp(LIGHT_ADD_SUB_VP, (uint8_t *)&tmp, 2);
            u8buf[0] = (uint8_t)(tmp);
            u8buf[1] = (uint8_t)(tmp);
            write_dgus_vp(0x82, (uint8_t *)&u8buf, 1);

            read_dgus_vp(SCREEN_MIN_VP, (uint8_t *)&G_Device_Ctrl.Cfg.off_display_time_min, 1);
            read_dgus_vp(SCREEN_SAVESTA_VP, (uint8_t *)&G_Device_Ctrl.Cfg.screen_save, 1);
			G_Device_Ctrl.Cfg.off_display_flag = (uint8_t)G_Device_Ctrl.Cfg.screen_save & 0x0F;
            Start_Once_SaveData();
            break;
        case 0x2202: // 22页-显示-取消
            read_dgus_vp(0x82, (uint8_t *)&u8buf, 1);


            write_dgus_vp(SCREEN_MIN_VP, (uint8_t *)&G_Device_Ctrl.Cfg.off_display_time_min, 1);
            write_dgus_vp(SCREEN_SAVESTA_VP, (uint8_t *)&G_Device_Ctrl.Cfg.screen_save, 1);
            break;
        case 0x2004:
            Set_Input(26, 3);
            Set_Input(26, 2);
            Set_Input(26, 1);
            break;
        case 0x26A1:
            Set_Input(26, 1);
            // input_sel=1;
            break;
        case 0x26A2:
            Set_Input(26, 2);
            // input_sel=2;
            break;
        case 0x26A3:
            Set_Input(26, 3);
            // input_sel=3;
            break;
        case 0x26A4:
            //密码设置确认
            if (input_sel == 1)
            {
                Set_Input(26, 2);
                // input_sel=2;
                break;
            }
            else if (input_sel == 2)
            {
                Set_Input(26, 3);
                // input_sel=3;
                break;
            }
            else
            {
            }
        case 0x2601:
            read_dgus_vp(PASWD26_VP1, &u8buf[0], 14);

            if (check_passwd_format(&u8buf[0]))
            {
                if (check_passwd_format(&u8buf[10]))
                {
                    if (check_passwd_format(&u8buf[20]))
                    {
                        if (0 == memcmp(&u8buf[0], &G_Device_Ctrl.Cfg.passwd[0], 4))
                        {
                            if (0 == memcmp(&u8buf[10], &u8buf[20], 4))
                            {
                                //密码正确
                                memcpy(G_Device_Ctrl.Cfg.passwd, &u8buf[10], 4);
								SwitchPageById(20);
                                Start_Once_SaveData();
                                break;
                            }
                            else
                            {
                                write_dgus_vp(0x535A, "\x00\x03", 1); //密码错误
                                G_Queue.showsuccse_time_ms = 1500;
                            }
                        }
                        else
                        {
                            write_dgus_vp(0x535A, "\x00\x03", 1); //密码错误
                            G_Queue.showsuccse_time_ms = 1500;
                        }
                    }
                }
            }
            Set_Input(26, 3);
            Set_Input(26, 2);
            Set_Input(26, 1);
            break;
        // case 0x26A1:
        // case 0x26A2:
        // case 0x26A3:
        //     tmp = key_value - 0x26A0;
        //     write_dgus_vp(PASWD26_VP1 + ((tmp - 1) * 3), "\xFF\xFF", 1);
        //     touch_ctrl(tmp);
        // break;
        case 0x2006:

            Set_Input(28, 1);
            SwitchPageById(28);
            break;
        case 0x2801:
            read_dgus_vp(0x5366, u8buf, 3);
            if (compare_passwd(u8buf))
            {
#if v851PROTOCOL_ENABLED
                V851ProtocolRequestFactoryReset();
#endif
                t5l_factory_reset_pending = 1U;
                SwitchPageById(29);
            }
            else
            {
                Set_Input(28, 1);
            }
            break;
        case 0x2802:
            SwitchPageById(20);
            break;
        case 0x2803:
            touch_ctrl(6);
            break;
        case 0x3001:
            read_dgus_vp(VOLUME_ADD_SUB_VP, (uint8_t *)&G_Device_Ctrl.Cfg.volume, 1);
            Start_Once_SaveData();
            break;
        case 0x3002:
            write_dgus_vp(VOLUME_ADD_SUB_VP, (uint8_t *)&G_Device_Ctrl.Cfg.volume, 1);
            tmp = G_Device_Ctrl.Cfg.volume * 3.6;
            write_dgus_vp(VOLUME_NEEDLE_VP, (uint8_t *)&tmp, 1); //指针
            break;
#if v851PROTOCOL_ENABLED
        case 0x30A1:
            V851ProtocolRequestFactoryReport();
            break;
#endif
        case 0x3101:
            break;
        case 0x3102:
            break;
        case 0x3201:
            //语言-确认
            read_dgus_vp(0x535B, (uint8_t *)&key_value, 1);
            Language_Init(key_value);
            // Start_Once_SaveData();
            break;
            case 0x200A:
                //关机
                Close_All_Dev();
                G_Device_Ctrl.dev_enable = 0;
                u8buf[0] = 0;
                u8buf[1] = 0;
                write_dgus_vp(0x82, (uint8_t *)&u8buf, 1);
                
                write_dgus_vp(0xE8,"\x5A\x00\x00\x00",2);
                // delay_ms(50);
                break;

            case 0x2301:
                read_dgus_vp(0x609,(uint8_t*)&key_value,1);
                if(key_value==79){
                    //立即更新
                    SwitchPageById(25);
                }
            break;
            case 0x2502:
                    //退出更新

            break;
        default:
            break;
        }
        key_value = 0;
        write_dgus_vp(0x5300, (uint8_t *)&key_value, 1);
    }
}

/*
brightness 0-100%
*/
void Set_Screen_Param(uint16_t brightness){
    uint16_t tmp;
    uint8_t u8buf[2];
    G_Device_Ctrl.Cfg.ligth_value=(uint8_t)brightness;
    write_dgus_vp(LIGHT_ADD_SUB_VP, (uint8_t *)&brightness, 1); //%
    //系统显示亮度范围20-100
    tmp=(uint16_t)G_Device_Ctrl.Cfg.ligth_value;

    u8buf[0] = (uint8_t)tmp;
    u8buf[1] = (uint8_t)tmp;
    write_dgus_vp(0x82, (uint8_t *)&u8buf, 1); //真实亮度

    tmp = (uint16_t)(G_Device_Ctrl.Cfg.ligth_value-20) * 4.5;// 0-50  75-180  100-360
    write_dgus_vp(LIGHT_NEEDLE_VP, (uint8_t *)&tmp, 1); //指针
    Start_Once_SaveData();
}

void SysCfg_Init()
{
    
    
    Flash_DataInit();
    // sys_config(0, 0);

    Set_Screen_Param((uint16_t)G_Device_Ctrl.Cfg.ligth_value);
    
    //息屏or屏保
    G_Device_Ctrl.Cfg.off_display_flag = (uint8_t)G_Device_Ctrl.Cfg.screen_save & 0x0F;
    //读取时间
    read_dgus_vp(SCREEN_MIN_VP, (uint8_t *)&G_Device_Ctrl.Cfg.off_display_time_min, 1);
    G_Device_Ctrl.Cfg.off_display_time_s = (G_Device_Ctrl.Cfg.off_display_time_min * 5 * 60);

    
    // SwitchPageById(3);
}

//判断屏幕是否被触摸
#define SCREEN_TOUCH_ADDR 0x0016
uint8_t screen_touch(void)
{
    uint8_t rewrite[2];
    uint16_t touch_data[3];
    uint8_t touch_sta;

    rewrite[0] = 0x00;

    read_dgus_vp(SCREEN_TOUCH_ADDR, (uint8_t *)touch_data, 3);
    if ((touch_data[0] >> 8) == 0x5A)
    {
        touch_sta = (uint8_t)(touch_data[0] & 0xFF);

        /* 抬起事件：关机态下用于唤醒开机（与原逻辑一致） */
        if ((touch_sta & 0x02) != 0U)
        {
            if (G_Device_Ctrl.dev_enable == 0)
            {
                G_Device_Ctrl.dev_enable = 1;
            }
        }

        rewrite[1] = touch_sta;
        write_dgus_vp(SCREEN_TOUCH_ADDR, (uint8_t *)rewrite, 1);

        /* 按下(1)/抬起(2)/按压中(3) 任一状态都算"有触摸"，用于立即退出屏保 */
        if ((touch_sta & 0x03) != 0U)
        {
            return 1;
        }
    }
    return 0;
}

void Sys_Cfg(void)
{
    static uint16_t volume;
    // static uint16_t last_page;
    static uint8_t page_chage = 0;
    uint8_t u8buf[8];
    uint16_t tmp1, tmp2;

    if (screen_touch())
    {
        /* 只要屏幕被触摸就退出屏保：重置计时、恢复背光、退出屏保图、清屏保标志。
           不依赖是否产生键值，避免屏保态下按键"点了没反应" */
        G_Device_Ctrl.Cfg.off_display_time_s = (G_Device_Ctrl.Cfg.off_display_time_min * 5 * 60);

        if (G_Device_Ctrl.Cfg.off_display_flag != 0U)
        {
            page_chage = 0;
            G_Device_Ctrl.Cfg.off_display_flag &= 0X0F;

            u8buf[0] = G_Device_Ctrl.Cfg.ligth_value;
            u8buf[1] = G_Device_Ctrl.Cfg.ligth_value;
            write_dgus_vp(0x82, (uint8_t *)&u8buf, 1);   //恢复背光
            write_dgus_vp(0x603, "\x00\x00", 1);         //退出屏保图，回到正常页面

            /* 同步恢复紫外锁提示弹窗，避免弹窗状态残留 */
            if(G_Device_Ctrl.UVC.remind_flag==1){
                write_dgus_vp(0xE8,"\x5A\x01\x00\x2C",2);
            }else if(G_Device_Ctrl.UVC.remind_flag==2){
                write_dgus_vp(0xE8,"\x5A\x01\x00\x2D",2);
            }else{
                write_dgus_vp(0xE8,"\x00\x00",1);
            }
        }

        //49页为首次初始化确认页：不自动跳主页，等待用户按键确认(key_scanf里处理)
        if(G_Device_Ctrl.dev_enable==1){
            read_dgus_vp(0x14, (uint8_t *)&tmp1, 1);
            if (tmp1 != 49){
                SwitchPageById(3);
                G_Device_Ctrl.dev_enable=2;
            }
        }
    }

    if(G_Device_Ctrl.dev_enable==0)return;

    //读取系统亮度
    //触摸屏背光待机设置：
    // D3=开启亮度，0x00-0x64；背光待机控制关闭时，D3为软件亮度调节接口。
    // D2=关闭亮度0x00-0x64；
    // D1:0=开启时间/10mS。
    read_dgus_vp(LIGHT_ADD_SUB_VP, (uint8_t *)&tmp1, 1);
    if (tmp1 != (uint16_t)G_Device_Ctrl.Cfg.ligth_value)
    {
        Set_Screen_Param(tmp1);
    }

    // volume
    read_dgus_vp(VOLUME_ADD_SUB_VP, (uint8_t *)&tmp1, 1);
    if (tmp1 != volume)
    {
        volume = tmp1;
        tmp2 = tmp1 * 3.6;
        write_dgus_vp(VOLUME_NEEDLE_VP, (uint8_t *)&tmp2, 1); //指针
    }
    //
    if (G_Device_Ctrl.Cfg.off_display_flag == 0x11)
    {
        if (page_chage == 0)
        {
            page_chage = 1;
            // read_dgus_vp(0x14, (uint8_t *)&last_page, 1);
            // SwitchPageById(1);
            write_dgus_vp(0xE8,"\x5A\x01\x00\x01",2);
        }
        //每轮换周期都要执行：写屏保图1、翻转flag、重启计时(page_chage只标记已进入屏保态)
        write_dgus_vp(0x603,"\x00\x01",1);
        G_Device_Ctrl.Cfg.off_display_flag &= 0x01;
        G_Device_Ctrl.Cfg.off_display_flag |= 0x21;
        G_Device_Ctrl.Cfg.off_display_time_s = (G_Device_Ctrl.Cfg.off_display_time_min * 5 * 60);
        // G_Device_Ctrl.Cfg.off_display_flag|=0x32;
    }
    else if (G_Device_Ctrl.Cfg.off_display_flag == 0x31)
    {
        // write_dgus_vp(0xE8,"\x5A\x01\x00\x00",2);
        write_dgus_vp(0x603,"\x00\x02",1);
        G_Device_Ctrl.Cfg.off_display_flag &= 0x01;
        G_Device_Ctrl.Cfg.off_display_time_s = (G_Device_Ctrl.Cfg.off_display_time_min * 5 * 60);
    }
    else if (G_Device_Ctrl.Cfg.off_display_flag == 0x12)
    {
        if (page_chage == 0)
        {
            // read_dgus_vp(0x14, (uint8_t *)&last_page, 1);
            page_chage = 1;
            u8buf[0] = 0;
            u8buf[1] = 0;
            write_dgus_vp(0x82, (uint8_t *)&u8buf, 1);
            G_Device_Ctrl.Cfg.off_display_flag &= 0x02;
            write_dgus_vp(0xE8,"\x5A\x01\x00\x00",2);
            // write_dgus_vp(0x603,"\x00\x00",1);
        }
        // SwitchPageById(0);
    }
}

//放入秒中断
void Dev_Time_Check()
{
    if(G_Device_Ctrl.Humidifier.sec){
        G_Device_Ctrl.Humidifier.sec--;
        if(G_Device_Ctrl.Humidifier.sec==0){
            G_Device_Ctrl.Humidifier.hour--;
            G_Device_Ctrl.Humidifier.sec=3600;
            Start_Once_SaveData();
            if(G_Device_Ctrl.Humidifier.hour==0){
                G_Device_Ctrl.Humidifier.filter_need_replace=1;
            }
        }
    }

    /* 紫外(UVC)到时自动关闭计时已移除：消杀由用户手动关闭 */

    if (G_Device_Ctrl.Cfg.save_interval_times)
    {
        G_Device_Ctrl.Cfg.save_interval_times--;
    }

    //显示
    if (G_Device_Ctrl.Cfg.off_display_time_s)
    {
        G_Device_Ctrl.Cfg.off_display_time_s--;
        if (G_Device_Ctrl.Cfg.off_display_time_s == 0)
        {
            if ((G_Device_Ctrl.Cfg.off_display_flag & 0xF1) == 0x01)
            {
                G_Device_Ctrl.Cfg.off_display_flag |= 0x11; //屏保1
            }
            else if ((G_Device_Ctrl.Cfg.off_display_flag & 0xF1) == 0x21)
            {
                G_Device_Ctrl.Cfg.off_display_flag |= 0x31; //屏保2
            }
            else if ((G_Device_Ctrl.Cfg.off_display_flag & 0xF2) == 0x02)
            {
                G_Device_Ctrl.Cfg.off_display_flag |= 0x12;//
            }
        }
    }

    /* 排风/雾化/UVB 运行间隔计时、进风延时关闭等任务已移除：计时由 STC 主控完成 */
}

void Dev_Aotu_Procese()
{
    /* 屏端已不再承担任何定时/联动任务（计时统一由 STC 主控完成）：
       - 排风/雾化/UVB 的运行与间隔自动轮换
       - 紫外(UVC) 到时自动关闭
       - 进风的加热强风联动、离子小风联动、加热关闭后 5 秒延时关风
       屏端只保留各设备的开关接口：InWind_On()/InWind_Off() 及 DGUS 开关 VP。 */
}

void STC_ReporData_Procese(uint32_t addr)
{
    uint16_t u16buf[48];
    int16_t tmp;

    /* STC 复位通知（0x611D=1）：STC 重启后外设状态丢失，
       屏端按现有设备状态重新下发指令，让外设恢复运行 */
    if (addr == STC_RESET_VP)
    {
        read_dgus_vp(STC_RESET_VP, (uint8_t *)&u16buf[0], 1);
        if (u16buf[0] == 1U)
        {
            stc_resync_pending = 1U;
            stc_resync_step = 0U;
        }
        return;
    }

    if (addr < 0x6000 || addr > 0x602F)
        return;
    read_dgus_vp(0x6000, (uint8_t *)&u16buf, 0x30);

    // GXHTC3温湿度传感器
    if (u16buf[2] == 0)
    {
        //正常
        G_Device_Ctrl.environment.humidity = u16buf[1];
        G_Device_Ctrl.environment.GXHTC3_err_sta = 0;
    }
    else
    {
        //湿度传感器异常
        G_Device_Ctrl.environment.GXHTC3_err_sta = 1;

    }
    //判断NTC传感器
    if (u16buf[0x1E] == 0)
    {
        G_Device_Ctrl.environment.temperaturex10 = (int16_t)(u16buf[3]);
        G_Device_Ctrl.environment.temperature = (int16_t)(u16buf[3] / 10);
        G_Device_Ctrl.environment.NTC_err_sta = 0;
    }
    else
    {
        if(G_Device_Ctrl.environment.GXHTC3_err_sta ==0){
            G_Device_Ctrl.environment.temperaturex10 = (int16_t)(u16buf[0]);
            G_Device_Ctrl.environment.temperature = (int16_t)(u16buf[0] / 10);
        }else{
            //温度传感器都异常
            G_Device_Ctrl.environment.temperature=0;
            G_Device_Ctrl.environment.NTC_err_sta = 1;
        }
    }


    //加热器
    if (u16buf[0x14] == 0)
    {
        G_Device_Ctrl.Heater.err_sta = 0;
    }
    else
    {
        G_Device_Ctrl.Heater.err_sta = 1;
    }

    //雾化器
    //水位_故障码
    if (u16buf[0x1A] == 2)
    {
        //无水
        if (G_Device_Ctrl.Humidifier.liquid_status != (uint8_t)u16buf[0x1A])
        {
            G_Device_Ctrl.Humidifier.liquid_status = (uint8_t)u16buf[0x1A];
            Humidifier_Off(0); //缺水关闭雾化，退出自动轮换，加水后需重新手动开启
            G_Device_Ctrl.Humidifier.run_status = 0;
        }
    }else{
        G_Device_Ctrl.Humidifier.liquid_status = (uint8_t)u16buf[0x1A];
        
    }

    //温度图标
    if ((G_Device_Ctrl.environment.GXHTC3_err_sta == 0&& G_Device_Ctrl.environment.NTC_err_sta == 0))
    {
        write_dgus_vp(0x5310, "\x00\x00", 1);//不显示
    }
    else
    {
        write_dgus_vp(0x5310, "\x00\x01", 1);//显示红色异常
        //如果两个传感器都异常就关闭加热
        if((G_Device_Ctrl.environment.GXHTC3_err_sta != 0) && (G_Device_Ctrl.environment.NTC_err_sta != 0)){
            if(G_Device_Ctrl.Heater.enable!=0){
                Heater_Off();//关闭加热
            }
        }
    }

    //湿度图标
    if (G_Device_Ctrl.Humidifier.liquid_status != 2 )
    {
        write_dgus_vp(0x5318, "\x00\x00", 1);
    }
    else
    {
        write_dgus_vp(0x5318, "\x00\x01", 1);
        if(G_Device_Ctrl.Humidifier.enable!=0){
            Humidifier_Off(0);//关闭湿度
        }

    }

    //显示温度
    if (G_Device_Ctrl.Cfg.temp_uint == 2)
    {
        tmp = (G_Device_Ctrl.environment.temperature * 9 / 5) + 32;
    }
    else
    {
        tmp = G_Device_Ctrl.environment.temperature;
    }
    write_dgus_vp(0x5311, (uint8_t *)&tmp, 1);

    //排风扇_故障码
    if (u16buf[6] == 1)
    {
        
        G_Device_Ctrl.Exhaust.err_sta = 1;
        write_dgus_vp(0x5328, "\x00\x01", 1);
        if(G_Device_Ctrl.Exhaust.enable!=0){
            Exhaust_Off(0);
        }
    }
    else if(u16buf[6] == 0)
    {

        G_Device_Ctrl.Exhaust.err_sta = 0;
        write_dgus_vp(0x5328, "\x00\x00", 1);
    }

    //进风故障判断
    if (u16buf[0x09] == 0 || u16buf[0x0C] == 0)
    {
        G_Device_Ctrl.Inlet_Fan.err_sta = 0;
    }
    else
    {
        G_Device_Ctrl.Inlet_Fan.err_sta = 1;
    }

    //紫外开关检测
    if(u16buf[0x1F]==1){
        //允许打开紫外
        G_Device_Ctrl.UVC.lock_enable=1;
        
       
        if(G_Device_Ctrl.UVC.remind_flag==1){
            UVC_On();
            G_Device_Ctrl.UVC.remind_flag=0;
            write_dgus_vp(0xE8,"\x00\x00",1);//关弹窗
        }else{
            if(G_Device_Ctrl.UVC.enable==0&&G_Device_Ctrl.UVC.remind_flag!=2){
                //如果意外断电需要弹窗关锁
                write_dgus_vp(0xE8,"\x5A\x01\x00\x2D",2);//弹窗关锁
                G_Device_Ctrl.UVC.remind_flag=2;
            }
        }



    }else{
        //禁止打开紫外
        G_Device_Ctrl.UVC.lock_enable=0;


        if(G_Device_Ctrl.UVC.enable==1){
            UVC_Off();
            
        }
        if(G_Device_Ctrl.UVC.remind_flag==2){
            UVC_Off();
            write_dgus_vp(0xE8,"\x00\x00",1);//关弹窗
            G_Device_Ctrl.UVC.remind_flag=0;
        }
    }
}

/*****************************************************************************
 函 数 名  :uint16_t Calculate_CRC16_Flash(unsigned char *updata, unsigned char len)
 功能描述  : CRC-16校验
 说明: 在通信协议中，发送方将 原始数据 + 其 CRC 校验码 一起传输。接收方调用此函数计算整个数据包(包括校验码部分)的CRC值，若结果为0，则说明数据无传输错误
 输入参数  : updata		要处理的数据
            len	长度
            mode  0用于验算CRC      1用于生成CRC值，并复制到数组中
 输出参数  :
*****************************************************************************/
uint16_t Calculate_CRC16_Flash(unsigned char *updata, unsigned int len, unsigned char mode)
{
    unsigned int Reg_CRC = 0xffff;
    unsigned int i, j;
    for (i = 0; i < len; i++)
    {
        Reg_CRC ^= *updata++;
        for (j = 0; j < 8; j++)
        {
            if (Reg_CRC & 0x0001)
            {
                Reg_CRC = Reg_CRC >> 1 ^ 0XA001;
            }
            else
            {
                Reg_CRC >>= 1;
            }
        }
    }
    if (mode == 1)
    {
        *updata++ = (uint8_t)Reg_CRC;
        *updata = (uint8_t)(Reg_CRC >> 8);
    }

    return Reg_CRC;
}



#define CACHE_ADDR      0x2000
#define NOR_FLASH_ADDR    0x08
#define NOR_FLASH_R_CMD     0x5A
#define NOR_FLASH_W_CMD     0xA5
uint16_t sector_idx=0;  //0-3
uint32_t cur_save_cnt;  
#define SECTOR_CNT 4
#define SAVEFLASH_CNT 10000
void nor_flash_write(uint32_t addr, uint8_t* buf, uint16_t len)
{
    uint8_t nor_flash_cmd[8];
    write_dgus_vp(CACHE_ADDR, buf, len);

    nor_flash_cmd[0] = NOR_FLASH_W_CMD;
    nor_flash_cmd[1] = (addr >> 16) & 0xFF;
    nor_flash_cmd[2] = (addr >> 8) & 0xFF;
    nor_flash_cmd[3] = addr& 0xFF;
    nor_flash_cmd[4] = (CACHE_ADDR >> 8) & 0xFF;
    nor_flash_cmd[5] = CACHE_ADDR & 0xFF;
    nor_flash_cmd[6] = (len >> 8) & 0xFF;
    nor_flash_cmd[7] = len & 0xFF;

    write_dgus_vp(NOR_FLASH_ADDR+0x1, &nor_flash_cmd[2], 3);
    write_dgus_vp(NOR_FLASH_ADDR, &nor_flash_cmd[0], 1);
    // EA=0;
    {
        /* NOR 操作最长等待 2000ms：擦写被掉电打断/写保护/忙标志异常时
           原 while(1) 会让整个主循环永久卡死（只能断电），故加超时退让 */
        uint16_t wait_ms = 2000U;
        while (wait_ms != 0U) {
            read_dgus_vp(NOR_FLASH_ADDR, nor_flash_cmd, 1);
            if (nor_flash_cmd[0] == 0) {
                break;
            }
            delay_ms(1);
            wait_ms--;
        }
    }
    // EA=1;
}

void nor_flash_read(uint32_t addr, uint8_t* buf, uint16_t len)
{
    uint8_t nor_flash_cmd[8];
    nor_flash_cmd[0] = NOR_FLASH_R_CMD;
    nor_flash_cmd[1] = (addr >> 16) & 0xFF;
    nor_flash_cmd[2] = (addr >> 8) & 0xFF;
    nor_flash_cmd[3] = addr& 0xFF;
    nor_flash_cmd[4] = (CACHE_ADDR >> 8) & 0xFF;
    nor_flash_cmd[5] = CACHE_ADDR & 0xFF;
    nor_flash_cmd[6] = (len >> 8) & 0xFF;
    nor_flash_cmd[7] = len & 0xFF;

    write_dgus_vp(NOR_FLASH_ADDR+0x1, &nor_flash_cmd[2], 3);
    write_dgus_vp(NOR_FLASH_ADDR, &nor_flash_cmd[0], 1);

    // EA=0;
    {
        /* NOR 操作最长等待 2000ms：擦写被掉电打断/写保护/忙标志异常时
           原 while(1) 会让整个主循环永久卡死（只能断电），故加超时退让 */
        uint16_t wait_ms = 2000U;
        while (wait_ms != 0U) {
            read_dgus_vp(NOR_FLASH_ADDR, nor_flash_cmd, 1);
            if (nor_flash_cmd[0] == 0) {
                break;
            }
            delay_ms(1);
            wait_ms--;
        }
    }
    // EA=1;
    read_dgus_vp(CACHE_ADDR, buf, len);
}


void Flash_DataInit(void)
{
#define IDXLEN 8
    int16_t crc1, crc2, tmp;
    uint16_t err_sta = 0;
    uint16_t databuf[32], backdatabuf[32];
    uint16_t idxbuf[4],backidxbuf[4];
    // memset(&G_Device_Ctrl, 0, 64);
    memset(&databuf, 0, 64);
    memset(&backdatabuf, 0, 64);

    memset(&idxbuf, 0, IDXLEN);
    memset(&backidxbuf, 0, IDXLEN);

    
    nor_flash_read(MAIN_ADDR,(uint8_t *)&idxbuf,IDXLEN/2);
    nor_flash_read(BACK_ADDR,(uint8_t *)&backidxbuf,IDXLEN/2);
    if (0 == Calculate_CRC16_Flash((uint8_t *)&idxbuf, IDXLEN, 0))
    {
        crc1 = 1;
    }
    else
    {
        crc1 = 0;
    }

    if (0 == Calculate_CRC16_Flash((uint8_t *)&backidxbuf, IDXLEN, 0))
    {
        crc2 = 1;
    }
    else
    {
        crc2 = 0;
    }

    err_sta = 0; // 为0时,代表正常,无需更新数据

    if (crc1 == 0 && crc2 == 0)
    {
        
        idxbuf[0]=0x5aa5;
        idxbuf[1]=0xaa55;
        idxbuf[2]=sector_idx=0;
        Calculate_CRC16_Flash((uint8_t *)&idxbuf, IDXLEN-2, 1);
        // 复制到备份
        nor_flash_write(MAIN_ADDR, (uint8_t *)&idxbuf, IDXLEN/2);
        nor_flash_write(BACK_ADDR, (uint8_t *)&idxbuf, IDXLEN/2);
    } 
    else if (crc1 == 1 || crc2 == 1)
    {
        if (crc1 == 1 && crc2 == 0) // 仅主份正常
        {
            nor_flash_write(BACK_ADDR, (uint8_t *)&idxbuf,IDXLEN/2);
            err_sta = 2;
        }
        else if (crc1 == 0 && crc2 == 1) // 仅备份正常
        {
            memcpy(&idxbuf, &backidxbuf, IDXLEN);
            nor_flash_write(MAIN_ADDR, (uint8_t *)&backidxbuf,IDXLEN/2);
            err_sta = 3;
        }
        sector_idx=idxbuf[2];
    }

    // UartSendData(&Uart2,&idxbuf,8);
    //读主区和备份区
    nor_flash_read(MAIN_DATA_ADDR+((uint32_t)sector_idx*0x800), (uint8_t *)&databuf, DATALEN / 2);
    nor_flash_read(BACK_DATA_ADDR+((uint32_t)sector_idx*0x800), (uint8_t *)&backdatabuf, DATALEN / 2);

    if (0 == Calculate_CRC16_Flash((uint8_t *)&databuf, DATALEN, 0))
    {
        crc1 = 1;
    }
    else
    {
        crc1 = 0;
    }

    if (0 == Calculate_CRC16_Flash((uint8_t *)&backdatabuf, DATALEN, 0))
    {
        crc2 = 1;
    }
    else
    {
        crc2 = 0;
    }

    err_sta = 0; // 为0时,代表正常,无需更新数据

    if (crc1 == 0 && crc2 == 0)
    {
        /*
         * 1. 出场设置
         * 2. 两份都是错误的,需要执行出厂初始化
         * */
        memset(&databuf, 0, DATALEN);
        //排风
        databuf[0] = G_Device_Ctrl.Exhaust.speed = 2;
        databuf[1] = G_Device_Ctrl.Exhaust.interval_time_h = OutWind_TIMER_1H;
        write_dgus_vp(0x5101, (uint8_t *)&G_Device_Ctrl.Exhaust.speed, 1); //排风
        write_dgus_vp(0x5102, (uint8_t *)&G_Device_Ctrl.Exhaust.interval_time_h, 1);
        // write_dgus_vp(0x5328, (uint8_t *)&G_Device_Ctrl.Exhaust.speed, 1); //排风

        // UVB
        databuf[2] = G_Device_Ctrl.UVB.brightness = 2;
        databuf[3] = G_Device_Ctrl.UVB.running_time_h = UVB_TIMER_2H;
        write_dgus_vp(0x510D, (uint8_t *)&G_Device_Ctrl.UVB.brightness, 1); // UVB
        write_dgus_vp(0x510E, (uint8_t *)&G_Device_Ctrl.UVB.running_time_h, 1);

        //雾化
        databuf[4] = G_Device_Ctrl.Humidifier.interval_time_h = MIST_INTERVAL_TIMER_2H;
        databuf[5] = G_Device_Ctrl.Humidifier.running_time_h = MIST_RUN_TIMER_1H;
        write_dgus_vp(0x5111, (uint8_t *)&G_Device_Ctrl.Humidifier.interval_time_h, 1); //雾化
        write_dgus_vp(0x5112, (uint8_t *)&G_Device_Ctrl.Humidifier.running_time_h, 1);
        //灯光
        databuf[6] = G_Device_Ctrl.Light.brightness = 2;
        write_dgus_vp(0x5115, (uint8_t *)&G_Device_Ctrl.Light.brightness, 1); //灯光

        //加热
        databuf[7] = G_Device_Ctrl.Heater.set_temp = 28;
        // databuf[7] = G_Device_Ctrl.Heater.set_temp = 40;//演示
        write_dgus_vp(0x5119, (uint8_t *)&G_Device_Ctrl.Heater.set_temp, 1); //加热

        // UVC
        databuf[8] = G_Device_Ctrl.UVC.running_time_h = UVC_TIMER_15M;
        write_dgus_vp(0x5109, (uint8_t *)&G_Device_Ctrl.UVC.running_time_h, 1); // UVC

        //温度单位
        databuf[9] = G_Device_Ctrl.Cfg.temp_uint = 1;
        write_dgus_vp(0x5126, (uint8_t *)&G_Device_Ctrl.Cfg.temp_uint, 1); //温度单位
        if (G_Device_Ctrl.Cfg.temp_uint == 1)
        {
            tmp = (uint16_t)(45 << 8) | 0;
            write_dgus_vp(0x5800 + 7, (uint8_t *)&tmp, 1);
        }
        else
        {
            tmp = (uint16_t)(6 << 8) | 0;
            write_dgus_vp(0x5800 + 7, (uint8_t *)&tmp, 1);
        }
        //屏保&息屏
        // read_dgus_vp(0x82, u8buf, 1);
        G_Device_Ctrl.Cfg.ligth_value = 80;//默认80
        databuf[10] = (uint16_t )G_Device_Ctrl.Cfg.ligth_value;
        databuf[11] = G_Device_Ctrl.Cfg.screen_save = 1;
        databuf[12] = G_Device_Ctrl.Cfg.off_display_time_min = 1;
        
        write_dgus_vp(SCREEN_SAVESTA_VP, (uint8_t *)&databuf[11], 1); //屏保&息屏
        write_dgus_vp(SCREEN_MIN_VP, (uint8_t *)&databuf[12], 1);     //屏保&息屏

        //音量
        databuf[13] = G_Device_Ctrl.Cfg.volume = 50;
        write_dgus_vp(VOLUME_ADD_SUB_VP, (uint8_t *)&databuf[13], 1); //

        //进风
        databuf[14] = G_Device_Ctrl.Inlet_Fan.speed = 2;
        databuf[15] = 0;

        //密码
        memcpy(&G_Device_Ctrl.Cfg.passwd[0], PASSWD, 4);
        memcpy(&databuf[16], &G_Device_Ctrl.Cfg.passwd[0], 4);

        //语言
        databuf[18] = 1;
       
        Language_Init(databuf[18]);

        //滤芯提醒
        databuf[19] = G_Device_Ctrl.Humidifier.filter_remind_month = 2;
        databuf[20] = G_Device_Ctrl.Humidifier.hour=G_Device_Ctrl.Humidifier.filter_remind_month*30*24;
        G_Device_Ctrl.Humidifier.sec=3600;
        write_dgus_vp(0x5349, (uint8_t *)&G_Device_Ctrl.Humidifier.filter_remind_month, 1);

        databuf[22] = G_Device_Ctrl.Humidifier.filter_need_replace = 0;
        write_dgus_vp(0x5348,(uint8_t*)&G_Device_Ctrl.Humidifier.filter_need_replace,1);



        
        *(uint32_t*)&databuf[28]=cur_save_cnt=1;//存储次数
        Calculate_CRC16_Flash((uint8_t *)&databuf, DATALEN - 2, 1);
        // 复制到备份
        nor_flash_write(MAIN_DATA_ADDR+((uint32_t)sector_idx*0x800), (uint8_t *)&databuf, DATALEN / 2);
        nor_flash_write(BACK_DATA_ADDR+((uint32_t)sector_idx*0x800), (uint8_t *)&databuf, DATALEN / 2);
        err_sta = 4;

        SwitchPageById(49);
    }
    else if (crc1 == 1 || crc2 == 1)
    {

        if (crc1 == 1 && crc2 == 0) // 仅主份正常
        {
            nor_flash_write(BACK_DATA_ADDR, (uint8_t *)&databuf, DATALEN / 2);
            err_sta = 2;
        }
        else if (crc1 == 0 && crc2 == 1) // 仅备份正常
        {
            memcpy(&databuf, &backdatabuf, DATALEN);
            nor_flash_write(MAIN_DATA_ADDR, (uint8_t *)&backdatabuf, DATALEN / 2);
            err_sta = 3;
        }
        G_Device_Ctrl.Exhaust.speed = databuf[0];
        G_Device_Ctrl.Exhaust.interval_time_h = databuf[1];
        write_dgus_vp(0x5101, (uint8_t *)&G_Device_Ctrl.Exhaust.speed, 1);           //排风
        write_dgus_vp(0x5102, (uint8_t *)&G_Device_Ctrl.Exhaust.interval_time_h, 1); //排风
        // write_dgus_vp(0x5328, (uint8_t *)&G_Device_Ctrl.Exhaust.speed, 1); //排风

        G_Device_Ctrl.UVB.brightness = databuf[2];
        G_Device_Ctrl.UVB.running_time_h = databuf[3];
        write_dgus_vp(0x510D, (uint8_t *)&G_Device_Ctrl.UVB.brightness, 1); // UVB
        write_dgus_vp(0x510E, (uint8_t *)&G_Device_Ctrl.UVB.running_time_h, 1);

        //雾化
        G_Device_Ctrl.Humidifier.interval_time_h = databuf[4];
        G_Device_Ctrl.Humidifier.running_time_h = databuf[5];
        write_dgus_vp(0x5111, (uint8_t *)&G_Device_Ctrl.Humidifier.interval_time_h, 1); //雾化
        write_dgus_vp(0x5112, (uint8_t *)&G_Device_Ctrl.Humidifier.running_time_h, 1);
        //灯光
        G_Device_Ctrl.Light.brightness = databuf[6];
        write_dgus_vp(0x5115, (uint8_t *)&G_Device_Ctrl.Light.brightness, 1); //灯光
        // write_dgus_vp(0x5320,(uint8_t*)&G_Device_Ctrl.Light.brightness,1);//更新灯光状态页图标状态

        //加热
        G_Device_Ctrl.Heater.set_temp = databuf[7];
        write_dgus_vp(0x5119, (uint8_t *)&G_Device_Ctrl.Heater.set_temp, 1); //加热

        // UVC
        G_Device_Ctrl.UVC.running_time_h = databuf[8];
        write_dgus_vp(0x5109, (uint8_t *)&G_Device_Ctrl.UVC.running_time_h, 1); // UVC

        //温度单位
        G_Device_Ctrl.Cfg.temp_uint = databuf[9];
        write_dgus_vp(0x5126, (uint8_t *)&G_Device_Ctrl.Cfg.temp_uint, 1); //温度单位

        if (G_Device_Ctrl.Cfg.temp_uint == 1)
        {
            tmp = (uint16_t)(45 << 8) | 0;
            write_dgus_vp(0x5800 + 7, (uint8_t *)&tmp, 1);
        }
        else
        {
            tmp = (uint16_t)(6 << 8) | 0;
            write_dgus_vp(0x5800 + 7, (uint8_t *)&tmp, 1);
        }

        //屏保&息屏
        G_Device_Ctrl.Cfg.ligth_value =(uint8_t ) databuf[10];
        G_Device_Ctrl.Cfg.screen_save = databuf[11];
        G_Device_Ctrl.Cfg.off_display_time_min = databuf[12];
        write_dgus_vp(SCREEN_MIN_VP, (uint8_t *)&G_Device_Ctrl.Cfg.off_display_time_min, 1); //屏保&息屏
        write_dgus_vp(SCREEN_SAVESTA_VP, (uint8_t *)&G_Device_Ctrl.Cfg.screen_save, 1);
        //音量
        G_Device_Ctrl.Cfg.volume = databuf[13];
        write_dgus_vp(VOLUME_ADD_SUB_VP, (uint8_t *)&G_Device_Ctrl.Cfg.volume, 1); //屏保&息屏

        //进风
        G_Device_Ctrl.Inlet_Fan.speed = databuf[14];

        //密码
        memcpy(&G_Device_Ctrl.Cfg.passwd[0], &databuf[16], 4);

        //语言
        // G_Device_Ctrl.Cfg.language = databuf[18];
        // write_dgus_vp(0x535B, (uint8_t *)&G_Device_Ctrl.Cfg.language, 1);
        Language_Init(databuf[18]);
        //滤芯提醒
        G_Device_Ctrl.Humidifier.filter_remind_month = databuf[19];
        G_Device_Ctrl.Humidifier.hour=databuf[20];
        G_Device_Ctrl.Humidifier.sec=3600;
        G_Device_Ctrl.Humidifier.filter_need_replace = databuf[22];
        write_dgus_vp(0x5348,(uint8_t*)&G_Device_Ctrl.Humidifier.filter_need_replace,1);
        write_dgus_vp(0x5349, (uint8_t *)&G_Device_Ctrl.Humidifier.filter_remind_month, 1);

        cur_save_cnt=*(uint32_t*)&databuf[28];//存储次数
        SwitchPageById(3);
    }
    // UartSendData(&Uart2,&G_Device_Ctrl.Cfg.passwd[0],4);
}

void Start_Once_SaveData()
{
    G_Device_Ctrl.Cfg.save_flag = 1;
    G_Device_Ctrl.Cfg.save_interval_times = 2;
}

void Flash_SaveData()
{
    uint16_t databuf[32];
    uint16_t idxbuf[2];

    if (G_Device_Ctrl.Cfg.save_flag != 1)
        return;
    if (G_Device_Ctrl.Cfg.save_interval_times != 0)
        return;


    G_Device_Ctrl.Cfg.save_flag = 0;
    memset(&databuf, 0, DATALEN);
    memset(&idxbuf, 0, 4);

    //排风
    databuf[0] = G_Device_Ctrl.Exhaust.speed;
    databuf[1] = G_Device_Ctrl.Exhaust.interval_time_h;

    // UVB
    databuf[2] = G_Device_Ctrl.UVB.brightness;
    databuf[3] = G_Device_Ctrl.UVB.running_time_h;

    //雾化
    databuf[4] = G_Device_Ctrl.Humidifier.interval_time_h;
    databuf[5] = G_Device_Ctrl.Humidifier.running_time_h;

    //灯光
    databuf[6] = G_Device_Ctrl.Light.brightness;

    //加热
    databuf[7] = G_Device_Ctrl.Heater.set_temp;

    // UVC
    databuf[8] = G_Device_Ctrl.UVC.running_time_h;

    //温度单位
    databuf[9] = G_Device_Ctrl.Cfg.temp_uint;

    //屏保&息屏
    databuf[10] = (uint16_t)G_Device_Ctrl.Cfg.ligth_value;
    databuf[11] = G_Device_Ctrl.Cfg.screen_save;
    databuf[12] = G_Device_Ctrl.Cfg.off_display_time_min;

    //音量
    databuf[13] = G_Device_Ctrl.Cfg.volume;

    //进风
    databuf[14] = G_Device_Ctrl.Inlet_Fan.speed;

    //密码
    memcpy(&databuf[16], &G_Device_Ctrl.Cfg.passwd[0], 4);

    //语言
    databuf[18] = G_Device_Ctrl.Cfg.language;

    //滤芯提醒
    databuf[19] = G_Device_Ctrl.Humidifier.filter_remind_month;
    databuf[20] = G_Device_Ctrl.Humidifier.hour;

    databuf[22] = G_Device_Ctrl.Humidifier.filter_need_replace; //滤芯是否需要更换


    cur_save_cnt+=1;
    if(cur_save_cnt>=SAVEFLASH_CNT){
        cur_save_cnt=0;
        sector_idx=(sector_idx+1)%SECTOR_CNT;
        idxbuf[2]=sector_idx;
        Calculate_CRC16_Flash((uint8_t *)&idxbuf, 6, 1);
        nor_flash_write(MAIN_ADDR, (uint8_t *)&idxbuf,  4);
        nor_flash_write(BACK_ADDR, (uint8_t *)&idxbuf,  4);
        // UartSendData(&Uart2,(uint8_t*)&cur_save_cnt,4);
        // UartSendData(&Uart2,(uint8_t*)&idxbuf[2],2);
    }
    *(uint32_t*)&databuf[28]=cur_save_cnt;//存储次数
    Calculate_CRC16_Flash((uint8_t *)&databuf, DATALEN - 2, 1);
    nor_flash_write(MAIN_DATA_ADDR+((uint32_t)sector_idx*0x800), (uint8_t *)&databuf, DATALEN / 2);
    nor_flash_write(BACK_DATA_ADDR+((uint32_t)sector_idx*0x800), (uint8_t *)&databuf, DATALEN / 2);
}

void Updata_Vpdata_To_Report()
{
    uint16_t databuf[64];
    databuf[0] = G_Device_Ctrl.Cfg.temp_uint;
    databuf[1] = (uint16_t)G_Device_Ctrl.Cfg.ligth_value;
    databuf[2] = G_Device_Ctrl.Cfg.off_display_time_min;
    databuf[3] = G_Device_Ctrl.Cfg.screen_save;
    databuf[4] = G_Device_Ctrl.Cfg.volume;
    databuf[5] = G_Device_Ctrl.Cfg.language;
    write_dgus_vp(0x3000, (uint8_t *)&databuf[0], 6);

    *(float *)&databuf[0] = ((float)G_Device_Ctrl.environment.temperaturex10 / 10);
    databuf[2] = G_Device_Ctrl.environment.humidity / 10;
    databuf[3] = 0;
    databuf[4] = 0;
    databuf[5] = 0;
    databuf[6] = 0;
    databuf[7] = 0;

    databuf[8] = G_Device_Ctrl.Lock.enable;
    databuf[9] = G_Device_Ctrl.Lock.enable;

    databuf[10] = G_Device_Ctrl.Exhaust.enable;
    databuf[11] = G_Device_Ctrl.Exhaust.speed;
    databuf[12] = G_Device_Ctrl.Exhaust.interval_time_h;
    databuf[13] = G_Device_Ctrl.Exhaust.running;
    databuf[14] = 0;

    databuf[15] = G_Device_Ctrl.Light.enable;
    databuf[16] = G_Device_Ctrl.Light.brightness;
    databuf[17] = G_Device_Ctrl.Light.running;
    databuf[18] = 0;
    databuf[19] = 0;

    databuf[20] = G_Device_Ctrl.UVB.enable;
    databuf[21] = G_Device_Ctrl.UVB.brightness;
    databuf[22] = G_Device_Ctrl.UVB.running_time_h;
    databuf[23] = G_Device_Ctrl.UVB.running;
    databuf[24] = 0;

    databuf[25] = G_Device_Ctrl.Anion.enable;
    databuf[26] = G_Device_Ctrl.Anion.running;
    databuf[27] = 0;
    databuf[28] = 0;
    databuf[29] = 0;

    databuf[30] = G_Device_Ctrl.Plasma.enable;
    databuf[31] = G_Device_Ctrl.Plasma.running;
    databuf[32] = 0;
    databuf[33] = 0;
    databuf[34] = 0;

    databuf[35] = G_Device_Ctrl.Heater.enable;
    databuf[36] = G_Device_Ctrl.Heater.set_temp;
    if (G_Device_Ctrl.Heater.enable == 0)
    {
        databuf[37] = 0;
    }
    else if ((G_Device_Ctrl.Heater.running != 0) ||
             (G_Device_Ctrl.Heater.run_status == 2))
    {
        databuf[37] = 1;
    }
    else
    {
        databuf[37] = 2;
    }
    databuf[38] = G_Device_Ctrl.Heater.running;
    databuf[39] = 0;

    databuf[40] = G_Device_Ctrl.Humidifier.enable;
    databuf[41] = G_Device_Ctrl.Humidifier.interval_time_h;
    databuf[42] = G_Device_Ctrl.Humidifier.running_time_h;
    databuf[43] = G_Device_Ctrl.Humidifier.running;
    databuf[44] = G_Device_Ctrl.Humidifier.liquid_status;

    databuf[45] = G_Device_Ctrl.Inlet_Fan.enable;
    databuf[46] = G_Device_Ctrl.Inlet_Fan.speed;
    databuf[47] = G_Device_Ctrl.Inlet_Fan.running;
    databuf[48] = 0;
    databuf[49] = 0;

    databuf[50] = G_Device_Ctrl.Humidifier.filter_life_percense;
    databuf[51] = G_Device_Ctrl.Humidifier.filter_need_replace;
    databuf[52] = 0;
    databuf[53] = 0;
    databuf[54] = 0;

    write_dgus_vp(0x3010, (uint8_t *)&databuf[0], 55);
}


void AlarmCode_Updata(){
    uint16_t databuf[16];
    databuf[0]=0;//门锁

    //温控
    if(G_Device_Ctrl.environment.GXHTC3_err_sta==1||G_Device_Ctrl.environment.NTC_err_sta==1){
        databuf[1]=3;
    }else if(G_Device_Ctrl.Heater.err_sta==1){
        databuf[1]=5;
    }else{
        databuf[1]=0;
    }

    //雾化水位
    if(G_Device_Ctrl.Humidifier.liquid_status==1){
        databuf[2]=2;//LOW_LIQUID
    }else if(G_Device_Ctrl.environment.GXHTC3_err_sta==1){
        databuf[2]=5;
    }else{
        databuf[2]=0;
    }
    databuf[3]=0;   

    if(G_Device_Ctrl.Exhaust.err_sta==1){
        databuf[4]=1;
    }else{
        databuf[4]=0;

    }
    
    if(G_Device_Ctrl.Inlet_Fan.err_sta==1){
        databuf[5]=1;
    }else{
        databuf[5]=0;

    }

        databuf[6]=0;

    write_dgus_vp(0x5348,(uint8_t*)&G_Device_Ctrl.Humidifier.filter_need_replace,1);
    if(G_Device_Ctrl.Humidifier.filter_need_replace==1){
        databuf[7]=1;
    }else{
        databuf[7]=0;

    }

    databuf[8]=0;
    databuf[9]=0;
    write_dgus_vp(0x3080,(uint8_t*)&databuf,10);
}


/*
 @brief 设置温度单位
 @param Unit 1=°C   2=°F
*/
void Set_Temperature_Unit(uint16_t Unit)
{
    uint16_t tmp,temp;
    G_Device_Ctrl.Cfg.temp_uint=Unit;
    if (G_Device_Ctrl.Cfg.temp_uint == 1)
    {
        tmp = (uint16_t)(45 << 8) | 0;
        write_dgus_vp(0x5800 + 7, (uint8_t *)&tmp, 1);

        write_dgus_vp(0x5311, (uint8_t *)&G_Device_Ctrl.environment.temperature, 1);
    }
    else
    {
        tmp = (uint16_t)(6 << 8) | 0;
        write_dgus_vp(0x5800 + 7, (uint8_t *)&tmp, 1);

        temp = G_Device_Ctrl.environment.temperature * 9 / 5 + 32;
        write_dgus_vp(0x5311, (uint8_t *)&temp, 1);
    }
    Start_Once_SaveData();
}



/*================ STC 开关状态回读校验（每 10s 一次，以 STC 上报为准）================
  STC 通过 0x6000~0x6028 周期上报实际状态。
  屏端下发开关后，若 10s 后屏端记录的状态与 STC 实测不一致（命令丢失、STC 故障保护
  关断、STC 复位等），以 STC 的为准覆盖屏端状态与设置页图标。
  保护：STC 未按新协议上报（版本号 0x6023 = 0）时不做校验，避免旧固件下误关设备。
==================================================================================== */
#define STC_STATE_CHECK_MS      10000UL

static uint32_t stc_state_check_tick;

/* 单个开关比对：不一致则以 STC 状态覆盖屏端状态 + 设置页图标 */
static void StcApplySwitch(uint16_t ctrl_vp, uint16_t *pEnable, uint8_t stc_on)
{
    uint16_t want;

    want = (stc_on != 0U) ? 1U : 0U;
    if (*pEnable == want)
    {
        return;
    }
    *pEnable = want;
    write_dgus_vp(ctrl_vp, (uint8_t *)pEnable, 1); //更新设置页图标开关状态
}

void StcStateCheckTask(void)
{
    uint32_t tick;
    uint16_t buf[0x30];
    uint8_t stc_on;

    tick = GetSysTick();
    if ((uint32_t)(tick - stc_state_check_tick) < STC_STATE_CHECK_MS)
    {
        return;
    }
    stc_state_check_tick = tick;

    /* 0x6000~0x602F 全量状态 */
    read_dgus_vp(0x6000, (uint8_t *)buf, 0x30);

    /* STC 版本号为 0 说明尚未按新协议上报，跳过校验 */
    // if (buf[0x23] == 0U)
    // {
    //     return;
    // }

    /* 5 个主功能只认 STC 的"功能状态"字（0x6024~0x6028），
       不再叠加档位/设备状态：档位字可能保留记忆值，字段错位时也会把
       不相干的设备误判为"开"（曾出现开负离子把 UVB 带亮） */
    stc_on = (uint8_t)((buf[0x24] != 0U) ? 1U : 0U);
    StcApplySwitch(HEATER_VP, &G_Device_Ctrl.Heater.enable, stc_on);

    stc_on = (uint8_t)((buf[0x25] != 0U) ? 1U : 0U);
    StcApplySwitch(MIST_VP, &G_Device_Ctrl.Humidifier.enable, stc_on);

    stc_on = (uint8_t)((buf[0x26] != 0U) ? 1U : 0U);
    StcApplySwitch(OUTWIND_VP, &G_Device_Ctrl.Exhaust.enable, stc_on);
    if (stc_on == 0U)
    {
        G_Device_Ctrl.Exhaust.target_speed = 0U;
        G_Device_Ctrl.Exhaust.running = FALSE;
    }

    stc_on = (uint8_t)((buf[0x27] != 0U) ? 1U : 0U);
    StcApplySwitch(UVB_VP, &G_Device_Ctrl.UVB.enable, stc_on);
    if (stc_on == 0U)
    {
        G_Device_Ctrl.UVB.target_brightness = 0U;
        G_Device_Ctrl.UVB.running = FALSE;
    }

    stc_on = (uint8_t)((buf[0x28] != 0U) ? 1U : 0U);
    StcApplySwitch(UVC_VP, &G_Device_Ctrl.UVC.enable, stc_on);

    /* 以下设备没有"功能状态"字，使用各自的"状态"字（明确的 0/1） */
    stc_on = (uint8_t)((buf[0x0D] != 0U) ? 1U : 0U);
    StcApplySwitch(LIGHT_VP, &G_Device_Ctrl.Light.enable, stc_on);

    /* 负离子 0x6017 */
    stc_on = (uint8_t)((buf[0x17] != 0U) ? 1U : 0U);
    StcApplySwitch(ANION_VP, &G_Device_Ctrl.Anion.enable, stc_on);

    /* 等离子 0x6018 */
    stc_on = (uint8_t)((buf[0x18] != 0U) ? 1U : 0U);
    StcApplySwitch(PLASMA_VP, &G_Device_Ctrl.Plasma.enable, stc_on);

    /* 进风：任一路档位非 0 视为开（0x6008 进风1档位 / 0x600B 进风2档位） */
    stc_on = (uint8_t)(((buf[0x08] != 0U) || (buf[0x0B] != 0U)) ? 1U : 0U);
    StcApplySwitch(INWIND_VP, &G_Device_Ctrl.Inlet_Fan.enable, stc_on);
    if (stc_on != 0U)
    {
        G_Device_Ctrl.Inlet_Fan.speed = buf[0x08];
        G_Device_Ctrl.Inlet_Fan.target_speed = buf[0x08];
    }
    else
    {
        G_Device_Ctrl.Inlet_Fan.speed = 0U;
        G_Device_Ctrl.Inlet_Fan.target_speed = 0U;
    }
}

void Mult_Task()
{
    AlarmCode_Updata();
    Updata_Vpdata_To_Report();
    PageFunction();
    StcStateCheckTask();
}


void Language_Init(uint16_t language_tpye)
{
    uint8_t i=0;
    uint8_t buf[2];
  
    if(language_tpye==0||language_tpye>4)return;
    write_dgus_vp(0x535B, (uint8_t *)&language_tpye, 1);
    if(language_tpye!=G_Device_Ctrl.Cfg.language)
    {
        G_Device_Ctrl.Cfg.language=language_tpye;
        Start_Once_SaveData();
        buf[0]=55+((G_Device_Ctrl.Cfg.language-1)*5);
        buf[1]=0;
        write_dgus_vp(0x5400, (uint8_t*)&buf, 1);
        
        for(i=0;i<6;i++){
            write_dgus_vp(0x5417+((uint16_t)i*0x10),(uint8_t*)&buf,1);//55
        }
    } 
}
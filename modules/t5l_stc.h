#ifndef __T5L_STC_H__
#define __T5L_STC_H__

#include "sys.h"
#define T5LTOSTC_TASK_INTERVAL1 (1)
#define T5LTOSTC_TASK_INTERVAL2 (500)
//===宏
#define ON (1)
#define OFF (0)




#define LIGHT_ADD_SUB_VP 0x5350
#define LIGHT_VALUE 0x5351
#define LIGHT_NEEDLE_VP 0x5352

#define SCREEN_SAVESTA_VP 0x5354
#define SCREEN_MIN_VP 0x5356
#define VOLUME_ADD_SUB_VP 0x5358
#define VOLUME_NEEDLE_VP 0x5359


#define PASWD26_VP1 0x5370
#define PASWD26_VP2 0x5375
#define PASWD26_VP3 0x537A


// 每个设置暂用4个地址8字节   1地址:开关状态 2地址:运行状态 3地址:档位 4地址:间隔时长
#define OUTWIND_VP (0x5100)
#define INWIND_VP (0x5104)
#define UVC_VP (0x5108)
#define UVB_VP (0x510C)
#define MIST_VP (0x5110)
#define LIGHT_VP (0x5114)
#define HEATER_VP (0x5118)
#define IR_VP (0x511C)
#define PLASMA_VP (0x5120)
#define ANION_VP (0x5124)
#define LOCK_VP (0x5128)

/* V851/DGUS report VPs for the mapped actuator state. */
#define T5L_STC_REPORT_EXHAUST_VP       0x301A
#define T5L_STC_REPORT_LIGHT_VP         0x301F
#define T5L_STC_REPORT_UVB_VP           0x3024
#define T5L_STC_REPORT_ANION_VP         0x3029
#define T5L_STC_REPORT_PLASMA_VP        0x302E
#define T5L_STC_REPORT_CLIMATE_VP       0x3033
#define T5L_STC_REPORT_HUMIDIFIER_VP    0x3038
#define T5L_STC_REPORT_INLET_FAN_VP     0x303D
#define T5L_STC_REPORT_FILTER_VP        0x3042

#define T5L_STC_MAPPED_FIELD_ENABLED    0x01U
#define T5L_STC_MAPPED_FIELD_SECONDARY  0x02U
#define T5L_STC_MAPPED_FIELD_TERTIARY   0x04U

typedef enum
{
    T5L_STC_MAPPED_EXHAUST = 0,
    T5L_STC_MAPPED_LIGHT,
    T5L_STC_MAPPED_UVB,
    T5L_STC_MAPPED_ANION,
    T5L_STC_MAPPED_PLASMA,
    T5L_STC_MAPPED_CLIMATE,
    T5L_STC_MAPPED_HUMIDIFIER,
    T5L_STC_MAPPED_INLET_FAN,
    T5L_STC_MAPPED_CONTROL_COUNT
} T5lStcMappedControl;

#define DATALEN (64)
#define MAIN_ADDR 0x4000U
#define BACK_ADDR 0x10000U

#define MAIN_DATA_ADDR 0x4800U
#define BACK_DATA_ADDR 0x10800U

#define PASSWD "0000"
#define PASSWD_BTYELEN 4


#define OUTWIND_AUTO_RUN_TIME_SEC 600    //排风运行时间秒600s（10-15min）
//////////////////////////////////////////////////
#define OUTWIND_CMDWORD 0x6100
#define INWIND_CMDWORD 0x6101
#define INWIND2_CMDWORD 0x6102
#define LIGHT_CMDWORD 0x6103
#define IR_CMDWORD 0x6104
#define UVC_CMDWORD 0x6105
#define UVB_CMDWORD 0x6106
#define HEATER_CMDWORD 0x6107
// #define OUTWIND_CMDWORD 0x6108
#define ANION_CMDWORD 0x6109
#define PLASMA_CMDWORD 0x610A
#define HUMIDIFIER_CMDWORD 0x610B
#define LOCK_CMDWORD 0x610C


#define SM_FAN_CMDWORD 0x610D
#define MIC_LED_CMDWORD 0x610E
#define CAMERA_LED_CMDWORD 0x610F
//////////////////////////////////////////////////////////////
typedef enum
{
    Type_OutWind = 1, // 排风
    Type_InWind,      // 进风
    Type_UVB,         // 中波段紫外
    Type_MIST,        // 雾化
    Type_LIGHT,       // 灯光
    Type_HEATER,      // 加热
    Type_UVC,         // 短波段紫外
    Type_IR,          // 红外
    Type_PLASMA1,     //等离子
    Type_ANION,        //负离子
    Type_LOCK,        //锁
} DEV_TYPE;

//==================== 枚举定义（可选，规范档位/定时选项）====================
//时长选项：0.5/1/2/4/6/8h
#define OutWind_TIMER_0_5H (0x1)
#define OutWind_TIMER_1H (0x2)
#define OutWind_TIMER_2H (0x3)
#define OutWind_TIMER_4H (0x4)
#define OutWind_TIMER_8H (0x5)

#define UVB_TIMER_2H (0x1)
#define UVB_TIMER_4H (0x2)
#define UVB_TIMER_6H (0x3)
#define UVB_TIMER_8H (0x4)

#define MIST_RUN_TIMER_0_5H (0x1)
#define MIST_RUN_TIMER_1H (0x2)
#define MIST_RUN_TIMER_2H (0x3)

#define MIST_INTERVAL_TIMER_2H (0x1)
#define MIST_INTERVAL_TIMER_4H (0x2)
#define MIST_INTERVAL_TIMER_8H (0x3)
#define MIST_INTERVAL_TIMER_12H (0x4)

#define UVC_TIMER_15M (0x1)
#define UVC_TIMER_30M (0x2)
#define UVC_TIMER_60M (0x3)

// UVC消杀定时
typedef enum
{
    UVC_TIMER_15MIN = 1,
    UVC_TIMER_30MIN,
    UVC_TIMER_1H,
} UvcTimerSel_t;

//==================== 各模块子结构体 ====================
// 排风模块
typedef struct
{
    uint16_t enable; // 总开关 0关1开

    uint16_t speed;           // 风速 1~6档
    uint16_t interval_time_h; // 定时间隔 1:0.5h  2:1h  3:2h  4:4h  5:8h

    uint16_t target_speed;      // 风速 1~6档
    uint32_t interval_time_sec; // 定时时长（秒）

    uint8_t running_min;       // 单次自动通风时长 固定10分钟（参数固化，可保留配置位）
    uint32_t running_time_sec; // 定时时长（秒）

    uint8_t err_sta;  //0正常  1堵转
    uint8_t auto_vent_en; //自动通风使能 0关闭  1开启
    uint8_t run_status;   // 1关闭运行 2开启运行
    uint8_t running;
} St_Exhaust;

// 进风模块（预留负离子替换风扇）
typedef struct
{
    uint16_t enable; // 手动使能（加热优先逻辑会屏蔽手动关闭）0关1开
    uint16_t speed;  // 风速 1~4档
    uint16_t off_delay_sec;     //延时n秒后关闭
    uint16_t target_speed;  // 风速 1~4档
    uint8_t anion_auto_en; // 负离子开启自动小风使能
    uint8_t plasma_auto_en; // 等离子开启自动小风使能
    uint8_t heater_auto_en; // 加热自动大风使能
    uint8_t run_status;     // 1关闭运行 2开启运行
    uint8_t auto_force_run; // 自动强制开启标志【重点】加热开启时置1，禁止手动关闭
    uint8_t running;
    uint8_t err_sta;
} St_InWind;

// 加热模块
typedef struct
{
    uint16_t enable;             // 加热总开关0关1开
    uint16_t set_temp;           // 设置温度 20~35 ℃
    uint16_t target_temp;           // 目标温度 20~35 ℃
    int16_t real_temp;          // 传感器实时温度

    uint8_t err_sta;  //0正常  1传感器 或加热棒异常
    uint8_t aotu_heater_en;
    uint8_t run_status; // 1关闭运行 2开启运行
    uint8_t running;
} St_Heater;

// 雾化模块
typedef struct
{
    uint16_t enable;         // 雾化开关 0关1开
    uint16_t running_time_h; // 单次运行时长 1:0.5h  2:1h  3:2h
    uint16_t interval_time_h; // 循环间隔 1:2h  2:4h  3:8h  4:12h

    uint32_t running_time_s; // 单次运行时长s
    uint32_t interval_time_s;     // 循环间隔时长s

    uint8_t err_sta;  //0正常  1传感器 或加热棒异常
    uint8_t auto_mist_en;       // 自动加湿使能
    uint8_t run_status;         // 运行状态  1待机 2运行
    uint8_t running;
    uint8_t liquid_status;   // 液位状态  0 = 有水，1 = 缺水，2 = 没水

    uint16_t hour;  //720/1440/2160
    uint16_t sec;  
    uint16_t filter_remind_month;//滤芯更换月份 1=1month  2=2month  3=3month
    uint8_t filter_life_percense;//滤芯
    uint16_t filter_need_replace;//0无，1滤芯更换提醒
    uint8_t save_time_flag;
} St_Mist;

// 照明模块
typedef struct
{
    uint16_t enable;     // 照明总开关  0关1开
    uint16_t brightness; // 1弱光/2正常/3强光/4强光
    uint16_t target_brightness;
    uint8_t run_status; // 1关闭运行 2开启运行
    uint8_t running;
} St_Light;

// UVC紫外线消杀
typedef struct
{
    uint16_t enable;    // UVC紫外线消杀总开关0关1开
    uint16_t target_enable;    // UVC紫外线消杀总开关0关1开
    uint16_t running_time_h;   // 单次运行时长h
    uint32_t running_time_s;

    uint8_t remind_flag; //1闭锁允许  2开锁禁止
    uint8_t lock_enable;  //1允许  0禁止
    uint8_t run_status; // 1关闭运行 2开启运行
    uint8_t running;
} St_Uvc;

// 红外线模块
typedef struct
{
    uint16_t enable;             // 红外线总开关0关1开
    uint16_t light_auto_ctrl_en; // 光线明暗自动开关使能
    uint8_t cam_lrcat_sync_en;   // 开启红外同步开启摄像头LRcat模式
    uint8_t run_status; // 1关闭运行 2开启运行

} St_IR;

// UVB模块
typedef struct
{
    uint16_t enable; // UVB总开关
    uint16_t brightness; // 1~4档亮度
    uint16_t running_time_h;   // 单次运行时长h 1:2h  2:4h  3:6h  4:8h
    uint16_t interval_time_h;     // 循环间隔时长h 24-running_time_h

    uint16_t target_brightness; // 1~4档亮度
    uint32_t running_time_s; // 单次运行时长s
    uint32_t interval_time_s;     // 循环间隔时长s

    uint8_t auto_UVB_en; 
    uint8_t run_status;     // 1关闭运行 2开启运行
    uint8_t running;
} St_Uvb;

//显示
typedef struct
{
    uint8_t ligth_value;
    uint8_t off_display_flag; //高4位=1 进入屏保&息屏    (低4位=1屏保  =2息屏 =3常亮不变)
    uint16_t off_display_time_min;
    uint16_t off_display_time_s;

    uint16_t temp_uint;   //温度单位 1=°C   2=°F
    uint16_t volume;      //音量
    uint16_t screen_save; // 1屏保  2息屏 3无
    uint16_t language; //1中文  2英文  3韩文  4日语
    uint8_t passwd[7];
    uint8_t save_flag;
    uint16_t save_interval_times;
} SYS_CONFIG;

//等离子
typedef struct
{
    uint16_t enable;
    uint8_t running;
} ST_COM;

//负离子
typedef struct _nai
{
    uint16_t enable;
    uint8_t running;
} ST_ANION;

//锁
typedef struct _lock
{
    uint16_t enable;
} ST_LOCK;

//==================== 传感器汇总结构体 ====================
typedef struct
{
    int16_t temperature;          // 舱内温度
    uint16_t humidity;            // 舱内湿度
    int16_t temperaturex10;          // 舱内温度

    uint8_t  GXHTC3_err_sta;//传感器故障
    uint8_t  NTC_err_sta;//传感器故障
} St_Sensor;

//==================== 全局联动标志（业务逻辑核心）====================
typedef struct
{
    uint8_t heater_priority;   // 加热优先级标志，加热开启=1，离子/等离子跟随加热进风策略
    uint8_t ion_plasma_enable; // 等离子/离子功能总开关（进风联动使用）
} St_LinkFlag;

//==================== 整机总控制结构体 ====================
typedef struct
{
    St_Exhaust Exhaust;
    St_InWind Inlet_Fan;
    St_Heater Heater;
    St_Mist Humidifier;
    St_Light Light;
    St_Uvc UVC;
    St_Uvb UVB;
    St_IR IR;
    ST_COM Plasma;
    ST_COM Anion;
    ST_LOCK Lock;
    ST_COM Small_Fan;
    ST_COM MicLed;
    ST_COM CameraLed;
    St_Sensor environment;
    St_LinkFlag link_flag; // 模块联动控制标志位
    SYS_CONFIG Cfg;
    uint8_t dev_enable;
} DeviceCtrl;

extern DeviceCtrl G_Device_Ctrl;

uint8_t T5lStcSyncMappedControl(T5lStcMappedControl control,
                                uint8_t field_mask,
                                uint16_t enabled,
                                uint16_t secondary,
                                uint16_t tertiary);
uint8_t T5lStcSyncLocalMappedControl(T5lStcMappedControl control,
                                     uint8_t field_mask,
                                     uint16_t enabled,
                                     uint16_t secondary,
                                     uint16_t tertiary);

extern void T5l_Stc_Init(void);
extern void Queue_Time_Check(void);
extern void T5L_Stc_Poll(void);
extern void key_scanf(void);
extern void Sys_Cfg(void);
extern void SysCfg_Init();
extern void Dev_Time_Check();
void Dev_Aotu_Procese();
void Flash_DataInit(void);
void Flash_SaveData();
void Start_Once_SaveData();
void T5l_Stc_UartRxProcess(uint16_t addr);
void STC_ReporData_Procese(uint32_t addr);
uint8_t check_passwd_format(uint8_t *u8buf );


void Exhaust_On();
void Exhaust_Off(uint8_t auto_flag);
void InWind_On(uint16_t speed);
void InWind_Off();
void Heater_On(int16_t target_tmp);
void Heater_Off(uint8_t auto_flag);
void Light_On(uint16_t target_brightness);
void Light_Off();
void UVC_On();
void UVC_Off();
void UVB_On(uint16_t target_brightness);
void UVB_Off(uint8_t auto_flag);
void Humidifier_On();
void Humidifier_Off(uint8_t auto_flag);
// void Anion_On(void);
// void Anion_Off(void);
// void Plasma_On(void);
// void Plasma_Off(void);


// void SMALL_FAN_On();
// void SMALL_FAN_Off();
// void MicLed_On();
// void MicLed_Off();
// void CameraLed_On();
// void CameraLed_Off();

void Dev_Switch_Ctrl(uint16_t *pEnable, uint16_t cmdWord, uint8_t bOn);



void Mult_Task();
void PageFunction(void);

void Set_Temperature_Unit(uint16_t Unit);
void Language_Init(uint16_t language_tpye);

#endif

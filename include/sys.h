
/**
 * @file sys.h
 * @brief T5L系统核心功能头文件
 * @details 提供系统任务管理、延时、CRC计算、DGUS通信等核心功能接口
 * @author yangming
 * @version 1.0.0
 */

#ifndef SYS_H
#define SYS_H

#include "T5LOSConfig.h"

/* DGUS系统变量定义 */

/* 当前显示页面ID */
#define sysDGUS_PIC_NOW             0x0014 

/**
 * D3：0x5A 表示启动一次页面处理，CPU 处理完清零。 
 * D2：处理模式:0x01=页面切换（把图片存储区指定的图片显示到当前背景页面）。 
 * D1:D0：图片 ID。
 */
#define sysDGUS_PIC_SET             0x0084  

/**
 * D3：用户写入 0x5A 启动一次系统参数配置，CPU 处理完清零。 
 * D2：触摸屏灵敏度配置值，只读。 
 * D1：触摸屏模式配置值，只读。 
 * D0：系统状态设置。 
 * .7：串口 CRC 校验设置，1=开启，0=关闭，只读。
 * .6：保留，写 0。
 * .5：上电加载 22 文件初始化变量空间 1=加载 0=不加载，只读。
 * .4：变量自动上传设置 1=开启，0=关闭，读写。
 * .3：触摸屏伴音控制 1=开启 0=关闭，读写。
 * .2：触摸屏背光待机控制 1=开启 0=关闭，读写。
 * .1-.0：显示方向 00=0° 01=90° 10=180° 11=270°，读写。
 */
#define sysDGUS_SYSTEM_CONFIG       0x0081

/**
 * 
 * D7:0x5A 表示触摸屏数据已经更新。 
 * D6:触摸屏状态 0x00=松开 0x01=第一次按压 0x02=抬起 0x03=按压中 
 * D5:D4=X 坐标 D3:D2=Y 坐标 D1:D0=0x0000。
 */
#define sysDGUS_TP_STATUS           0x0016

#define sysWAE_PLAY_ADDR            0x00A0

/**
 * D7：操作模式 0x5A=读 0xA5=写，CPU 操作完清零。 
 * D6:4：片内 Nor Flash 数据库首地址，必须是偶数，0x000000-0x03:FFFE，256KWords。
 * D3:2：数据变量空间首地址，必须是偶数。 
 * D1:0：读写字长度，必须是偶数。
 */
#define sysDGUS_FLASH_RW_CMD                0x0008
#define flashMAIN_BLOCK_ORDER              (uint8_t )0                            /**< 主块序号，每一块有32个扇区 */
#if flashDUAL_BACKUP_ENABLED
#define flashDGUS_COPY_VP_ADDRESS           0x1000                      /**< DGUS VP地址 */
#define FLASH_COPY_ONCE_SIZE                0x800                          /**< 每次复制的Flash数据块大小，4k字节数据，2k字数据 */
#define FLASH_COPY_MAX_SECTOR                 32                              /**< 最大复制扇区个数 */
#define flashBACKUP_BLOCK_ORDER             (uint8_t )2                            /**< 用作双备份块的序号 */
#define flashBACKUP_FLAG_ADDRESS            0x0000                      /**< 双备份改动标志缓存地址 */
#define flashBACKUP_FLAG_DEFAULT_VALUE      (uint16_t )0x5aa5      /**< 双备份改动标志默认值 */
#define flashBACKUP_DGUS_CACHE_ADDRESS      0xF000                      /**< 双备份缓存地址 */
#endif /* flashDUAL_BACKUP_ENABLED */


/* ADC通道数据VP地址定义 */
#define sysDGUS_ADC_CHANNEL0        0x0032  /**< ADC通道0数据 */
#define sysDGUS_ADC_CHANNEL1        0x0033  /**< ADC通道1数据 */
#define sysDGUS_ADC_CHANNEL2        0x0034  /**< ADC通道2数据 */
#define sysDGUS_ADC_CHANNEL3        0x0035  /**< ADC通道3数据 */
#define sysDGUS_ADC_CHANNEL4        0x0036  /**< ADC通道4数据 */
#define sysDGUS_ADC_CHANNEL5        0x0037  /**< ADC通道5数据 */
#define sysDGUS_ADC_CHANNEL6        0x0038  /**< ADC通道6数据 */
#define sysDGUS_ADC_CHANNEL7        0x0039  /**< ADC通道7数据 */
#define sysDGUS_ADC_CHANNEL_COUNT   (uint16_t )8       /**< ADC通道数量 */
#define sysDGUS_ADC_INTERVAL        100     /**< ADC采样间隔，单位为毫秒 */
#define sysDGUS_ADC_AVERAGE_COUNT   (uint16_t )20      /**< ADC采样平均次数 */



/**
 * @brief 系统任务定义结构体
 * @details 定义系统任务的基本属性，包括任务ID、执行间隔、计数器和函数指针
 */
typedef struct TaskDefine
{
    uint8_t taskID;                 /**< 任务唯一标识符 */
    uint16_t taskInterval;          /**< 任务执行间隔时间，单位为系统时钟节拍 */
    uint16_t taskCounter;           /**< 任务执行计数器，用于实现定时执行 */
    void (*taskFunction)(void);     /**< 任务执行函数指针 */
} SysTask;

/**
 * @brief 页面状态结构体
 * @details 用于维护每个页面的独立状态信息
 */
typedef struct {
    uint16_t last_exit_page_id;     /**< 最后离开的页面ID */
    uint16_t last_target_page_id;   /**< 上次的目标页面ID */
} PageState;

#define dgusMAX_MONITORED_PAGES 2


/**
 * @brief 计数任务执行间隔定义
 * @details 定义计数任务的执行周期，单位为毫秒
 */
#define COUNT_TASK_INTERVAL 1000

/**
 * @brief 计数任务函数
 * @details 执行计数操作并更新DGUS显示的任务函数
 */
void CountTask(void);

/**
 * @brief 添加系统任务
 * @details 向系统任务调度器中添加一个新的任务
 * @param[in] taskID 任务唯一标识符
 * @param[in] taskInterval 任务执行间隔时间，单位为系统时钟节拍
 * @param[in] taskFunction 任务执行函数指针
 */
void SysTaskAdd(uint8_t taskID, uint16_t taskInterval, void (*taskFunction)(void));

/**
 * @brief 移除系统任务
 * @details 从系统任务调度器中移除指定ID的任务
 * @param[in] taskID 要移除的任务标识符
 */
void SysTaskRemove(uint8_t taskID);

/**
 * @brief 运行系统任务调度器
 * @details 执行任务调度逻辑，检查并运行到期的任务
 */
void SysTaskRun(void);

/**
 * @brief 微秒级延时函数
 * @details 提供精确的微秒级别延时功能
 * @param[in] us 延时时间，单位为微秒
 */
void delay_us(uint16_t us);

/**
 * @brief 毫秒级延时函数
 * @details 提供毫秒级别延时功能
 * @param[in] ms 延时时间，单位为毫秒
 */
void delay_ms(uint16_t ms);

/**
 * @brief CRC-16校验计算函数
 * @details 计算数据缓冲区的CRC-16校验值，采用标准多项式0xA001
 * @param[in] pBuf 指向待计算数据缓冲区的指针
 * @param[in] buf_len 数据缓冲区长度
 * @return 计算得到的CRC-16校验值
 */
uint16_t crc_16(uint8_t *pBuf, uint16_t buf_len);

/**
 * @brief 从DGUS读取VP数据
 * @details 从DGUS显示屏的指定VP地址读取数据到缓冲区
 * @param[in] addr VP地址
 * @param[out] buf 接收数据的缓冲区指针
 * @param[in] len 要读取的数据长度
 */
void read_dgus_vp(uint32_t addr, uint8_t *buf, uint8_t len);

/**
 * @brief 向DGUS写入VP数据
 * @details 将缓冲区数据写入到DGUS显示屏的指定VP地址
 * @param[in] addr VP地址
 * @param[in] buf 待写入数据的缓冲区指针
 * @param[in] len 要写入的数据长度
 */
void write_dgus_vp(uint32_t addr, uint8_t *buf, uint16_t len);
//void write_dgus_vp(uint32_t addr, uint8_t *buf, uint8_t len);

/**
 * @brief 将数值转换为ASCII字符并复制到指定数组
 * @param[in] arr 目标数组
 * @param[in] value 要转换的数值
 * @param[in] start 复制起始位置
 * @return 复制结束位置
 */
uint16_t CopyAsciiValue(uint8_t *arr,uint16_t value,uint16_t start); 


/**
 * @brief 将ASCII字符串复制到指定数组
 * @param[in] arr 目标数组
 * @param[in] ascii 源ASCII字符串
 * @param[in] start 复制起始位置
 * @return 复制结束位置
 */
uint16_t CopyAsciiString(uint8_t *arr,uint8_t *ascii,uint16_t start); 


/**
 * @brief 读取当前页面ID
 * @details 从DGUS显示屏读取当前显示的页面ID
 * @return 当前页面ID
 */
uint16_t ReadPageId(void);


/**
 * @brief 切换到指定页面ID
 * @details 向DGUS发送命令切换到指定的页面ID
 * @param[in] page_id 要切换到的页面ID
 * @return 无
 */
void SwitchPageById(uint16_t page_id);


#if sysDGUS_AUTO_UPLOAD_ENABLED || uartTA_PROTOCOL_ENABLED
/**
 * @brief DGUS自动上传功能
 */
void DgusAutoUpload(void);
#endif /* sysDGUS_AUTO_UPLOAD_ENABLED || uartTA_PROTOCOL_ENABLED */


/**
 * @brief DGUS页面扫描任务
 * @details 定期扫描DGUS显示屏的页面ID并进行处理
 * @note 该任务会在系统任务调度中周期性执行，建议执行周期30ms
 * @note 该任务会根据当前页面是否是目标页面ID执行不同的操作
 * @warning 定义的页面切换数量不能超过dgusMAX_MONITORED_PAGES
 */
void DgusPageScanTask(void);


/**
 * @brief DGUS键值扫描任务
 * @details 定期扫描DGUS显示屏的键值并进行处理
 * @note 根据对应地址的对应键值进行操作
 * @note 该任务会在系统任务调度中周期性执行，建议执行周期30ms
 * 
 */
void DgusValueScanTask(void);


/* flash相关操作 */

#define flashWRITE_FLAG 0xA5 /**< Flash写操作标志 */
#define flashREAD_FLAG  0x5A /**< Flash读操作标志 */

/**
 * @brief T5L NOR Flash读写操作宏定义
 * @details dgusToFlash和FlashToDgus宏用于简化Flash与DGUS VP之间的数据传输操作
 * @details dgustoflashwithdata和flashtodguswithdata宏用于带数据缓冲区的读写操作
 */


#define FlashToDgus(flash_block,flash_addr,dgus_vp_addr,len) \
    T5lNorFlashRW(flashREAD_FLAG, flash_block, flash_addr, dgus_vp_addr, NULL, len)

#if flashDUAL_BACKUP_ENABLED
#define DgusToFlash(flash_block,flash_addr,dgus_vp_addr,len) \
    do{                                                                                     \
        T5lNorFlashRW(flashWRITE_FLAG, flash_block, flash_addr, dgus_vp_addr, NULL, len);              \
        T5lNorFlashRW(flashWRITE_FLAG, flashBACKUP_BLOCK_ORDER, flash_addr, dgus_vp_addr, NULL, len);  \
    }while(0);
#else
#define DgusToFlash(flash_block,flash_addr,dgus_vp_addr,len) \
    T5lNorFlashRW(flashWRITE_FLAG, flash_block, flash_addr, dgus_vp_addr, NULL, len)
#endif /* flashDUAL_BACKUP_ENABLED */

#define FlashToDgusWithData(flash_block,flash_addr,dgus_vp_addr,data_buf,len) \
    T5lNorFlashRW(flashREAD_FLAG, flash_block, flash_addr, dgus_vp_addr, data_buf, len)

#if flashDUAL_BACKUP_ENABLED
#define DgusToFlashWithData(flash_block,flash_addr,dgus_vp_addr,data_buf,len) \
    do{                                                                                         \
        T5lNorFlashRW(flashWRITE_FLAG, flash_block, flash_addr, dgus_vp_addr, data_buf, len);              \
        T5lNorFlashRW(flashWRITE_FLAG, flashBACKUP_BLOCK_ORDER, flash_addr, dgus_vp_addr, data_buf, len);  \
    }while(0);
#else
#define DgusToFlashWithData(flash_block,flash_addr,dgus_vp_addr,data_buf,len) \
    T5lNorFlashRW(flashWRITE_FLAG, flash_block, flash_addr, dgus_vp_addr, data_buf, len)
#endif /* flashDUAL_BACKUP_ENABLED */

/**
 * @brief T5L NOR Flash读写操作
 * @param[in] RWFlag 读写标志，0x5a表示读，0xa5表示写
 * @param[in] flash_block Flash块号 (0-3)
 * @param[in] flash_addr Flash地址 (0x0000-0xFFFF)
 * @param[in] dgus_vp_addr DGUS VP地址 (0x0000-0xFFFF)
 * @param[in,out] data_buf 数据缓冲区指针，用于读写数据暂存区
 * @param[in] len 数据长度
 * @warning flash_addr,以双字节为单位，必须是偶数
 * @warning dgus_vp_addr和len必须是偶数,以双字节为单位
 * @warning data_buf缓冲区必须足够大以容纳len字节数据
 */
void T5lNorFlashRW(uint8_t RWFlag,
                  uint8_t flash_block,
                  uint16_t flash_addr,
                  uint16_t dgus_vp_addr,
                  uint8_t *data_buf,
                  uint16_t len);


#if flashDUAL_BACKUP_ENABLED

/**
 * @brief T5L NOR Flash初始化函数
 * @details 初始化Flash块，设置双备份标志和数据缓冲区
 * @warning 4k字节为写入的最小单位，在双备份开启时，使用每一个扇区的开始的0x0000和0x0001作为双备份标志位
 * @note 初始化时间大约为5s
 */
void T5lNorFlashInit(void);
#endif /* flashDUAL_BACKUP_ENABLED */


/**
 * @brief ADC任务
 * @details 定期读取ADC通道的值并进行处理
 * @note 该任务会在系统任务调度中周期性执行，建议执行周期为sysDGUS_ADC_INTERVAL
 */
void AdcTask(void);


#if sysDGUS_CHART_ENABLED
/**
 * @brief 写入单个图表数据点
 * @details 将指定图表ID和数据点数写入DGUS显示屏
 * @param[in] chart_id 图表ID
 * @param[in] point_num 数据点数量
 * @param[in] data_buf 数据缓冲区指针，包含要写入的数据点
 * @note data_buf的长度必须为point_num * 2 + 1
 */
void SysWriteSingleChart(uint8_t chart_id, uint8_t point_num, uint8_t *data_buf);
#endif /* sysDGUS_CHART_ENABLED */


/**
 * @brief T5L CPU初始化函数
 * @details 完成T5L芯片的基本初始化，包括内核、GPIO、UART、定时器等模块
 */
void T5LCpuInit(void);

#endif /* SYS_H */

# Template4T5L

基于8051单片机的T5L嵌入式系统开发模板

## 📋 目录

- [特性](#特性)
- [移植前须知](#移植前须知)
- [文件夹结构](#文件夹结构)
- [已支持功能](#已支持功能)
- [快速开始](#快速开始)
- [贡献指南](#贡献指南)

## ✨ 特性

1. **注释完备** - 每个函数和宏定义都有详细的MISRA风格中文注释
2. **移植简单** - 修改`T5LOSConfig.h`文件即可对部分功能进行裁剪
3. **代码占用空间小** - 最小系统仅包含定时器0，变量地址和代码占用不到9K
4. **双触发模式** - 支持键值变化触发和页面切换触发两种方式
5. **任务轮询模式** - 方便进行任务的添加和删除操作，更关注业务逻辑

## ⚠️ 移植前须知

### 编码规范
- **编码格式**: 使用UTF-8编码
- **注释规范**: 禁用单行注释，统一使用`/* */`进行注释
- **命名规范**: 
  - 函数采用驼峰命名法 (如: `SysTaskAdd`)
  - 变量采用下划线命名法 (如: `task_counter`)

### 开发环境
- **编译器**: Keil uVision
- **目标芯片**: 8051系列单片机
- **调试工具**: 支持标准8051调试接口

## 📁 文件夹结构

```
Template4T5L/
├── docs/           # 📖 说明文档
├── include/        # 🔧 平台侧文件
│   ├── T5L/       # T5L平台相关文件
│   └── T5F/       # T5F平台相关文件
├── modules/        # 📦 模块文件
├── project/        # 🛠️ Keil工程文件
├── source/         # 📄 源文件
└── user/          # 👤 用户文件
```

## 🚀 已支持功能

### 硬件接口
- [x] **定时器** - Timer0, Timer1, Timer2
- [x] **串口通信** - UART2, UART3, UART4, UART5
- [x] **GPIO** - 通用输入输出控制
- [x] **PWM** - 脉宽调制输出
- [x] **ADC** - 模数转换器
- [x] **Flash** - 片内Flash写入和读取

### 通信协议
- [x] **I2C通信** - 主从模式支持
- [x] **ModbusRTU** 
- [x] **SPI通信** 
- [x] **CAN通信** 
- [x] **迪文8283** - 标准DGUS通信协议

### 实时时钟
- [x] **RX8130** - 高精度RTC芯片
- [x] **SD2058** - 低功耗RTC芯片

### 系统功能
- [x] **看门狗** - 系统监控和复位

### BOOT 控制字与升级重启

APP 每 10ms 轮询 VP `0x0020..0x0021` 的 4 字节大端控制字：

| 控制字 | 行为 |
| --- | --- |
| `5A A5 5A A5` | 保存控制字后软重启，由 BOOT 进入升级模式 |
| `AA 55 xx xx` | 保存控制字后软重启，由 BOOT 加载指定的 4KB 起始块 `xxxx` |
| 其他值 | 忽略，不写 Flash、不重启 |

UART4 使用现有 DGUS `0x82` 写入通道，例如升级请求为
`5A A5 07 82 00 20 5A A5 5A A5`，加载块 112 为
`5A A5 07 82 00 20 AA 55 00 70`。
控制字保存到 NOR 主块 0 的字地址 `0x0020`，长度 2 字；等待 Flash 命令完成后，
向 VP `0x0004` 写入 `55 AA 5A A5` 触发软重启。复位请求发出后不重复写 Flash。

APP 启动时仅清空 VP 中已消费的控制字，不改 NOR 中保存的启动目标，避免指定块加载后循环重启。
进入升级模式后，由 BOOT 在 recovery 结束后发送一次 `06` 就绪通知（TLV `09 00 01 01`），
上位机再发送 `06` 开始通知和 `07` 数据帧。
APP 当前 UART4 为 115200；本次配套 BOOT 的 UART4 为 921600，上位机需在重启后切换到 BOOT 波特率。

## 🏁 快速开始

### 1. 环境配置
```bash
# 克隆项目
git clone https://github.com/yanghaoshuan/Template4T5L.git

# 进入项目目录
cd Template4T5L
```

### 2. 配置系统
编辑 `include/T5L/T5LOSConfig.h` 文件，根据需求配置功能模块：

```c
/* 系统配置示例 */
#define sysMAX_TASK_NUM         8           /* 最大任务数量 */
#define sysFOSC                 206438400UL /* 系统时钟频率 */

/* 功能模块使能配置 */
#define uartUART2_ENABLED       1       /* 启用UART2 */
#define gpioGPIO_ENABLE         1       /* 启用GPIO */
#define i2cI2C_ENABLED          1       /* 启用I2C */
```

### 3. 基本使用示例

```c
#include "sys.h"
#include "uart.h"
#include "gpio.h"

void main(void)
{
    /* 系统初始化 */
    T5LCpuInit();
    
    /* 添加任务 */
    SysTaskAdd(0, 1000, MyTask);  /* 每1000ms执行一次 */
    
    /* 主循环 */
    while(1)
    {
        SysTaskRun();  /* 运行任务调度器 */
    }
}

void MyTask(void)
{
    /* 用户任务代码 */
    // LED闪烁、数据采集等
}
```

## 🤝 贡献指南

我们欢迎社区贡献！请遵循以下步骤：

1. Fork 本仓库
2. 创建特性分支 (`git checkout -b feature/AmazingFeature`)
3. 提交更改 (`git commit -m 'Add some AmazingFeature'`)
4. 推送到分支 (`git push origin feature/AmazingFeature`)
5. 打开 Pull Request

### 代码规范
- 遵循现有的代码风格
- 添加必要的注释和文档
- 确保代码通过编译测试

## 📞 联系我们

- 项目主页: [Template4T5L](https://github.com/yanghaoshuan/Template4T5L)
- 提交问题: [Issues](https://github.com/yanghaoshuan/Template4T5L/issues)
- 作者：    [yanghaoshuan](https://github.com/yanghaoshuan)

## 🎉 致谢

感谢所有为本项目做出贡献的开发者！

---


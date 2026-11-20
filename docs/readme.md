# Template4T5L

基于8051单片机的T5L嵌入式系统开发模板

## V851 启动完成通知

当前广告屏业务在系统、UART、RTC、OTA 初始化和任务注册完成后，进入主循环前，
通过 `Uart_R11`（本分支为 UART5，921600/8N1）发送一次 `AA 55 00 03 F3 6D 61`。
长度 `0003` 包含命令字和两字节 CRC；CRC16 使用初值 `FFFF`、反射多项式 `A001`，
覆盖前五字节 `AA 55 00 03 F3`，结果 `616D` 按低字节在前发送。

`F3` 表示业务 APP 已启动就绪，普通上电、软件复位及 OTA 后进入 APP 都会发送。
它不表示本次 OTA 成功，升级结果仍由 bootloader 的 B8/B9 通知。
APP 校验失败或停留在 bootloader 时不会发送 F3；发送也不等待 V851 的启动握手，
不随业务周期任务重复。V851 应先监听 UART5，再触发 T5L 重启。
UART5 空闲时直接写入首字节启动发送，其余字节由真实发送完成中断续发。

## V851 时区设置（DGUS 100 → 110）

DGUS 工程位于 `D:\youngwork\zengyi_ota\GUI`。在 100 页点击“时间修改”进入
110 页，并立即发送一次当前已保存时区。选择 `+`（东区）或 `-`（西区）、输入小时数，页面显示 `UTC+08:00` 等格式，
点击保存生效。协议只包含整数小时，支持 `UTC-12:00` 至 `UTC+14:00`；
不支持半小时/四分之一小时时区。零时区统一显示为 `UTC+00:00`。

| VP | 用途 |
| --- | --- |
| `06F0..06F7` | UTC 格式显示，16 字节 ASCII |
| `06F8` | 命令：1 保存，2 取消/恢复已保存值，3 进入时间设置并发送当前时区；处理后清零 |
| `06F9` | 符号：0 东区/正，1 西区/负 |
| `06FA` | 小时数：东区 0..14，西区 0..12 |
| `06FB` | 状态：0 待设置，1 已保存待同步，2 已同步，3 输入无效 |
| `06FC..06FF` | NOR 主块 0 同地址的持久记录及内部传输缓存 |

未保存过时默认 `UTC+08:00`。保存时先写入 NOR Flash，再通过 UART5 发送无 CRC 的
`AA 55 00 03 F4 sign hour`；例如西八区为 `AA 55 00 03 F4 01 08`。
重复保存同一值只重新同步，不重复写 Flash。上电恢复保存值，在 F3 就绪通知后的
首次业务任务中发送一次；收到 V851 启动握手后也重新同步，覆盖双方独立重启的情况。
页面编辑和保存不依赖该握手。

回复按 `AA 55 00 04 F4 sign hour yy mm dd hh mm ss` 的 **13 字节**接收，
也兼容长度字段为 `00 09` 的同格式回复。所有时间字段为普通二进制数值，年份为
2000..2099 的后两位；仅时区与当前保存值匹配且日期时间有效时更新 RTC 和显示。
截断帧、无效日期、旧时区回复不更新时间。F4 分包跨串口空闲可续收。
收到回复只同步 RTC，不反向修改保存的时区。

Keil 编译后可用以下命令生成与现有 BOOT 匹配的 112 APP 字库（先备份旧文件）：

```powershell
python project/pack_app.py project/Objects/T5L51.bin D:/youngwork/zengyi_ota/GUI/DWIN_SET/DWIN_SET/112_t5l51.lib
```

该工具将 APP 补零到 `0xFF00`，在 `0xFF00` 写入低字节在前的 CRC16，再补至 64 KiB，
并验证 BOOT 使用的整段 CRC 为 0。页面资源与固件需配套更新。

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


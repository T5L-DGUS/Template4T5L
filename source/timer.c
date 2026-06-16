/**
 * @file    timer.c
 * @brief   定时器驱动程序实现文件
 * @details 本文件实现了系统定时器的完整功能，包括硬件配置、中断处理
 *          和系统时钟节拍维护
 * @author  yangming
 * @version 1.0.0
 */

#include "timer.h"
#include "uart.h"

#if gpioGPIO_ENABLE
#include "gpio.h"
#endif /* gpioGPIO_ENABLE */

/**
 * @brief 系统任务定时器节拍计数器定义
 */
uint16_t SysTaskTimerTick = 0;

uint32_t SysCurrentTick = 0;

#if gpioPWM_ENABLE && gpioGPIO_ENABLE
uint16_t gpio_count_pwm = 0;  /**< PWM高电平计数 */
#endif /* gpioPWM_ENABLE && gpioGPIO_ENABLE */


/**
 * @brief 启动定时器0函数
 * @details 配置并启动定时器0，用于产生系统时钟节拍，根据使能情况启动对应定时器
 * @param 无
 * @return 无
 * @post 定时器0已启动并开始产生周期性中断，根据使能情况启动对应定时器
 */

void TimerInit(void)
{
    TCON |= 0x0F;
    #if timeTIMER1_ENABLED
    TMOD =  timeT1_GATE_DISABLE | timeT1_MODE_TIMER | timeT1_MODE_16BIT | 
            timeT0_GATE_DISABLE | timeT0_MODE_TIMER | timeT0_MODE_16BIT; 
    #else
    TMOD =  timeT0_GATE_DISABLE | timeT0_MODE_TIMER | timeT0_MODE_16BIT;
    #endif  /* timeTIMER1_ENABLED */
    
	TH0   = sysDEFAULT_ZERO;
	TL0   = sysDEFAULT_ZERO;
	TR0   = sysDEFAULT_ZERO;

    #if timeTIMER1_ENABLED
    TH1   = sysDEFAULT_ZERO;
    TL1   = sysDEFAULT_ZERO;
    TR1   = sysDEFAULT_ZERO;
    #endif /* timeTIMER1_ENABLED */

    #if timeTIMER2_ENABLED
    T2CON = (timeT2_FREQ_MODE >> 1) << 7 | 0xF0;
    TH2 = sysDEFAULT_ZERO;
    TL2 = sysDEFAULT_ZERO;
    #endif /* timeTIMER2_ENABLED */

    TH0 = timeT0_TICK >> 8;
    TL0 = timeT0_TICK & 0xFF;
    ET0 = 1;       
    TR0 = 1; 

    #if timeTIMER1_ENABLED
    TH1 = timeT1_TICK >> 8;
    TL1 = timeT1_TICK & 0xFF;
    ET1 = 1;       
    TR1 = 1;
    #endif /* timeTIMER1_ENABLED */

    #if timeTIMER2_ENABLED
    TRL2H = timeT2_TICK >> 8;
    TRL2L = timeT2_TICK & 0xFF;
    T2CON |= 0x01;
    ET2 = 1;
    #endif /* timeTIMER2_ENABLED */
}



/**
 * @brief 定时器0中断服务程序
 * @details 处理定时器0中断，维护系统时钟节拍和UART超时计数
 * @note 中断号1，周期性执行系统维护任务
 * @note 自动更新所有启用UART的接收超时计数器
 * @note 维护系统任务调度时钟节拍
 */
void Timer0Isr() interrupt 1
{
    ET0 = 0;
    TH0 = timeT0_TICK >> 8;
    TL0 = timeT0_TICK & 0xFF;

	#if uartUART2_ENABLED
		if(Uart2.RxTimeout)
			Uart2.RxTimeout--;
    #endif /* uartUART2_ENABLED */

    #if uartUART3_ENABLED
		if(Uart3.RxTimeout)
			Uart3.RxTimeout--;
    #endif /* uartUART3_ENABLED */

    #if uartUART4_ENABLED
		if(Uart4.RxTimeout)
			Uart4.RxTimeout--;
    #endif /* uartUART4_ENABLED */

    #if uartUART5_ENABLED
		if(Uart5.RxTimeout)
			Uart5.RxTimeout--;
    #endif /* uartUART5_ENABLED */

    SysTaskTimerTick++;

    SysCurrentTick++;
    ET0 = 1;
}


/* 定时器1支持 - 条件编译 */
#if timeTIMER1_ENABLED
/**
 * @brief 定时器1中断服务程序
 * @details 处理定时器1中断，执行用户自定义的定时任务
 * @note 中断号3
 */
void Timer1Isr() interrupt 3
{
    ET1 = 0;
    TH1 = timeT1_TICK >> 8;
    TL1 = timeT1_TICK & 0xFF;

    /* use timer1*/
    __NOP();
    ET1 = 1;
}
#endif /* timeTIMER1_ENABLED */


/* 定时器2支持 - 条件编译 */
#if timeTIMER2_ENABLED
/**
 * @brief 定时器2中断服务程序
 * @details 处理定时器2中断，执行用户自定义的定时任务
 * @note 中断号5
 */
void Timer2Isr() interrupt 5
{
    TF2 = 0;

    /* use timer2 */
    __NOP();
    #if gpioPWM_ENABLE && gpioGPIO_ENABLE
    {
        if (gpio_count_pwm < GPIO_PWM_DUTY_CYCLE)
        {
            GPIO_PWM_PORT = 1;
        }
        else
        {
            GPIO_PWM_PORT = 0; 
        }
        gpio_count_pwm++;
        if (gpio_count_pwm >= GPIO_PWM_MAX_DUTY)
        {
            gpio_count_pwm = 0; /* 重置计数器 */
        }
    }
    #endif /* gpioPWM_ENABLE && gpioGPIO_ENABLE */
    
}
#endif /* timeTIMER2_ENABLED */
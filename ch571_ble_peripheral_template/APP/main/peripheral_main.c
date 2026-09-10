/********************************** (C) COPYRIGHT *******************************
 * File Name          : main.c
 * Author             : WCH
 * Version            : V1.1
 * Date               : 2020/08/06
 * Description        : 外设从机应用主函数及任务系统初始化
 *********************************************************************************
 * Copyright (c) 2021 Nanjing Qinheng Microelectronics Co., Ltd.
 * Attention: This software (modified or not) and binary are used for 
 * microcontroller manufactured by Nanjing Qinheng Microelectronics.
 *******************************************************************************/

/******************************************************************************/
/* 头文件包含 */
#include "CONFIG.h"
#include "HAL.h"
#include "gattprofile.h"
#include "peripheral.h"
#include "stdio_redirect.h"
#include "apl_shell/letter_shell_app.h"
#include <stdio.h>

/*********************************************************************
 * GLOBAL TYPEDEFS
 */
__attribute__((aligned(4))) uint32_t MEM_BUF[BLE_MEMHEAP_SIZE / 4];

#if(defined(BLE_MAC)) && (BLE_MAC == TRUE)
const uint8_t MacAddr[6] = {0x84, 0xC2, 0xE4, 0x03, 0x02, 0x02};
#endif

/*********************************************************************
 * @fn      Main_Circulation
 *
 * @brief   主循环
 *
 * @return  none
 */
__attribute__((section(".highcode")))
__attribute__((noinline))
void Main_Circulation()
{
    while(1)
    {
        TMOS_SystemProcess();
    }
}

/*********************************************************************
 * @fn      hardware_init
 *
 * @brief   硬件初始化函数
 *
 * @return  none
 */
static void hardware_init(void)
{
#if(defined(DCDC_ENABLE)) && (DCDC_ENABLE == TRUE)
    PWR_DCDCCfg(ENABLE);
#endif
    SetSysClock(CLK_SOURCE_PLL_60MHz);
    setvbuf(stdout, NULL, _IONBF, 0);
#if(defined(HAL_SLEEP)) && (HAL_SLEEP == TRUE)
    GPIOA_ModeCfg(GPIO_Pin_All, GPIO_ModeIN_PU);
    GPIOB_ModeCfg(GPIO_Pin_All, GPIO_ModeIN_PU);
#endif
    Stdio_RedirectInit();
    printf("%s\n", VER_LIB);
}

/*********************************************************************
 * @fn      tasks_init
 *
 * @brief   任务初始化函数
 *
 * @return  none
 */
static void tasks_init(void)
{
    CH57X_BLEInit();
    HAL_Init();
    GAPRole_PeripheralInit();
    Peripheral_Init();
#if BOARD_CFG_SHELL_ENABLE == 1
    ShellInit();
#endif
}

/*********************************************************************
 * @fn      main
 *
 * @brief   主函数
 *
 * @return  none
 */
int main(void)
{
    hardware_init();
    tasks_init();
    Main_Circulation();
}

/******************************** endfile @ main ******************************/

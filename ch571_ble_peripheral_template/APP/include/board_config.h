/********************************** (C) COPYRIGHT *******************************
 * File Name          : board_config.h
 * Description        : Board-specific build options（板级唯一配置 BOARD_CFG_ 前缀，按功能分层）。
 *******************************************************************************/

#ifndef BOARD_CONFIG_H
#define BOARD_CONFIG_H

#include <stdint.h>         /* 基础类型（供依赖配置头的模块使用） */

/* ============================ Release 标准宏 ============================ */
/* 统一 release 判定：编译期定义 NDEBUG（-DNDEBUG）即为 release；
 * 也可手工把 BOARD_CFG_RELEASE 置 1。release 用于裁剪调试/后门能力。 */
#define BOARD_CFG_RELEASE    0

/* ============================ 调试与日志输出 ============================ */

/* 调试输出端口：可选 Debug_UART0、Debug_UART1、Debug_UART2 或 Debug_UART3。 */
#define DEBUG                   Debug_UART1

/* 启用后，printf/PRINT 输出重定向到 USB CDC 虚拟串口。 */
#define USB_CDC_PRINTF

/* ============================ 日志：debug_log 全局配置 ============================ */
/* 全局 debug 开关：0=使能日志；1=全部关闭。 */
#define BOARD_CFG_LOG_NODBG         0

/* 全局调试层：低于该级别的日志不输出。可用 DBG_ERROR、DBG_WARNING、DBG_INFO、DBG_LOG。 */
#define BOARD_CFG_LOG_LEVEL         DBG_INFO

/* ============================ Shell：命令行控制台 ============================ */
/* 输入/输出走 USB CDC，建立最低优先级任务消费输入。 */

/* 兼容 apl_shell 库（letter-shell）使用的 BOARD_CONFIG_ 命名 */
#define BOARD_CFG_SHELL_ENABLE          1       /* 1=启用 shell 控制台 */
#define BOARD_CFG_SHELL_BUFFER_SIZE     256     /* 命令行输入缓冲 */
#define BOARD_CFG_SHELL_HISTORY_MAX_NUMBER    5U   /* 历史历史命令记录数量 */

/* ============================ 中断优先级 ============================ */
/* PFIC 仅优先级字段高 4 位有效，数值越小优先级越高；0xF0 为最低优先级。 */
#define BOARD_CFG_USB_IRQ_PRIORITY   0xF0u
#define BOARD_CFG_TMR0_IRQ_PRIORITY  0xF0u

/* ============================ 固件版本（协议 F0 查询用） ============================ */
#define BOARD_CFG_FW_VERSION_MAJOR  1u   /* 如 V1.0.0 -> 01 00 00 */
#define BOARD_CFG_FW_VERSION_MINOR  0u
#define BOARD_CFG_FW_VERSION_PATCH  0u

#endif

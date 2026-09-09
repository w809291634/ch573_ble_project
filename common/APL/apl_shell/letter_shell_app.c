/********************************** (C) COPYRIGHT *******************************
 * File Name          : letter_shell_app.c
 * Description        : letter-shell 端口适配层（CH571 / USB CDC）。
 *                      输入：USB CDC / 串口中断收到的字节推入环形缓冲，
 *                          最低优先级任务定时把缓冲字节喂给 shellHandler。
 *                      输出：userShellWrite 重定向到 USB CDC（阻塞发送保证不丢）。
 *******************************************************************************/

#include "CONFIG.h"                 /* TMOS 任务注册/定时 */
#include "board_config.h"           /* 板级配置（shell 开关/缓冲大小） */
#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include "letter_shell_app.h"
#include "shell.h"
#include "usb_cdc/usb_cdc.h"
#include "utility/utility.h"

#define DBG_TAG         "shell"
#define DBG_LVL         DBG_INFO
#include "debug_log.h"

#if BOARD_CFG_SHELL_ENABLE == 1

/*********************************************************************
 * 常量
 *********************************************************************/
#define SHELL_TASK_EVT          0x0001
#define SHELL_POLL_PERIOD       MS1_TO_SYSTEM_TIME(10)   /* 10ms 轮询一次 */

#define SHELL_RX_BUF_SIZE       128     /* 2 的幂，用掩码取余 */

/*********************************************************************
 * 本地变量
 *********************************************************************/
static Shell user_shell;
char shellBuffer[BOARD_CFG_SHELL_BUFFER_SIZE];

/* 单生产者(中断)单消费者(任务)环形缓冲 */
static uint8_t  s_rxBuf[SHELL_RX_BUF_SIZE];
static volatile uint16_t s_rxWr;
static volatile uint16_t s_rxRd;

static tmosTaskID s_shellTaskId = INVALID_TASK_ID;

/*********************************************************************
 * 环形缓冲
 *********************************************************************/
static void shell_rx_put(uint8_t ch)
{
    uint16_t next = (s_rxWr + 1) & (SHELL_RX_BUF_SIZE - 1);
    if(next != s_rxRd)                      /* 满则丢弃 */
    {
        s_rxBuf[s_rxWr] = ch;
        s_rxWr = next;
    }
}

static int shell_rx_get(uint8_t *ch)
{
    if(s_rxRd == s_rxWr)
        return 0;
    *ch = s_rxBuf[s_rxRd];
    s_rxRd = (uint16_t)((s_rxRd + 1) & (SHELL_RX_BUF_SIZE - 1));
    return 1;
}

/*********************************************************************
 * 输出：shell 写回调（重定向到 USB CDC，阻塞发送保证不丢）
 *********************************************************************/
short userShellWrite(char *data, unsigned short len)
{
    unsigned short sent = 0;
    while(sent < len)
    {
        uint8_t chunk = ((unsigned short)(len - sent) > 64) ?
                        (uint8_t)64 : (uint8_t)(len - sent);   /* USB 单包上限 64 */
        if(USB_CDC_WriteBlocking((const uint8_t *)&data[sent], chunk) == 0)
            break;                                             /* 未连接/超时则放弃 */
        sent += chunk;
    }
    return sent;
}

/* 使用 shellHandler 逐字节喂入，不使用阻塞读，read 保留占位 */
short userShellRead(char *data, unsigned short len)
{
    (void)data;
    (void)len;
    return -1;
}

/*********************************************************************
 * 任务：把缓冲字节喂给 shell
 *********************************************************************/
static uint16_t Shell_TaskProc(tmosTaskID taskId, uint16_t events)
{
    if(events & SHELL_TASK_EVT)
    {
        uint8_t ch;
        while(shell_rx_get(&ch))
        {
            shellHandler(&user_shell, (char)ch);
        }
        tmos_start_task(taskId, SHELL_TASK_EVT, SHELL_POLL_PERIOD);
        events ^= SHELL_TASK_EVT;
    }
    return events;
}

/*********************************************************************
 * 对外接口
 *********************************************************************/
static void Shell_RxByte(uint8_t ch)
{
    shell_rx_put(ch);
}

void ShellInit(void)
{
    /* 注册为最后执行的任务，获得最低调度优先级 */
    s_shellTaskId = TMOS_ProcessEventRegister(Shell_TaskProc);

    /* 配置 shell 读写回调并初始化（命令表模式由 shellInit 自动关联） */
    user_shell.write = userShellWrite;
    user_shell.read  = userShellRead;
    shellInit(&user_shell, shellBuffer, BOARD_CFG_SHELL_BUFFER_SIZE);

    tmos_start_task(s_shellTaskId, SHELL_TASK_EVT, SHELL_POLL_PERIOD);
    log_i("apl_shell init");
}

Shell* GetShellHandle(void)
{
    return &user_shell;
}

/*********************************************************************
 * USB CDC 接收强实现（覆盖 usb_cdc.c 的 weak 默认回显）
 * 串口输入：只需在串口接收中断/回调中调用 Shell_RxByte() 即可复用同一入口。
 *********************************************************************/
void USB_CDC_OnReceive(const uint8_t *data, uint8_t length)
{
    uint8_t i;
    for(i = 0; i < length; i++)
    {
        Shell_RxByte(data[i]);
    }
}

#endif /* BOARD_CFG_SHELL_ENABLE == 1 */
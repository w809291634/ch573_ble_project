#include "letter_shell_app.h"
#include "shell.h"
#include "CONFIG.h"
#include "CH57x_sys.h"          /* SYS_ResetExecute / GetSysClock */
#include "board_config.h"
#include "peripheral.h"         /* Peripheral_SetName */
#include "apl_utility/utility.h"    /* Utility_GetTickMs */
#include "drv_usb_cdc/usb_cdc.h"    /* USB_CDC_DisconnectForReset */
#include <stdio.h>              /* printf（重定向到 USB CDC） */
#include <stddef.h>             /* ptrdiff_t */
#include <string.h>

void *_sbrk(ptrdiff_t incr);    /* newlib 堆指针（CH57x_sys.c 实现） */

/*
 * 用户自定义 shell 命令（命令导出方式）。
 *
 * 使用 SHELL_EXPORT_CMD() 导出，命令会进入链接脚本的 shellCommand 段，
 * shell 启动时自动扫描注册。添加新命令只需在此文件追加导出即可。
 */

/* 软复位 */
void cmd_reboot(void)
{
    printf("system reset...\r\n");
    SYS_ResetExecute();
}
SHELL_EXPORT_CMD(
SHELL_CMD_PERMISSION(0)|SHELL_CMD_TYPE(SHELL_TYPE_CMD_FUNC)|SHELL_CMD_DISABLE_RETURN,
reboot, cmd_reboot, system reset);

/* 显示堆内存剩余（newlib 堆从 _end 到 _heap_end） */
void cmd_free(void)
{
    extern char _end[];
    extern char _heap_end[];
    char *cur = (char *)_sbrk(0);
    unsigned long total = (unsigned long)((char *)_heap_end - (char *)_end);
    unsigned long used  = (unsigned long)((char *)cur - (char *)_end);
    unsigned long avail = (unsigned long)((char *)_heap_end - cur);

    printf("heap: total=%lu used=%lu free=%lu\r\n", total, used, avail);
}
SHELL_EXPORT_CMD(
SHELL_CMD_PERMISSION(0)|SHELL_CMD_TYPE(SHELL_TYPE_CMD_FUNC)|SHELL_CMD_DISABLE_RETURN,
free, cmd_free, show free memory);

#include "letter_shell_app.h"
#include "shell.h"
#include "CONFIG.h"
#include "CH57x_sys.h"          /* SYS_ResetExecute / GetSysClock */
#include "board_config.h"
#include "peripheral.h"         /* Peripheral_SetName */
#include "apl_utility/utility.h"    /* Utility_GetTickMs */
#include "drv_usb_cdc/usb_cdc.h"    /* USB_CDC_DisconnectForReset */
#include "CH57xBLE_LIB.h"           /* GAPRole_GetParameter / GAPROLE_* */
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

/* 系统综合信息 */
void cmd_info(void)
{
    extern char _end[];
    extern char _heap_end[];
    char *cur = (char *)_sbrk(0);
    unsigned long total = (unsigned long)((char *)_heap_end - (char *)_end);
    unsigned long used  = (unsigned long)((char *)cur - (char *)_end);
    unsigned long avail = (unsigned long)((char *)_heap_end - cur);

    uint8_t ble_state;
    uint8_t mac[6];
    const char *state_str[] = {"INIT","STARTED","ADV","WAIT","CONN","CONN_ADV","ERR"};

    GAPRole_GetParameter(GAPROLE_STATE, &ble_state);
    GAPRole_GetParameter(GAPROLE_BD_ADDR, mac);

    printf("=== System Info ===\r\n");
#ifdef FIRMWARE_VERSION
    printf("fw:     %s\r\n", FIRMWARE_VERSION);
#else
    printf("fw:     unknown\r\n");
#endif
    printf("uptime: %lu ms\r\n", Utility_GetTickMs());
    printf("sysclk: %lu Hz\r\n", GetSysClock());
    printf("heap:   total=%lu used=%lu free=%lu\r\n", total, used, avail);
    printf("chip:   0x%02X\r\n", R8_CHIP_ID);
    printf("reset:  0x%02X\r\n", R8_RESET_STATUS & 0x07);
    printf("ble:    %s (%d)\r\n", (ble_state <= 6) ? state_str[ble_state] : "UNKNOWN", ble_state);
    printf("mac:    %02X:%02X:%02X:%02X:%02X:%02X\r\n", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
}
SHELL_EXPORT_CMD(
SHELL_CMD_PERMISSION(0)|SHELL_CMD_TYPE(SHELL_TYPE_CMD_FUNC)|SHELL_CMD_DISABLE_RETURN,
info, cmd_info, show system summary);

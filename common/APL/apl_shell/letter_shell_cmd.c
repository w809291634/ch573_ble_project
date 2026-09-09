#include "letter_shell_app.h"
#include "shell.h"
#include "CONFIG.h"
#include "CH57x_sys.h"          /* SYS_ResetExecute / GetSysClock */
#include "board_config.h"
#include "peripheral.h"         /* Peripheral_SetName */
#include "nv/nv_app.h"          /* NvApp_SetAdvName / NvApp_GetAdvName / NvApp_GetStoreMode */
#include "app_logic/app_logic.h"         /* App_Proto_GetView / app_proto_view_t */
#include "protocol/app_protocol_ble.h"   /* App_Ble_GetSyncTs */
#include "utility/utility.h"    /* Utility_GetTickMs */
#include "usb_cdc/usb_cdc.h"    /* USB_CDC_DisconnectForReset */
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

/* 设置广播名：写入 NV 并更新广播（argv[1] 为名称） */
void cmd_setname(int argc, char *argv[])
{
    if(argc < 2)
    {
        printf("usage: setname <name>\r\n");
        return;
    }
    if(strlen(argv[1]) > BOARD_CFG_NV_NAME_MAX)
    {
        printf("name too long (max %d)\r\n", (int)BOARD_CFG_NV_NAME_MAX);
        return;
    }
    if(NvApp_SetAdvName(argv[1]) != 0)
    {
        printf("save nv fail\r\n");
        return;
    }
    printf("name saved: %s, rebooting...\r\n", argv[1]);
    /* 与 reboot 一致：直接软复位（先打印，复位后 USB 自动重新枚举） */
    SYS_ResetExecute();
}
SHELL_EXPORT_CMD(
SHELL_CMD_PERMISSION(0)|SHELL_CMD_TYPE(SHELL_TYPE_CMD_MAIN)|SHELL_CMD_DISABLE_RETURN,
setname, cmd_setname, set adv name (saved to NV));

/* 读取广播名（NV 记录） */
void cmd_getname(int argc, char *argv[])
{
    char name[BOARD_CFG_NV_NAME_MAX + 1];

    (void)argc;
    (void)argv;
    if(NvApp_GetAdvName(name, sizeof(name)) == 0)
        printf("adv name: %s\r\n", name);
    else
        printf("no saved name\r\n");
}
SHELL_EXPORT_CMD(
SHELL_CMD_PERMISSION(0)|SHELL_CMD_TYPE(SHELL_TYPE_CMD_MAIN)|SHELL_CMD_DISABLE_RETURN,
getname, cmd_getname, get adv name from NV);

/* 系统状态：系统时间/运行时长/系统时钟/BLE 连接 等 */

/* Unix 秒 → UTC 日期时间（正确闰年），写入 out[0..5]=年/月/日/时/分/秒 */
static void cmd_epoch_utc(uint32_t ts, uint32_t out[6])
{
    static const uint8_t mdays[12] = {31,28,31,30,31,30,31,31,30,31,30,31};
    uint32_t days = ts / 86400u;
    uint32_t sod  = ts % 86400u;
    uint32_t y = 1970u, leap, d, mon;

    while(1)
    {
        leap = ((y % 4u == 0u) && (y % 100u != 0u)) || (y % 400u == 0u);
        d = leap ? 366u : 365u;
        if(days < d) break;
        days -= d;
        y++;
    }
    leap = ((y % 4u == 0u) && (y % 100u != 0u)) || (y % 400u == 0u);
    for(mon = 0u; mon < 12u; mon++)
    {
        d = mdays[mon] + ((mon == 1u && leap) ? 1u : 0u);
        if(days < d) break;
        days -= d;
    }
    out[0] = y;
    out[1] = mon + 1u;
    out[2] = days + 1u;
    out[3] = sod / 3600u;
    out[4] = (sod % 3600u) / 60u;
    out[5] = sod % 60u;
}

void cmd_sysinfo(int argc, char *argv[])
{
    uint32_t sysclk, uptime_ms, uptime_s, sync_ts, utc[6], local[6];
    app_proto_view_t v;
    char     name[BOARD_CFG_NV_NAME_MAX + 1];

    (void)argc;
    (void)argv;

    sysclk    = GetSysClock();                     /* 系统主频(Hz) */
    uptime_ms = Utility_GetTickMs();               /* TMOS 系统时钟：开机后运行时长(ms) */
    uptime_s  = uptime_ms / 1000u;
    sync_ts   = App_Ble_GetSyncTs();               /* A0 同步到的系统时间戳(秒) */
    App_Proto_GetView(&v);                         /* 状态快照：模式/门店/锁定/连接 */

    printf("sysclk     : %lu Hz\r\n", (unsigned long)sysclk);
    printf("uptime     : %lums (%lu s, %lud %02lu:%02lu:%02lu)\r\n",
           (unsigned long)uptime_ms, (unsigned long)uptime_s,
           (unsigned long)(uptime_s / 86400u),
           (unsigned long)((uptime_s % 86400u) / 3600u),
           (unsigned long)((uptime_s % 3600u) / 60u),
           (unsigned long)(uptime_s % 60u));

    if(sync_ts != 0u)
    {
        cmd_epoch_utc(sync_ts, utc);
        /* 当前设备部署在中国标准时间区，控制台额外显示 UTC+8 本地时间；
         * Unix 时间戳本身保持 UTC，不改变 NV 中保存的同步值。 */
        cmd_epoch_utc(sync_ts + (8u * 60u * 60u), local);
        printf("sync_ts    : %lu s (UTC %lu-%02lu-%02lu %02lu:%02lu:%02lu)\r\n",
               (unsigned long)sync_ts,
               (unsigned long)utc[0], (unsigned long)utc[1], (unsigned long)utc[2],
               (unsigned long)utc[3], (unsigned long)utc[4], (unsigned long)utc[5]);
        printf("local_time : %lu-%02lu-%02lu %02lu:%02lu:%02lu (UTC+8)\r\n",
               (unsigned long)local[0], (unsigned long)local[1], (unsigned long)local[2],
               (unsigned long)local[3], (unsigned long)local[4], (unsigned long)local[5]);
    }
    else
    {
        printf("sync_ts    : not synced (0)\r\n");
    }

    /* 门店/家庭模式 + 是否锁定 + BLE 连接 + 广播名（并入 sysinfo） */
    printf("store_mode : %s\r\n", (v.store_mode == NV_APP_STORE_MODE_STORE) ? "STORE" : "FAMILY");
    printf("locked     : %s\r\n", v.locked ? "LOCKED" : "UNLOCKED");
    printf("ble_link   : %s\r\n", v.ble_linked ? "connected" : "disconnected");
    if(NvApp_GetAdvName(name, sizeof(name)) == 0)
        printf("adv name   : %s\r\n", name);
    else
        printf("adv name   : (none)\r\n");
}
SHELL_EXPORT_CMD(
SHELL_CMD_PERMISSION(0)|SHELL_CMD_TYPE(SHELL_TYPE_CMD_MAIN)|SHELL_CMD_DISABLE_RETURN,
sysinfo, cmd_sysinfo, show system info (clock/uptime/sync time/BLE link));

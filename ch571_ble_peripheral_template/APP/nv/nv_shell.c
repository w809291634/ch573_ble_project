/****************************** (C) COPYRIGHT *******************************
 * File Name          : nv_shell.c
 * Description        : NV 应用层 shell 命令：读取/设置统一 NV 结构体内的信息。
 *                       命令：
 *                         nvset store       设置门店模式
 *                         nvset family      设置家庭模式
 *                         nvset name <name> 设置广播名称
 *                        （读取门店/家庭模式 + 广播名称已并入 sysinfo 命令，nvshow 已移除）
 *******************************************************************************/

#include "apl_shell/letter_shell_app.h"
#include "apl_shell/shell.h"
#include "board_config.h"
#include "nv/nv_app.h"
#include <stdio.h>
#include <string.h>

/* 设置 NV 信息（release 版本通过宏禁用，避免现场误改） */
#if BOARD_CFG_NV_SET_CMD_ENABLE
void cmd_nvset(int argc, char *argv[])
{
    if(argc < 2)
    {
        printf("usage: nvset store|family|name <name>\r\n");
        return;
    }

    if(strcmp(argv[1], "store") == 0)
    {
        NvApp_SetStoreMode(NV_APP_STORE_MODE_STORE);
        printf("store_mode=STORE\r\n");
    }
    else if(strcmp(argv[1], "family") == 0)
    {
        NvApp_SetStoreMode(NV_APP_STORE_MODE_FAMILY);
        printf("store_mode=FAMILY\r\n");
    }
    else if(strcmp(argv[1], "name") == 0)
    {
        if(argc < 3)
        {
            printf("usage: nvset name <name>\r\n");
            return;
        }
        if(NvApp_SetAdvName(argv[2]) != 0)
            printf("save fail\r\n");
        else
            printf("name saved\r\n");
    }
    else
    {
        printf("bad param: %s\r\n", argv[1]);
    }
}
SHELL_EXPORT_CMD(
SHELL_CMD_PERMISSION(0)|SHELL_CMD_TYPE(SHELL_TYPE_CMD_MAIN)|SHELL_CMD_DISABLE_RETURN,
nvset, cmd_nvset, set NV info);
#endif /* BOARD_CFG_NV_SET_CMD_ENABLE */
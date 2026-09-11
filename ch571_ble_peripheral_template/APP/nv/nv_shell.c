/****************************** (C) COPYRIGHT *******************************
 * File Name          : nv_shell.c
 * Description        : 通用应用 NV Shell 命令。
 *******************************************************************************/

#include "apl_shell/shell.h"
#include "nv_app.h"
#include "CH57x_common.h"
#include <stdio.h>
#include <string.h>

/*********************************************************************
 * @fn      cmd_blename
 * @brief   查询或设置 BLE 广播名称；设置成功后重启设备。
 * @param   argc - 参数数量。
 * @param   argv - 参数列表，支持 get 或 set <name>。
 * @return  none
 */
void cmd_blename(int argc, char *argv[])
{
    if(argc == 2 && strcmp(argv[1], "get") == 0)
    {
        printf("%.*s\r\n", (int)BOARD_CFG_NV_NAME_MAX, NvApp_Get()->adv_name);
        return;
    }

    if(argc >= 3 && strcmp(argv[1], "set") == 0)
    {
        if(NvApp_SetAdvName(argv[2]) != 0)
        {
            printf("save failed\r\n");
            return;
        }
        printf("name saved, rebooting...\r\n");
        SYS_ResetExecute();
        return;
    }

    printf("usage: blename get | blename set <name>\r\n");
}
SHELL_EXPORT_CMD(
SHELL_CMD_PERMISSION(0)|SHELL_CMD_TYPE(SHELL_TYPE_CMD_MAIN)|SHELL_CMD_DISABLE_RETURN,
blename, cmd_blename, get or set BLE advertising name);

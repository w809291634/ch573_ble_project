/********************************** (C) COPYRIGHT *******************************
 * File Name          : nv_app.h
 * Description        : 应用 NV 配置：使用单一结构体整体加载与保存。
 *******************************************************************************/

#ifndef NV_APP_H
#define NV_APP_H

#include <stdint.h>
#include "board_config.h"

#ifdef __cplusplus
extern "C" {
#endif

/* 统一 NV 配置结构：新增持久化字段都加在这里 */
typedef struct
{
    uint32_t magic;                          /* 结构标识（首字段，用于校验） */
    char     adv_name[BOARD_CFG_NV_NAME_MAX]; /* 广播名称 */
} app_nv_cfg_t;

/* 启动时加载配置；magic 无效时恢复默认并保存。 */
void NvApp_Init(void);

/* 获取同一份应用 NV 缓存。修改可写指针后，调用 NvApp_Save() 持久化。 */
const app_nv_cfg_t *NvApp_Get(void);

/* 更新广播名称并保存完整应用 NV 配置。 */
int NvApp_SetAdvName(const char *name);

/* 将完整配置结构写回 DataFlash。 */
int NvApp_Save(void);

/* 恢复内存中的默认配置；需要持久化时再调用 NvApp_Save()。 */
void NvApp_Reset(void);

#ifdef __cplusplus
}
#endif

#endif /* NV_APP_H */

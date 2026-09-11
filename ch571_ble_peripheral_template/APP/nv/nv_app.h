/********************************** (C) COPYRIGHT *******************************
 * File Name          : nv_app.h
 * Description        : NV 应用层：所有持久化信息统一为一个结构体，加载/保存整体读写。
 *                      现有字段：广播名称、门店/家庭模式。
 *******************************************************************************/

#ifndef NV_APP_H
#define NV_APP_H

#include <stdint.h>
#include "board_config.h"

#ifdef __cplusplus
extern "C" {
#endif

/* 门店/家庭模式取值 */
#define NV_APP_STORE_MODE_FAMILY   0u     /* 家庭模式：不锁定 */
#define NV_APP_STORE_MODE_STORE    1u     /* 门店模式：需 BLE 激活 */

/* 统一 NV 配置结构：新增持久化字段都加在这里 */
typedef struct
{
    uint32_t magic;                          /* 结构标识（首字段，用于校验） */
    uint32_t version;                        /* 结构版本（结构变化时递增） */
    uint8_t  store_mode;                     /* 0=家庭, 1=门店 */
    uint8_t  reserved1;                      /* 预留 */
    uint8_t  reserved2;
    uint8_t  reserved3;
    char     adv_name[BOARD_CFG_NV_NAME_MAX]; /* 广播名称 */
    uint32_t sync_ts;                        /* A0 同步时间戳，作为治疗 ID 种子（仅同步时保存） */
} app_nv_cfg_t;

/* 从 DataFlash 加载整个配置到内部缓存；无效/新机用默认值。启动时调用一次。 */
void NvApp_Load(void);

/* 把内部缓存整个写回 DataFlash。 */
void NvApp_Save(void);

/* 广播名称：返回 0=成功，-1=无效 */
int NvApp_GetAdvName(char *name, uint32_t maxLen);
int NvApp_SetAdvName(const char *name);

/* 门店/家庭模式：0=家庭, 1=门店；写后立即存 NV */
void NvApp_GetStoreMode(uint8_t *mode);
void NvApp_SetStoreMode(uint8_t mode);

/* A0 同步时间戳：启动时读取，同步时写一次（值不变则不写，避免擦除） */
void NvApp_GetSyncTs(uint32_t *ts);
void NvApp_SetSyncTs(uint32_t ts);

#ifdef __cplusplus
}
#endif

#endif /* NV_APP_H */
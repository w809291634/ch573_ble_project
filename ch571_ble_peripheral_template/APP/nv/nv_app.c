/********************************** (C) COPYRIGHT *******************************
 * File Name          : nv_app.c
 * Description        : NV 应用层实现：所有持久化字段统一在一个结构体中，
 *                      加载/保存时整体读写 DataFlash 前区（见 board_config 的
 *                      BOARD_CFG_NV_DFLASH_OFFSET）。字段：广播名称、门店/家庭模式。
 *******************************************************************************/

#include "nv_app.h"
#include "nv_io.h"
#include <string.h>

#define DBG_TAG   "NV"
#define DBG_LVL   DBG_LOG
#include "debug_log.h"

#define NV_APP_MAGIC        0x5A4E5641UL    /* 'ZNVA' 结构标识 */
#define NV_APP_VERSION      1u              /* 结构版本，结构变化时递增 */

static app_nv_cfg_t g_cfg;

/* 是否为有效的配置（magic/version 一致）。 */
static int nv_cfg_valid(void)
{
    return (g_cfg.magic == NV_APP_MAGIC) && (g_cfg.version == NV_APP_VERSION);
}

/* 用默认值填充配置。 */
static void nv_cfg_default(void)
{
    memset(&g_cfg, 0, sizeof(g_cfg));
    g_cfg.magic   = NV_APP_MAGIC;
    g_cfg.version = NV_APP_VERSION;
    g_cfg.store_mode = NV_APP_STORE_MODE_FAMILY;
    g_cfg.sync_ts = 0u;              /* 未同步过时间戳，默认 0 */
    /* adv_name 默认空：外围用默认广播名 */
}

void NvApp_Load(void)
{
    if(NV_Read(BOARD_CFG_NV_DFLASH_OFFSET, &g_cfg, sizeof(g_cfg)) != 0 ||
       !nv_cfg_valid())
    {
        log_w("nv use default");    /* 使用默认值提示 */
        nv_cfg_default();
        NV_Write(BOARD_CFG_NV_DFLASH_OFFSET, &g_cfg, sizeof(g_cfg));   /* 写回默认 */
    }
}

void NvApp_Save(void)
{
    g_cfg.magic   = NV_APP_MAGIC;
    g_cfg.version = NV_APP_VERSION;
    NV_Write(BOARD_CFG_NV_DFLASH_OFFSET, &g_cfg, sizeof(g_cfg));
}

int NvApp_GetAdvName(char *name, uint32_t maxLen)
{
    uint32_t len;

    NvApp_Load();                          /* 每次读都从 flash 同步最新 */
    if(name == 0 || maxLen == 0)
        return -1;

    len = (uint32_t)strlen(g_cfg.adv_name);
    if(len == 0 || len >= maxLen)
        return -1;

    memcpy(name, g_cfg.adv_name, len);
    name[len] = '\0';
    return 0;
}

int NvApp_SetAdvName(const char *name)
{
    uint32_t len;

    NvApp_Load();                          /* 每次写都先读最新，避免覆盖其它字段 */
    len = (name == 0) ? 0 : (uint32_t)strlen(name);
    if(len > BOARD_CFG_NV_NAME_MAX)
        len = BOARD_CFG_NV_NAME_MAX;

    memset(g_cfg.adv_name, 0, sizeof(g_cfg.adv_name));
    if(len)
        memcpy(g_cfg.adv_name, name, len);
    NvApp_Save();                          /* 整体保存完整结构 */
    return 0;
}

void NvApp_GetStoreMode(uint8_t *mode)
{
    NvApp_Load();
    if(mode)
        *mode = (g_cfg.store_mode == NV_APP_STORE_MODE_STORE) ? NV_APP_STORE_MODE_STORE
                                                             : NV_APP_STORE_MODE_FAMILY;
}

void NvApp_SetStoreMode(uint8_t mode)
{
    NvApp_Load();                          /* 先读最新，改字段后整体保存 */
    g_cfg.store_mode = (mode == NV_APP_STORE_MODE_STORE) ? NV_APP_STORE_MODE_STORE
                                                         : NV_APP_STORE_MODE_FAMILY;
    NvApp_Save();
}

void NvApp_GetSyncTs(uint32_t *ts)
{
    NvApp_Load();
    if(ts)
        *ts = g_cfg.sync_ts;
}

void NvApp_SetSyncTs(uint32_t ts)
{
    NvApp_Load();                          /* 先读最新，避免覆盖其它字段 */
    if(g_cfg.sync_ts == ts)
        return;                            /* 值不变则不写，避免过多擦除 */
    g_cfg.sync_ts = ts;
    NvApp_Save();
}
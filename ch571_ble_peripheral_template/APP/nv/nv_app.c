/********************************** (C) COPYRIGHT *******************************
 * File Name          : nv_app.c
 * Description        : 应用 NV 配置实现。
 *******************************************************************************/

#include "nv_app.h"
#include "HAL.h"
#include <string.h>

#define NV_APP_MAGIC        0x4E564150UL

static __attribute__((aligned(4))) app_nv_cfg_t s_app_nv;
typedef char app_nv_cfg_fits_area[(sizeof(app_nv_cfg_t) <= BOARD_CFG_NV_AREA_SIZE) ? 1 : -1];
typedef char app_nv_cfg_word_aligned[(sizeof(app_nv_cfg_t) % sizeof(uint32_t) == 0) ? 1 : -1];
typedef char app_nv_area_word_aligned[(BOARD_CFG_NV_AREA_SIZE % sizeof(uint32_t) == 0) ? 1 : -1];

#define NV_APP_WORD_COUNT   ((uint32_t)(sizeof(s_app_nv) / sizeof(uint32_t)))
#define NV_APP_AREA_WORD_COUNT ((uint32_t)(BOARD_CFG_NV_AREA_SIZE / sizeof(uint32_t)))

/*********************************************************************
 * @fn      nv_copy_name
 * @brief   复制并截断广播名称，确保目标字符串以空字符结束。
 * @param   dst - 目标名称缓冲。
 * @param   src - 源名称字符串。
 * @return  none
 */
static void nv_copy_name(char *dst, const char *src)
{
    uint32_t i;

    for(i = 0; i < (BOARD_CFG_NV_NAME_MAX - 1U) && src[i] != '\0'; i++)
        dst[i] = src[i];
    dst[i] = '\0';
}

/*********************************************************************
 * @fn      NvApp_Reset
 * @brief   将应用 NV 缓存恢复为默认值。
 * @return  none
 */
void NvApp_Reset(void)
{
    memset(&s_app_nv, 0, sizeof(s_app_nv));
    s_app_nv.magic = NV_APP_MAGIC;
    nv_copy_name(s_app_nv.adv_name, BOARD_CFG_NV_DEFAULT_ADV_NAME);
}

/*********************************************************************
 * @fn      NvApp_Init
 * @brief   从 DataFlash 加载应用 NV；magic 无效时恢复默认并保存。
 * @return  none
 */
void NvApp_Init(void)
{
    if(Lib_Read_Flash(BOARD_CFG_NV_DFLASH_OFFSET, NV_APP_WORD_COUNT,
                      (uint32_t *)&s_app_nv) != 0 ||
       s_app_nv.magic != NV_APP_MAGIC)
    {
        NvApp_Reset();
        (void)NvApp_Save();
    }
    else
    {
        s_app_nv.adv_name[BOARD_CFG_NV_NAME_MAX - 1U] = '\0';
    }
}

/*********************************************************************
 * @fn      NvApp_Get
 * @brief   获取只读应用 NV 缓存。
 * @return  当前应用 NV 配置指针。
 */
const app_nv_cfg_t *NvApp_Get(void)
{
    return &s_app_nv;
}

/*********************************************************************
 * @fn      NvApp_Save
 * @brief   将完整应用 NV 配置写入 DataFlash 分配页。
 * @return  0-成功，-1-失败。
 */
int NvApp_Save(void)
{
    uint32_t page_buf[NV_APP_AREA_WORD_COUNT];

    s_app_nv.magic = NV_APP_MAGIC;
    memset(page_buf, 0xFF, sizeof(page_buf));
    memcpy(page_buf, &s_app_nv, sizeof(s_app_nv));

    return (Lib_Write_Flash(BOARD_CFG_NV_DFLASH_OFFSET, NV_APP_AREA_WORD_COUNT,
                            page_buf) == 0) ? 0 : -1;
}

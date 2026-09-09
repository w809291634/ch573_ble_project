/********************************** (C) COPYRIGHT *******************************
 * File Name          : utility.c
 * Description        : 通用工具函数实现。
 *******************************************************************************/

#include "CONFIG.h"         /* TMOS 节拍时钟 */
#include "apl_utility/utility.h"

/*********************************************************************
 * 对外接口
 *********************************************************************/
uint32_t Utility_GetTickMs(void)
{
    return Utility_TicksToMs(TMOS_GetSystemClock());
}

uint32_t Utility_TicksToMs(uint32_t ticks)
{
    return (uint32_t)(((uint64_t)ticks * (uint32_t)SYSTEM_TIME_MICROSEN + 500) / 1000);
}

uint16_t Utility_MedianFilter(const uint16_t *buf, uint8_t size)
{
    uint16_t tmp[16];   /* 窗口大小上限 16 */
    uint8_t  i, j;

    if(buf == 0U || size == 0U)
        return 0U;
    if(size > 16U)
        size = 16U;

    /* 复制后插入排序（升序），不改动调用者数据 */
    for(i = 0U; i < size; i++)
        tmp[i] = buf[i];
    for(i = 1U; i < size; i++)
    {
        for(j = i; j > 0U && tmp[j - 1U] > tmp[j]; j--)
        {
            uint16_t t = tmp[j - 1U];
            tmp[j - 1U] = tmp[j];
            tmp[j] = t;
        }
    }

    return tmp[size / 2U];   /* 奇数窗口取正中位（建议窗口取奇数） */
}

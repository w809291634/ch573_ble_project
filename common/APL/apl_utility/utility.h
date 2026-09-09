/********************************** (C) COPYRIGHT *******************************
 * File Name          : utility.h
 * Description        : 通用工具函数。
 *                      毫秒时钟统一从这里取，避免各处重复换算 TMOS 节拍。
 *******************************************************************************/

#ifndef UTILITY_H
#define UTILITY_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* 系统毫秒时钟（基于 TMOS 节拍换算）。供延时、双击判定等统一使用。 */
uint32_t Utility_GetTickMs(void);

/* 将 TMOS 节拍数换算为毫秒（带舍入）。 */
uint32_t Utility_TicksToMs(uint32_t ticks);

/* 中值滤波：对 buf[0..size-1] 取中位数返回（size≤16）。
 * 内部复制后排序，不改动调用者数组；常用于去除 ADC 等采样中的偶发尖峰。 */
uint16_t Utility_MedianFilter(const uint16_t *buf, uint8_t size);

#ifdef __cplusplus
}
#endif

#endif /* UTILITY_H */
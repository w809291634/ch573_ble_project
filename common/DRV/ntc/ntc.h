/* NTC(B3950-10K) 温度采集接口。
 * 硬件：10k 上拉 3.3V，NTC 下拉 GND，分压点接 CH571 PA4/AIN0(ADC 12bit)。
 * 温度由阻值查表+线性插值得出，纯整数、无浮点依赖，适合 BLE 工程链接环境。 */
#ifndef APP_NTC_H
#define APP_NTC_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* 初始化 ADC（PA4 浮空输入 + 单通道配置）。在硬件初始化阶段调用一次。 */
void Ntc_Init(void);

/* 读取一次平均后的 12bit ADC 原始值（0~4095）。 */
uint16_t Ntc_GetAdc(void);

/* 由分压关系换算 NTC 当前阻值（欧姆）。 */
uint32_t Ntc_GetResistanceOhm(void);

/* 换算温度，返回 温度*10（摄氏度，0.1°C 精度）。
 * 例：25.3℃ → 253；超出查表范围会钳制到边界。 */
int Ntc_GetTempC10(void);

#ifdef __cplusplus
}
#endif

#endif /* APP_NTC_H */
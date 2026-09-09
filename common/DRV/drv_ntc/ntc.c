/* NTC(B3950-10K) 温度采集：分压接 PA4/AIN0，12bit ADC。
 * 分压：10k 上拉到 3.3V，NTC 下拉到 GND，PA4 测分压点。
 * 阻值换算（用 ADC 比例，无需绝对参考电压）：
 *     Vadc/Vcc = Rntc/(Rup+Rntc)  =>  Rntc = Rup * Vadc/(Vcc-Vadc)
 * 其中 Vadc/Vcc = raw/4095，故 Rntc = Rup * raw/(4095-raw)。
 * 温度由阻值查表(ntc_lut.h)+线性插值得到，纯整数、无浮点 log 依赖。
 */
#include "CONFIG.h"
#include "ntc.h"
#include "ntc_lut.h"
#include "utility/utility.h"

#if BOARD_CFG_NTC_ENABLE == 1

#define NTC_ADC_MAX     4095UL    /* 12bit 满量程 */

static signed short s_rough_calib;   /* ADC 内部偏移粗校值（对照 EVT 示例） */

/* 采集 samples 次(每次叠加粗校值)，取中值滤波作为本次读数，抑制偶发尖峰。 */
static uint32_t Ntc_SampleAdc(void)
{
    uint16_t tmp[BOARD_CFG_NTC_SAMPLES];
    uint8_t  i;

    for(i = 0U; i < BOARD_CFG_NTC_SAMPLES; i++)
    {
        int v = (int)ADC_ExcutSingleConver() + s_rough_calib;
        if(v < 0)     v = 0;
        if(v > (int)NTC_ADC_MAX) v = (int)NTC_ADC_MAX;
        tmp[i] = (uint16_t)v;
    }
    return Utility_MedianFilter(tmp, BOARD_CFG_NTC_SAMPLES);
}

void Ntc_Init(void)
{
    GPIOA_ModeCfg(BOARD_CFG_NTC_GPIO_PIN, GPIO_ModeIN_Floating);   /* PA4 浮空输入 */
    ADC_ExtSingleChSampInit(SampleFreq_3_2, ADC_PGA_0);            /* 单通道, 0dB */
    s_rough_calib = ADC_DataCalib_Rough();                         /* 内部偏移粗校(示例) */
    ADC_ChannelCfg(BOARD_CFG_NTC_ADC_CHANNEL);                     /* PA4 -> AIN0 */
}

uint16_t Ntc_GetAdc(void)
{
    return (uint16_t)Ntc_SampleAdc();
}

uint32_t Ntc_GetResistanceOhm(void)
{
    uint32_t raw = Ntc_SampleAdc();
    if(raw > NTC_ADC_MAX)
        raw = NTC_ADC_MAX;

    /* 由 raw 与 ADC 参考电压求分压点电压 Vadc(mV)；
     * Rntc = PULLUP * Vadc / (VCC - Vadc)。避免直接假设参考=VCC。 */
    uint32_t vadc_mv = ((uint64_t)raw * BOARD_CFG_NTC_ADC_REF_MV) / NTC_ADC_MAX;
    uint32_t vcc_mv  = BOARD_CFG_NTC_VCC_MV;

    if(vadc_mv >= vcc_mv)
        return 0xFFFFFFFFU;   /* 分压点到顶(如 NTC 开路/极高阻)，返回一个超大阻值 */
    return ((uint64_t)BOARD_CFG_NTC_PULLUP_OHM * vadc_mv) / (vcc_mv - vadc_mv);
}

int Ntc_GetTempC10(void)
{
    uint32_t rntc = Ntc_GetResistanceOhm();
    uint32_t i;

    /* 表按温度升序(阻值降序)排列；越界时钳制到边界温度 */
    if(rntc >= ntc_lut[0])
        return NTC_LUT_START_DEGC * 10;                             /* <= -20℃ */
    if(rntc <= ntc_lut[NTC_LUT_NUM - 1U])
        return (NTC_LUT_START_DEGC + (int32_t)NTC_LUT_NUM - 1) * 10;/* >= 120℃ */

    for(i = 0U; i < NTC_LUT_NUM - 1U; i++)
    {
        if(ntc_lut[i] >= rntc && rntc > ntc_lut[i + 1U])
            break;
    }

    /* 线性插值：(i + START)℃ 加上 (rntc 落在这一格内的分数)*1℃ */
    {
        uint32_t span = ntc_lut[i] - ntc_lut[i + 1U];
        uint32_t frac = ((ntc_lut[i] - rntc) * 10U) / span;         /* 0~9 */
        return ((int)((int32_t)i + NTC_LUT_START_DEGC) * 10) + (int)frac;
    }
}

#endif /* BOARD_CFG_NTC_ENABLE */
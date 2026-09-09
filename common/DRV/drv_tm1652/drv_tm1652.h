/* TM1652 数码管驱动: [Start] 地址字节 数据字节 [Stop], 固定 2 字节一帧。
 * 0x18 = 显示控制((亮度<<4)|开关位), 0x08/28/48/68/88 = GR1~GR5 段码。
 *
 * 走软件单线帧(准 I2C), 不用硬件 I2C 外设: TM1652 是"准 I2C"芯片、应答不严格,
 * 硬件外设固定时序对不上(实测不亮)。也不用 SI2C_WriteNbyte, 它固定发 3 字节(会多写
 * 一个字节到下一位数码管), 改用更底层自己拼 2 字节帧。
 *
 * 使用前: board_config.h 开 BOARD_CFG_COMM_DRV_USE_TM1652 并配好引脚宏,
 * 再用 comm_drv_gpio_init() 把 SDA 配成准双向口。
 *
 * 段码表/控制字节格式来自第三方资料, 未经本项目硬件最终验证, 上机后建议核对。
 */
#ifndef __COMM_DRV_TM1652_H__
#define __COMM_DRV_TM1652_H__
#include "board_config.h"

#ifndef BOARD_CFG_COMM_DRV_USE_TM1652
#define BOARD_CFG_COMM_DRV_USE_TM1652   0
#endif

#if (BOARD_CFG_COMM_DRV_USE_TM1652)

#include <stdint.h>

#define TM1652_DIGIT_NUM        5      /* GR 总数：GR1~GR4 为数码管，GR5 为指示灯。 */
#define TM1652_BRIGHTNESS_MAX   7      /* 亮度寄存器最大值(3 位, 0~7) */

/* 共阴段码表, bit0~6=a~g, bit7=dp(小数点); 下标 0~9 对应数字 0~9 */
extern const uint8_t TM1652_SegTable[10];

/* 常用非数字段码, 可以跟 TM1652_SEG_DP 按位或叠加小数点 */
#define TM1652_SEG_BLANK   0x00u   /* 全灭 */
#define TM1652_SEG_DASH    0x40u   /* '-' */
#define TM1652_SEG_L       0x38u   /* 'L' */
#define TM1652_SEG_o       0x5Cu   /* 小写 'o', 配合 SEG_L 显示告警文案 "Lo" */
#define TM1652_SEG_H       0x76u   /* 'H' */
#define TM1652_SEG_F       0x71u   /* 'F' */
#define TM1652_SEG_DP      0x80u   /* 小数点(第 8 段) */

/* 初始化显示控制器(亮度 + 开关), 调用前必须先完成上面"前置条件"里说的 GPIO 配置。
 * brightness: 0~TM1652_BRIGHTNESS_MAX(超出会截断); on: 0=灭屏, 非0=开屏 */
void TM1652_Init(uint8_t brightness, uint8_t on);

/* 开关屏, 不改变当前亮度设置 */
void TM1652_DisplayOnOff(uint8_t on);

/* 调整亮度, 不改变当前开关状态; level 超过 TM1652_BRIGHTNESS_MAX 会被截断到该值 */
void TM1652_SetBrightness(uint8_t level);

/* 设置第 pos 位(0~4)数码管的 7 段(a~g, bit0~6)段码；bit7(指示灯/多用段)保留不动。 */
void TM1652_SetDigit(uint8_t pos, uint8_t seg);

/* 直写第 pos 位整字节段码(bit0~7 全含)，覆盖该 GR 全部段；用于设定含 bit7 指示灯的整体状态。 */
void TM1652_SetSegByte(uint8_t pos, uint8_t seg);

/* 设置第 pos 位数码管段码中的某一位(bit: 0~7 对应 a~g/dp)点亮(on=1)或熄灭(on=0)。
 * 只改这一位, 其余段位保持原状(基于内部影子段码, 段码寄存器只写不可读)。
 * 常用于把某一位(小数点/某个段)当独立指示灯单独控制; pos/bit 超范围直接忽略 */
void TM1652_SetSegBit(uint8_t pos, uint8_t seg_bit, uint8_t on);

/* GR1~GR5 全部清空(写 TM1652_SEG_BLANK) */
void TM1652_Clear(void);

#ifdef BOARD_CFG_COMM_DRV_TM1652_TEST
/* 排查用: 4 位一起从 0~9 循环显示, step_ms 为每位停留时长; 阻塞不返回, 调用前需 EA=1 */
void TM1652_Test(uint16_t step_ms);

/* 排查用: 逐位点亮第 pos 个 DIGx(0~4)的每个段位(bit0=a~bit6=g, bit7=dp), wait_ms 每位停留;
 * 用于核对 TM1652 段位到硬件实际走线(段位可能接单独 LED 而非标准 7 段数码管);
 * 阻塞不返回, 调用前需 EA=1; pos 越界忽略 */
void TM1652_SegBitTest(uint8_t pos, uint16_t wait_ms);
#endif

#endif /* BOARD_CFG_COMM_DRV_USE_TM1652 */
#endif /* __COMM_DRV_TM1652_H__ */
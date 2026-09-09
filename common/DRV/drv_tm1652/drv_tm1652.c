/* ============ TM1652 数码管驱动 (common/drv/drv_tm1652) ============
 * 4 位数码管, 走软件 I2C 自拼 2 字节帧([地址字节][数据字节]);
 * 只做显示, 不做 TM1652 自带按键扫描。详细说明见 drv_tm1652.h。
 *
 * 使用示例:
 *   comm_drv_gpio_init(GPIO_P2, GPIO_Pin_4 | GPIO_Pin_5, GPIO_PullUp);  // SDA/SCL 准双向口
 *   TM1652_Init(1, 1);                               // 亮度 1, 开屏
 *   TM1652_SetDigit(0, TM1652_SegTable[3]);          // 第 1 位显示 '3'
 *   TM1652_SetDigit(1, TM1652_SegTable[0] | TM1652_SEG_DP);   // 第 2 位显示 '0.'
 *   TM1652_SetSegBit(2, 1, 1);                       // 某一位当独立指示灯用
 *   TM1652_DisplayOnOff(0);                          // 灭屏(不改变亮度)
 */
#include "drv_tm1652.h"
#include "CH57x_common.h"       /* mDelaymS：段位测试命令阻塞等待 */
#include "apl_shell/shell.h"    /* SHELL_EXPORT_CMD：数码管段位测试命令 */
#include <stdio.h>              /* printf */
#include <stdlib.h>             /* atoi */

#ifdef BOARD_CFG_COMM_DRV_USE_TM1652

#ifndef TM1652_SDA_INIT
#error "Define TM1652_SDA_INIT, TM1652_SDA_HIGH and TM1652_SDA_LOW in board_config.h"
#endif
#ifndef TM1652_SDA_HIGH
#error "Define TM1652_SDA_HIGH in board_config.h"
#endif
#ifndef TM1652_SDA_LOW
#error "Define TM1652_SDA_LOW in board_config.h"
#endif

#define TM1652_ADDR_CTRL    0x18u
#define TM1652_BIT_TIME_US  52u
#define TM1652_LATCH_US     3000u

/* 显示控制调节命令字节位分配(见 TM1652 说明书)：
 *   [B7..B4] 位驱动占空比(0=关, 1~15 对应 1/16~15/16)
 *   [B3..B1] 段驱动电流(0~7 对应 1/8~8/8，推荐 >=2/8)
 *   [B0]     显示模式(0=8段×5位, 1=7段×6位) */
#define TM1652_CTRL_CURRENT_8_8   0x0Eu   /* 段驱动电流 8/8 */
#define TM1652_CTRL_MODE_8SEG5GR  0x00u   /* 8 段×5 位输出 */

/* 显示地址命令字 = (GR 号位<<5) |（B4=0 地址类型）| 0x08(B3~B0=1000 固定)。
 * GR1=000→0x08, GR2=100→0x88, GR3=010→0x48, GR4=110→0xC8, GR5=001→0x28, GR6=101→0xA8。
 * 注意 GR5 地址 0x28，与显示控制命令 0x18 不同。 */
static const uint8_t s_digit_addr[TM1652_DIGIT_NUM] = {0x08u, 0x88u, 0x48u, 0xC8u, 0x28u};   /* GR1~GR5 地址 */

const uint8_t TM1652_SegTable[10] =
{
    0x3Fu, 0x06u, 0x5Bu, 0x4Fu, 0x66u,   /* 0 1 2 3 4 */
    0x6Du, 0x7Du, 0x07u, 0x7Fu, 0x6Fu    /* 5 6 7 8 9 */
};

static uint8_t s_brightness;   /* 影子寄存器: 控制字节只写不可读, 本地记一份 */
static uint8_t s_on;
static uint8_t s_digit_seg[TM1652_DIGIT_NUM];   /* GR1~GR5 段码影子：寄存器只写不可读，
                                                  按位改(SetSegBit)必须先本地记一份 */

/* 发一帧 [Start] addr dat [Stop], 不用 data 做形参名(C51 关键字, 见 CLAUDE.md) */
/* TM1652 单线帧：起始位、8 位数据(LSB 先发)、奇校验位、停止位。 */
static void tm1652_write_byte(uint8_t dat)
{
    uint8_t bit;
    uint8_t one_count = 0;

    TM1652_SDA_LOW();
    mDelayuS(TM1652_BIT_TIME_US);
    for(bit = 0; bit < 8u; bit++)
    {
        if(dat & 0x01u)
        {
            TM1652_SDA_HIGH();
            one_count++;
        }
        else
        {
            TM1652_SDA_LOW();
        }
        mDelayuS(TM1652_BIT_TIME_US);
        dat >>= 1;
    }
    if(one_count & 0x01u)
    {
        TM1652_SDA_LOW();
    }
    else
    {
        TM1652_SDA_HIGH();
    }
    mDelayuS(TM1652_BIT_TIME_US);
    TM1652_SDA_HIGH();
    mDelayuS(TM1652_BIT_TIME_US);
}
static void tm1652_write2(uint8_t addr, uint8_t dat)
{
    tm1652_write_byte(addr);
    tm1652_write_byte(dat);
    TM1652_SDA_HIGH();
    mDelayuS(TM1652_LATCH_US);
}

/* 按当前 s_brightness/s_on 重发一次控制字节。
 * 亮度 0~7 映射到占空比 1/16~15/16(奇数档)，保证最低档也有可见亮度；
 * 显示时开销固定 8/8 段电流 + 8段×5位模式，关屏时占空比写 0。 */
static void tm1652_write_ctrl(void)
{
    uint8_t ctrl;

    if(!s_on)
    {
        ctrl = 0x00u;
    }
    else
    {
        uint8_t duty = (uint8_t)((s_brightness * 2u + 1u) & 0x0Fu);
        ctrl = (uint8_t)((duty << 4) | TM1652_CTRL_CURRENT_8_8 | TM1652_CTRL_MODE_8SEG5GR);
    }
    tm1652_write2(TM1652_ADDR_CTRL, ctrl);
}

void TM1652_Init(uint8_t brightness, uint8_t on)
{
    s_brightness = (brightness > TM1652_BRIGHTNESS_MAX) ? TM1652_BRIGHTNESS_MAX : brightness;
    s_on = on ? 1u : 0u;
    TM1652_SDA_INIT();
    TM1652_SDA_HIGH();
    mDelayuS(TM1652_LATCH_US);
    TM1652_Clear();
    tm1652_write_ctrl();
}

void TM1652_DisplayOnOff(uint8_t on)
{
    s_on = on ? 1u : 0u;
    tm1652_write_ctrl();
}

void TM1652_SetBrightness(uint8_t level)
{
    s_brightness = (level > TM1652_BRIGHTNESS_MAX) ? TM1652_BRIGHTNESS_MAX : level;
    tm1652_write_ctrl();
}

/* 直写第 pos 位整字节段码(bit0~7 全含)，会覆盖该 GR 当前所有段。
 * 最底层写入原语：所有段级/位级接口最终都落到这里。 */
void TM1652_SetSegByte(uint8_t pos, uint8_t seg)
{
    if (pos >= TM1652_DIGIT_NUM)
        return;
    s_digit_seg[pos] = seg;
    tm1652_write2(s_digit_addr[pos], seg);
}

/* 设置第 pos 位数码管的 7 段(a~g, bit0~6)段码；bit7(指示灯/多用段)保留不动。
 * 基于影子段码读-改-写：只替换 bit0~6，bit7 保持原状——因为同 GR 上可能挂着档位等
 * 独立指示灯(见 drv_tm1652.h 说明)，数字刷新不能覆盖它。封装于 TM1652_SetSegByte。 */
void TM1652_SetDigit(uint8_t pos, uint8_t seg)
{
    uint8_t v = (uint8_t)((s_digit_seg[pos] & 0x80u) | (seg & 0x7Fu));  /* 只改 7 段，保留 bit7 */
    TM1652_SetSegByte(pos, v);
}

/* 设置第 pos 位数码管段码中的第 bit 位(0~7 对应 a~g/dp)点亮(on=1)或熄灭(on=0)。
 * 只改这一位, 其余段位保持原状——因为段码寄存器只写不可读, 必须基于 s_digit_seg 影子
 * 改完再整字节写回。常用于把某一位(如小数点/某个段)当独立指示灯单独控制。
 *
 * 注意: 本驱动所有接口都只在应用层(主循环)调用, **不推荐在中断里调用**——软件 I2C 时序
 * 和 s_digit_seg 影子的读-改-写都依赖连续执行, 中断里调用可能被其它中断再打断导致时序错乱
 * 或显示不一致。显示刷新请统一走应用层的 app_disp_show() 汇总。 */
void TM1652_SetSegBit(uint8_t pos, uint8_t seg_bit, uint8_t on)
{
    if (pos >= TM1652_DIGIT_NUM)
        return;
    if (seg_bit >= 8u)
        return;
    if (on)  s_digit_seg[pos] |=  (uint8_t)(1u << seg_bit);
    else     s_digit_seg[pos] &= (uint8_t)~(1u << seg_bit);
    TM1652_SetSegByte(pos, s_digit_seg[pos]);
}

void TM1652_Clear(void)
{
    uint8_t i;
    for (i = 0; i < TM1652_DIGIT_NUM; i++)
        TM1652_SetSegByte(i, TM1652_SEG_BLANK);   /* 整字节清空(GM+7段) */
}

/* 数码管段位测试命令：控制 GR/bit 亮灭，直写硬件段码并在命令内阻塞等待 2s 观察。
 * 在 shell 任务里执行，阻塞期间只停 shell 轮询，应用主循环若在刷新显示会覆盖该位
 * （无应用写入时该位保持）。用于核对段位到硬件实际走线。
 * 需 BOARD_CFG_SHELL_ENABLE==1（启用控制台）才编译导出。 */
#if (BOARD_CFG_SHELL_ENABLE == 1)
void cmd_seg(int argc, char *argv[])
{
    int gr, bit, on;

    if(argc < 4)
    {
        printf("usage: seg <gr(0-4)> <bit(0-7)> <0|1>\r\n");
        return;
    }
    gr  = atoi(argv[1]);
    bit = atoi(argv[2]);
    on  = atoi(argv[3]);
    if(gr < 0 || gr > 4 || bit < 0 || bit > 7)
    {
        printf("bad param (gr 0-4, bit 0-7)\r\n");
        return;
    }

    TM1652_Clear();
    /* 直写某一位亮灭（SetSegBit 基于影子读-改-写，只动这一位），并阻塞让该段保持可见 */
    TM1652_SetSegBit((uint8_t)gr, (uint8_t)bit, on ? 1u : 0u);

    printf("seg gr%d b%d = %d (hold 500ms)\r\n", gr, bit, on);
    mDelaymS(500);
}
SHELL_EXPORT_CMD(
SHELL_CMD_PERMISSION(0)|SHELL_CMD_TYPE(SHELL_TYPE_CMD_MAIN)|SHELL_CMD_DISABLE_RETURN,
seg, cmd_seg, test one GR/bit (hold 2s));
#endif /* BOARD_CFG_SHELL_ENABLE == 1 */

#ifdef BOARD_CFG_COMM_DRV_TM1652_TEST
#include "apl_soft_timer/apl_soft_timer.h"   /* softTimer_DelayMS() */

/* 4 位一起从 0 数到 9 循环显示, 阻塞不返回, 调用前需 EA=1(内部用 softTimer_DelayMS 计时)。
 * 4 位的小数点(DP)每次循环闪一下(亮一小段再灭), 不是常亮——常亮只能看出"能不能点亮",
 * 闪烁能同时验证 DP 能点亮、也能正常熄灭(正常应用里 display_mmss() 只点第 1 位的 DP 当
 * MM:SS 分隔符, 另外 3 位的 DP 平时用不到, 光跑正常流程发现不了它们是不是真的能点亮)。 */
void TM1652_Test(uint16_t step_ms)
{
    uint8_t n = 0;
    uint16_t dp_on_ms = (uint16_t)(step_ms / 4u);   /* 闪一下的亮灯时长: 整个周期的 1/4 */
    TM1652_DisplayOnOff(1);
    for (;;)
    {
        uint8_t i;
        for (i = 0; i < TM1652_DIGIT_NUM; i++)
            TM1652_SetDigit(i, (uint8_t)(TM1652_SegTable[n] | TM1652_SEG_DP));
        softTimer_DelayMS(dp_on_ms);
        for (i = 0; i < TM1652_DIGIT_NUM; i++)
            TM1652_SetDigit(i, TM1652_SegTable[n]);
        softTimer_DelayMS((uint16_t)(step_ms - dp_on_ms));
        n = (uint8_t)((n + 1u) % 10u);
        log_raw("TM1652 %d\n", n);
    }
}

/* 逐位测试: 只测第 pos 个 DIGx(0~3), 依次只点亮该位段码的每一位, wait_ms 每个位停留, 打印提示。
 * 参数 pos 选要测哪个 DIG, 想全测就在 main 里循环调用(见 main.c)。
 * 用途: 核对 TM1652 段位(bit)到硬件实际走线——本工程 TM1652 可能不只驱动标准 7 段数码管,
 *       段位可能接单独 LED(档位标识/指示灯等), 用这个函数逐位跑一遍, 确认哪个 bit 对应哪颗灯。
 * 段位对应: bit0=a  bit1=b  bit2=c  bit3=d  bit4=e  bit5=f  bit6=g  bit7=dp(小数点)。
 * 阻塞不返回, 调用前需 EA=1(内部用 softTimer_DelayMS 计时); pos 越界直接忽略。 */
void TM1652_SegBitTest(uint8_t pos, uint16_t wait_ms)
{
    uint8_t b;

    if (pos >= TM1652_DIGIT_NUM) return;
    TM1652_DisplayOnOff(1);
    log_raw("\n--- DIG%u ---\n", pos);
    for (b = 0; b < 8; b++)
    {
        TM1652_SetDigit(pos, (uint8_t)(1u << b));   /* 只点亮第 b 位, 其余熄灭 */
        log_raw("D%u b%u\n", pos, b);
        softTimer_DelayMS(wait_ms);
    }
    TM1652_SetDigit(pos, TM1652_SEG_BLANK);           /* 该 DIG 测完熄灭 */
    log_raw("D%u end\n", pos);
}
#endif /* BOARD_CFG_COMM_DRV_TM1652_TEST */

#endif /* BOARD_CFG_COMM_DRV_USE_TM1652 */

#include "apl_inputex/apl_input.h"
#include <string.h>
#include "board_config.h"

/* APL_INPUT_LOG_E/I 可由 board_config.h 映射到工程日志；默认仍保持静默。 */
// #define DBG_TAG "input"
// #define DBG_LVL DBG_LOG
// #include "debug_log.h"

/*********************************************************************************************
* 配置
*********************************************************************************************/
/* 所有的输入对象 */
static input_obj_t g_in_objs[BOARD_CONFIG_INPUT_OBJS_NUM]={0,};
static input_mgr_t g_in_mgr={0,};

// 输入对象状态位
#define INPUT_ACTIVATION                           0   // 输入激活

// EVT/BKEVT 位号起点：低 8 位(0..7)保留给状态位, 第 8 位起用于激活超时事件
#define INPUT_ACTIVE_TIMEOUT_EVT_BASE              8
/*********************************************************************************************
* 定义
*********************************************************************************************/
/* 事件位(下标即超时级)：EVT 同步事件位，BKEVT 保持/首次判定位。
 * EVT 占 [BASE, BASE+EVT_NUM)，BKEVT 紧随其后，随 BOARD_CONFIG_INPUT_TOUT_EVT_NUM 自动扩展。 */
#define INPUT_ACTIVE_TIMEOUT_EVT(n)     (INPUT_ACTIVE_TIMEOUT_EVT_BASE + (n))
#define INPUT_ACTIVE_TIMEOUT_BKEVT(n)   (INPUT_ACTIVE_TIMEOUT_EVT_BASE + BOARD_CONFIG_INPUT_TOUT_EVT_NUM + (n))

/*********************************************************************************************
* 变量
*********************************************************************************************/
/* 获取毫秒时基：统一经管理对象持有的函数指针访问(未注入则返回 0) */
static uint32_t apl_get_ms(const input_mgr_t* in_gp)
{
    return (in_gp->get_ms != NULL) ? in_gp->get_ms() : 0U;
}

#if BOARD_CONFIG_EN_INPUT_MASK == 1
/* 组中某组合掩码最早激活按钮的起始时刻(ms)：作为组合激活时间 */
static uint32_t apl_mask_active_time(const input_mgr_t* in_gp, uint32_t mask)
{
    uint32_t t = apl_get_ms(in_gp);
    int first = 1;
    for (uint32_t i = 0; i < in_gp->obj_num; i++) {
        if ((mask & (1u << i)) && (first || in_gp->objs[i].active_time < t)) {
            t = in_gp->objs[i].active_time;
            first = 0;
        }
    }
    return t;
}

/* 统一组事件派发：对第 n 级超时，把单键/组合按下与抬起转成 input_ev_t 回调应用 */
static void apl_combo_notify(input_mgr_t* in_gp, uint32_t n)
{
    input_event_cb_t cb = in_gp->event_cb;
    if (cb == NULL) return;

    int b_mask = (int)in_gp->in_mask;
    int T_mask = (int)in_gp->in_tout_mask[n];
    int T_maskbk = (int)in_gp->in_tout_maskbk[n];
    int l_mask = (int)in_gp->inact_mask;
    input_ev_t ev = { .level = n };

    /* 按钮激活 且 满足本级超时：单按钮 */
    if (b_mask != 0 && (b_mask & (b_mask - 1)) == 0) {
        if (T_mask != 0 && (T_mask & (T_mask - 1)) == 0) {
            for (uint32_t i = 0; i < in_gp->obj_num; i++) {
                if (T_mask == (1u << i)) {
                    bitsop_clear_bit(&in_gp->in_tout_mask[n], (int)i);
                    ev.evt = INPUT_EV_SINGLE_DOWN;
                    ev.idx = i;
                    ev.mask = (1u << i);
                    ev.active_ms = in_gp->objs[i].active_time;
                    ev.elapsed_ms = apl_get_ms(in_gp) - ev.active_ms;
                    cb(&ev, in_gp->event_user);
                    ev = (input_ev_t){ .level = n };
                }
            }
        }
    } else if (b_mask != 0) {
        /* 多按钮：全部满足本级超时才上报组合 */
        if (T_mask != 0 && (T_mask & (T_mask - 1)) != 0) {
            bitsop_and(&in_gp->in_tout_mask[n], ~T_mask);
            ev.evt = INPUT_EV_COMBO_DOWN;
            ev.mask = (uint32_t)T_mask;
            ev.active_ms = apl_mask_active_time(in_gp, ev.mask);
            ev.elapsed_ms = apl_get_ms(in_gp) - ev.active_ms;
            cb(&ev, in_gp->event_user);
            ev = (input_ev_t){ .level = n };
        }
    }

    /* 按钮非激活：单按钮释放 */
    if (T_maskbk != 0 && (T_maskbk & (T_maskbk - 1)) == 0 && l_mask != 0) {
        if ((l_mask & T_maskbk) == T_maskbk) {
            for (uint32_t i = 0; i < in_gp->obj_num; i++) {
                if (T_maskbk & (1u << i)) {
                    ev.evt = INPUT_EV_SINGLE_UP;
                    ev.idx = i;
                    ev.mask = (1u << i);
                    ev.active_ms = in_gp->objs[i].active_time;
                    ev.elapsed_ms = in_gp->objs[i].inact_time - ev.active_ms;
                    cb(&ev, in_gp->event_user);
                    ev = (input_ev_t){ .level = n };
                }
            }
            in_gp->inact_mask = BITSOP_INIT(0);
            Input_Mgr_ClearToutBk(in_gp);
        }
    } else if (T_maskbk != 0 && l_mask != 0) {
        /* 多按钮组合释放：上报组合关闭 */
        if ((l_mask & T_maskbk) == T_maskbk) {
            ev.evt = INPUT_EV_COMBO_UP;
            ev.mask = (uint32_t)T_maskbk;
            ev.active_ms = apl_mask_active_time(in_gp, ev.mask);
            ev.elapsed_ms = apl_get_ms(in_gp) - ev.active_ms;
            cb(&ev, in_gp->event_user);
            ev = (input_ev_t){ .level = n };
            in_gp->inact_mask = BITSOP_INIT(0);
            Input_Mgr_ClearToutBk(in_gp);
        }
    }
}
#else  /* BOARD_CONFIG_EN_INPUT_MASK == 0 */
/* 单键事件发射(单键模式 MASK==0)：把逐对象超时/释放转成统一单键事件 */
static void apl_ev_emit(input_mgr_t* in_gp, input_evt_t t, uint32_t idx, uint32_t level,
                        uint32_t active_ms, uint32_t elapsed_ms)
{
    if (in_gp->event_cb == NULL) return;
    input_ev_t ev;
    ev.evt = t;
    ev.level = level;
    ev.idx = idx;
    ev.mask = (1u << idx);
    ev.active_ms = active_ms;
    ev.elapsed_ms = elapsed_ms;
    in_gp->event_cb(&ev, in_gp->event_user);
}
#endif  /* BOARD_CONFIG_EN_INPUT_MASK */

/*********************************************************************************************
* 函数
*********************************************************************************************/

// 输入扫描
void Input_Scan(void)
{
    int ret = 0;
    uint32_t obj_num = g_in_mgr.obj_num;

    /* 未配置读入口则无对象可扫 */
    if(g_in_mgr.read == NULL){
        APL_INPUT_LOG_E("inp:scan no-init");   /* 未初始化即扫描，仅调试用 */
        return;
    }

    /* 获取所有输入状态 */
    if(g_in_mgr.scan) g_in_mgr.scan(&g_in_mgr);
    /* 触发对应事件 */
    for(uint32_t i = 0;i < obj_num; i++){

        if(g_in_mgr.read(i)){
            /* 输入触发 */
            ret = bitsop_test_and_set_bit(&g_in_objs[i].Status,INPUT_ACTIVATION);
            if(ret == 0){
                g_in_objs[i].active_time = apl_get_ms(&g_in_mgr);
            }

            uint32_t elapsed_time = apl_get_ms(&g_in_mgr) - g_in_objs[i].active_time;

#if BOARD_CONFIG_EN_INPUT_MASK == 1
            /* 组标记 */
            bitsop_set_bit(&g_in_mgr.in_mask,i);
#endif

            /* 逐级触发激活超时事件(表驱动, 下标即超时级)
             * 未触达本级 break，等价于原逐级 else continue */
#if BOARD_CONFIG_EN_INPUT_MASK == 1
            int lv0_reached = 0;    // 已触达 T0 级，用于"去抖确认后"才清非激活标记
#endif
            for(int n = 0;n<BOARD_CONFIG_INPUT_TOUT_EVT_NUM; n++){
                uint32_t t_cur = g_in_mgr.timout.timeout[n];
                if(!(t_cur && elapsed_time >= t_cur)) break;    // 未触达本级，后续级更不会触发
#if BOARD_CONFIG_EN_INPUT_MASK == 1
                lv0_reached = 1;
#endif

                /* 首次触达该级：置位事件并回调 */
                if(bitsop_test_and_set_bit(&g_in_objs[i].Status,INPUT_ACTIVE_TIMEOUT_BKEVT(n)) == 0){
                    bitsop_set_bit(&g_in_objs[i].Status,INPUT_ACTIVE_TIMEOUT_EVT(n));
#if BOARD_CONFIG_EN_INPUT_MASK == 0
                    /* 单键激活超时达到：走统一事件回调 */
                    apl_ev_emit(&g_in_mgr, INPUT_EV_SINGLE_DOWN, (uint32_t)i, (uint32_t)n,
                                g_in_objs[i].active_time, elapsed_time);
#else
                    /* 组标记 */
                    bitsop_set_bit(&g_in_mgr.in_tout_mask[n],i);
#endif
                }

#if BOARD_CONFIG_EN_INPUT_MASK == 1
                /* 本级保持标记：只有未进入下一级窗口才保持，否则导致本级组的单击失效。
                 * 下一级不存在(数组末尾)或被禁用(timeout==0)时，本级即"最后一级"，保持。 */
                uint32_t t_next = (n + 1 < BOARD_CONFIG_INPUT_TOUT_EVT_NUM)
                                ? g_in_mgr.timout.timeout[n+1] : 0U;
                if (t_next == 0U || elapsed_time < t_next)
                    bitsop_set_bit(&g_in_mgr.in_tout_maskbk[n],i);
#endif
            }

#if BOARD_CONFIG_EN_INPUT_MASK == 1
            /* 输入去抖后清除组 非激活标记(仅当确认已触达 T0, 复刻原逐级 else continue 语义) */
            if(lv0_reached)
                bitsop_clear_bit(&g_in_mgr.inact_mask,i);
#endif
        }else{
#if BOARD_CONFIG_EN_INPUT_MASK == 1
            /* 清除组标记 */
            bitsop_clear_bit(&g_in_mgr.in_mask,i);
#endif
            // 输入非激活
            ret = bitsop_test_and_clear_bit(&g_in_objs[i].Status,INPUT_ACTIVATION);
            if(ret == 1){
                /* 任一超时级处于激活保持状态 */
                int has_bk = 0;
                for(int n = 0;n<BOARD_CONFIG_INPUT_TOUT_EVT_NUM && !has_bk; n++){
                    has_bk = bitsop_test_bit(&g_in_objs[i].Status,INPUT_ACTIVE_TIMEOUT_BKEVT(n));
                }
                if(has_bk){
                    g_in_objs[i].inact_time = apl_get_ms(&g_in_mgr);
#if BOARD_CONFIG_EN_INPUT_MASK == 0
                    /* 单键释放：走统一事件回调 */
                    apl_ev_emit(&g_in_mgr, INPUT_EV_SINGLE_UP, (uint32_t)i, 0,
                                g_in_objs[i].active_time, g_in_objs[i].inact_time - g_in_objs[i].active_time);
#else
                    // 设置组 非激活标记
                    bitsop_set_bit(&g_in_mgr.inact_mask,i);
#endif
                }

                // 清理所有激活超时事件(仅启用级)
                for(int n = 0;n<BOARD_CONFIG_INPUT_TOUT_EVT_NUM; n++){
                    bitsop_clear_bit(&g_in_objs[i].Status,INPUT_ACTIVE_TIMEOUT_EVT(n));
                    bitsop_clear_bit(&g_in_objs[i].Status,INPUT_ACTIVE_TIMEOUT_BKEVT(n));
                }

#if BOARD_CONFIG_EN_INPUT_MASK == 1
                for(int n = 0;n<BOARD_CONFIG_INPUT_TOUT_EVT_NUM; n++){
                    bitsop_clear_bit(&g_in_mgr.in_tout_mask[n],i);
                }
#endif
            }
        }
    }

#if BOARD_CONFIG_EN_INPUT_MASK == 1
    /* 统一组事件回调(倒序派发, 高优先级级先回调) */
    for (int n = BOARD_CONFIG_INPUT_TOUT_EVT_NUM - 1; n >= 0; n--)
        apl_combo_notify(&g_in_mgr, (uint32_t)n);
#endif
}

// 输入管理器初始化(统一事件模式)：一次性注入 时间表/读/时基/扫描/事件回调
int Input_Mgr_InitEvent(const input_mgr_cfg_t* cfg)
{
    if(cfg == NULL){
        APL_INPUT_LOG_E("inp:cfg NULL");
        return APL_INPUT_ERR_PARAM;
    }
    if(cfg->event_cb == NULL){
        APL_INPUT_LOG_E("inp:no evt_cb");
        return APL_INPUT_ERR_EVT_CB;
    }
    if(cfg->read == NULL){
        APL_INPUT_LOG_E("inp:no read");
        return APL_INPUT_ERR_READ;
    }
    if(cfg->get_ms == NULL){
        APL_INPUT_LOG_E("inp:no tbase");
        return APL_INPUT_ERR_TIMEBASE;
    }

    uint32_t num = cfg->obj_num ? cfg->obj_num : BOARD_CONFIG_INPUT_OBJS_NUM;
    if(num > BOARD_CONFIG_INPUT_OBJS_NUM) num = BOARD_CONFIG_INPUT_OBJS_NUM;   // 防越界

    // 整体清零后注册统一事件
    memset(&g_in_mgr, 0, sizeof(input_mgr_t));

    // 成员变量初始化(对象数组一律用内部全局数组)
    g_in_mgr.objs = g_in_objs;
    g_in_mgr.obj_num = num;

    // 统一时间表/读电平/时基/事件回调/扫描程序：由配置结构体统一注入
    if(cfg->timout)
        g_in_mgr.timout = *cfg->timout;
    g_in_mgr.read = cfg->read;
    g_in_mgr.get_ms = cfg->get_ms;
    g_in_mgr.event_cb = cfg->event_cb;
    g_in_mgr.event_user = cfg->event_user;
    g_in_mgr.scan = cfg->scan;

    // 整体清零所有对象(读/时间表统一在管理对象, 对象只需状态与时间清零)
    for(uint32_t i = 0; i < BOARD_CONFIG_INPUT_OBJS_NUM; i++)
        memset(&g_in_objs[i], 0, sizeof(input_obj_t));

    APL_INPUT_LOG_I("inp:init ok n=%u", (unsigned)num);
    return APL_INPUT_OK;
}

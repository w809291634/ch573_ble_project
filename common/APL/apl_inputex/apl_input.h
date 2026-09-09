#ifndef APL_INPUT_H
#define APL_INPUT_H
#include "board_config.h"
#include "bitsops/bits_ops.h"

/* 配置在使用类型与位掩码前校验，避免错误配置表现为数组越界或未定义移位。 */
#ifndef BOARD_CONFIG_INPUT_OBJS_NUM
#error "apl_inputex requires BOARD_CONFIG_INPUT_OBJS_NUM in board_config.h"
#endif
#ifndef BOARD_CONFIG_INPUT_TOUT_EVT_NUM
#error "apl_inputex requires BOARD_CONFIG_INPUT_TOUT_EVT_NUM in board_config.h"
#endif
#ifndef BOARD_CONFIG_EN_INPUT_MASK
#error "apl_inputex requires BOARD_CONFIG_EN_INPUT_MASK (0 or 1) in board_config.h"
#endif
#if (BOARD_CONFIG_INPUT_OBJS_NUM < 1U) || (BOARD_CONFIG_INPUT_OBJS_NUM > 31U)
#error "BOARD_CONFIG_INPUT_OBJS_NUM must be in the range 1..31"
#endif
#if (BOARD_CONFIG_INPUT_TOUT_EVT_NUM < 1U) || (BOARD_CONFIG_INPUT_TOUT_EVT_NUM > 12U)
#error "BOARD_CONFIG_INPUT_TOUT_EVT_NUM must be in the range 1..12"
#endif
#if (BOARD_CONFIG_EN_INPUT_MASK != 0U) && (BOARD_CONFIG_EN_INPUT_MASK != 1U)
#error "BOARD_CONFIG_EN_INPUT_MASK must be 0 (single-key) or 1 (group)"
#endif

/*********************************************************************************************
* 配置(示例，请在 board_config.h 中实际定义)
*********************************************************************************************/
// #define BOARD_CONFIG_INPUT_OBJS_NUM             3                           // 申请多少个 in 对象
// #define BOARD_CONFIG_INPUT_TOUT_EVT_NUM         2                           // 使用超时事件的个数
// #define BOARD_CONFIG_EN_INPUT_MASK              1U                          // 组模式：组合按 + 统一事件回调

/*********************************************************************************************
* 定义
*********************************************************************************************/
/* 统一错误码（Input_Mgr_InitEvent 返回；0=成功，负值=失败，便于应用可追溯/可恢复） */
#define APL_INPUT_OK                 (0)
#define APL_INPUT_ERR_PARAM          (-1)   /* cfg 为空指针 */
#define APL_INPUT_ERR_EVT_CB         (-2)   /* 事件回调 event_cb 缺失 */
#define APL_INPUT_ERR_READ           (-3)   /* 读电平回调 read 缺失 */
#define APL_INPUT_ERR_TIMEBASE       (-4)   /* 毫秒时基 get_ms 缺失 */

/* 分级日志钩子：默认关闭，可通过 board_config.h 映射到工程自身日志
 * 例：#define APL_INPUT_LOG_E log_e   #define APL_INPUT_LOG_I log_i */
#ifndef APL_INPUT_LOG_E
#define APL_INPUT_LOG_E(fmt, ...) ((void)0)
#endif
#ifndef APL_INPUT_LOG_I
#define APL_INPUT_LOG_I(fmt, ...) ((void)0)
#endif

/*********************************************************************************************
* 类型
*********************************************************************************************/
/* 输入对象回调函数 */
typedef int (*read_level)(uint32_t index);
/* 毫秒时基回调：由管理对象持有(函数指针), 代替散落的 BOARD_CONFIG_IN_GET_MS() */
typedef uint32_t (*get_ms_cb_t)(void);
/* 组回调函数 */
typedef int (*gp_ins_scan_cb_t)(void* user_data);

/*********** 统一组事件(可选, 组模式下一回调即可覆盖全部按键/组合) ***********/
typedef enum
{
    INPUT_EV_SINGLE_DOWN = 0,   /* 单按钮激活超时达到 */
    INPUT_EV_SINGLE_UP,         /* 单按钮释放 */
    INPUT_EV_COMBO_DOWN,        /* 组合按下(多按钮同时达到本级超时) */
    INPUT_EV_COMBO_UP,          /* 组合释放 */
}input_evt_t;

typedef struct
{
    input_evt_t evt;            /* 事件类型 */
    uint32_t level;             /* 超时级 0..TOUT_EVT_NUM-1 */
    uint32_t idx;               /* 单键事件: 按钮下标 */
    uint32_t mask;              /* 组合事件: 组合掩码(单键为 1<<idx) */
    uint32_t active_ms;         /* 本次激活起始时刻(ms); 组合取最早激活按钮 */
    uint32_t elapsed_ms;        /* 距激活时长(ms): 按下=hold 时长, 释放=持续时长 */
}input_ev_t;

typedef int (*input_event_cb_t)(const input_ev_t* ev, void* user_data);

/* 规范公开类型名；input_* 名称保留用于兼容既有 0.2.x 调用方。 */
typedef input_evt_t apl_input_event_type_t;
typedef input_ev_t apl_input_event_t;
typedef input_event_cb_t apl_input_event_cb_t;

/**********  输入对象 **********/
/* 激活超时时间表：下标即超时级，0 表示该级无效，单位ms */
typedef struct 
{
    uint32_t timeout[BOARD_CONFIG_INPUT_TOUT_EVT_NUM];
}input_timout_t;
typedef input_timout_t apl_input_timeout_t;

typedef struct 
{
    /* 状态位 */
    bitsop_t Status;

    uint32_t active_time;        // 激活记录时间
    uint32_t inact_time;         // 非激活记录时间
}input_obj_t;

/*********** 输入组对象 ***********/
typedef struct 
{
    /* 统一激活超时时间表(所有对象共用) */
    input_timout_t timout;

    /* 成员键值扫描 */
    gp_ins_scan_cb_t scan;

    /* 读电平 */
    read_level read;

    /* 毫秒时基(函数指针)：由统一配置结构体注入 */
    get_ms_cb_t get_ms;

    /* 统一事件回调(单键与组合均通过它上报) */
    input_event_cb_t event_cb;
    void* event_user;

#if BOARD_CONFIG_EN_INPUT_MASK == 1
    /* 组成员实时状态 */
    bitsop_t in_mask;
    /* 组成员非激活标记 */
    bitsop_t inact_mask;
    /* 所有输入公共 MASK：下标即超时级 */
    bitsop_t in_tout_mask[BOARD_CONFIG_INPUT_TOUT_EVT_NUM];     // 本级触发掩码,一次触发一次置位,通知后清除
    bitsop_t in_tout_maskbk[BOARD_CONFIG_INPUT_TOUT_EVT_NUM];   // 本级保持掩码,组合释放后由模块复位
#endif  /* BOARD_CONFIG_EN_INPUT_MASK */

    /* 成员指针 */
    input_obj_t* objs;
    uint32_t obj_num;
}input_mgr_t;

/*********** 输入管理器统一配置(结构体注入, 替代分散参数/宏) ***********/
typedef struct
{
    get_ms_cb_t        get_ms;        /* 毫秒时基(必填) */
    gp_ins_scan_cb_t   scan;          /* 键值扫描 */
    input_event_cb_t   event_cb;      /* 统一事件回调(必填) */
    void*              event_user;    /* 事件回调用户数据 */
    uint32_t           obj_num;       /* 对象数量, 0 用内部默认 OBJS_NUM */

    /* 统一激活超时时间表(所有对象共用) */
    const input_timout_t* timout;    /* 时间表, 可为 NULL */
    read_level        read;          /* 读电平(必填) */
}input_mgr_cfg_t;
typedef input_mgr_cfg_t apl_input_config_t;

/*********************************************************************************************
* 外部变量申明
*********************************************************************************************/
// extern input_obj_t g_in_objs[BOARD_CONFIG_INPUT_OBJS_NUM];

/*********************************************************************************************
* 函数
*********************************************************************************************/
void Input_Scan(void);
/* 统一事件初始化：用配置结构体一次性注入 时基/扫描/事件回调, 并批量初始化对象。
 * 事件回调会收到 单键按下/抬起(所有模式) 与 组合按下/抬起(仅组模式)。 */
int Input_Mgr_InitEvent(const input_mgr_cfg_t* cfg);

/* 新代码使用小写 snake_case；旧符号保持源码兼容。 */
static inline int apl_input_init(const apl_input_config_t* config)
{
    return Input_Mgr_InitEvent(config);
}

static inline void apl_input_scan(void)
{
    Input_Scan();
}

#if BOARD_CONFIG_EN_INPUT_MASK == 1
/* 复位所有超时级"保持掩码"(in_tout_maskbk)。
 * 模块在完成组合抬起通知后会自动调用；仅在应用需要主动取消当前组合周期时使用。
 * in_gp 为 input_mgr_t 指针。 */
static inline void Input_Mgr_ClearToutBk(input_mgr_t* in_gp)
{
    for (uint32_t n = 0; n < BOARD_CONFIG_INPUT_TOUT_EVT_NUM; n++)
        in_gp->in_tout_maskbk[n] = BITSOP_INIT(0);
}

static inline void apl_input_clear_timeout_backup(input_mgr_t* manager)
{
    Input_Mgr_ClearToutBk(manager);
}
#endif  /* BOARD_CONFIG_EN_INPUT_MASK */
#endif

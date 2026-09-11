/********************************** (C) COPYRIGHT *******************************
 * File Name          : user_profile.c
 * Description        : User Profile BLE 服务实现。
 *                      服务 0xFFF0，特性 0xFFF6（READ/WRITE/WRITE_NO_RSP/NOTIFY）。
 *                      收到主机写入打印 HEX，发送打印 HEX，用于调试助手验证收发。
 *******************************************************************************/

#include "CONFIG.h"
#include "user_profile.h"

#define DBG_TAG         "usr_prof"
#define DBG_LVL         DBG_INFO
#include "debug_log.h"

/*********************************************************************
 * 常量：属性表位置
 *********************************************************************/
#define USR_PROF_CHAR_VALUE_POS  2      /* 特性值在属性表中的下标 */
#define USR_PROF_MAX_CONNECTION  PERIPHERAL_MAX_CONNECTION

/*********************************************************************
 * 本地变量
 *********************************************************************/
static usr_prof_cbs_t *usrProfAppCBs = NULL;
static uint16_t usrProfConnHandle = GAP_CONNHANDLE_INIT;

/* 服务/特性 UUID */
static const uint8_t usrProfServUUID[ATT_BT_UUID_SIZE] = {
    LO_UINT16(USR_PROF_SERV_UUID), HI_UINT16(USR_PROF_SERV_UUID)};
static const uint8_t usrProfCharUUID[ATT_BT_UUID_SIZE] = {
    LO_UINT16(USR_PROF_CHAR_UUID), HI_UINT16(USR_PROF_CHAR_UUID)};

static const gattAttrType_t usrProfService = {ATT_BT_UUID_SIZE, usrProfServUUID};

/* 特性属性：读/写/无响应写/通知 */
static uint8_t usrProfCharProps = GATT_PROP_READ | GATT_PROP_WRITE |
                              GATT_PROP_WRITE_NO_RSP | GATT_PROP_NOTIFY;

/* 特性值（裸数据缓冲，写入即分发，读返回最近内容） */
static uint8_t usrProfCharValue[USR_PROF_CHAR_LEN] = {0};

/* CCCD：每个客户端一份 */
static gattCharCfg_t usrProfCharConfig[USR_PROF_MAX_CONNECTION];

static gattAttribute_t usrProfAttrTbl[] = {
    /* 服务声明 */
    {
        {ATT_BT_UUID_SIZE, primaryServiceUUID},
        GATT_PERMIT_READ,
        0,
        (uint8_t *)&usrProfService},
    /* 特性声明 */
    {
        {ATT_BT_UUID_SIZE, characterUUID},
        GATT_PERMIT_READ,
        0,
        &usrProfCharProps},
    /* 特性值 */
    {
        {ATT_BT_UUID_SIZE, usrProfCharUUID},
        GATT_PERMIT_READ | GATT_PERMIT_WRITE,
        0,
        usrProfCharValue},
    /* Client Characteristic Configuration（通知使能） */
    {
        {ATT_BT_UUID_SIZE, clientCharCfgUUID},
        GATT_PERMIT_READ | GATT_PERMIT_WRITE,
        0,
        (uint8_t *)usrProfCharConfig},
};

/*********************************************************************
 * 回调声明
 *********************************************************************/
static bStatus_t usrProf_ReadAttrCB(uint16_t connHandle, gattAttribute_t *pAttr,
                                uint8_t *pValue, uint16_t *pLen, uint16_t offset,
                                uint16_t maxLen, uint8_t method);
static bStatus_t usrProf_WriteAttrCB(uint16_t connHandle, gattAttribute_t *pAttr,
                                 uint8_t *pValue, uint16_t len, uint16_t offset,
                                 uint8_t method);

static gattServiceCBs_t usrProfCBs = {
    usrProf_ReadAttrCB,
    usrProf_WriteAttrCB,
    NULL
};

/*********************************************************************
 * 对外接口
 *********************************************************************/
bStatus_t UsrProf_AddService(uint32_t services)
{
    (void)services;
    GATTServApp_InitCharCfg(INVALID_CONNHANDLE, usrProfCharConfig);
    return GATTServApp_RegisterService(usrProfAttrTbl,
                                       GATT_NUM_ATTRS(usrProfAttrTbl),
                                       GATT_MAX_ENCRYPT_KEY_SIZE,
                                       &usrProfCBs);
}

bStatus_t UsrProf_RegisterAppCBs(usr_prof_cbs_t *appCallbacks)
{
    if(appCallbacks == NULL)
        return bleAlreadyInRequestedMode;
    usrProfAppCBs = appCallbacks;
    return SUCCESS;
}

bStatus_t UsrProf_SetParameter(uint8_t len, void *value)
{
    uint16_t cpLen = len;
    if(cpLen > USR_PROF_CHAR_LEN)
        cpLen = USR_PROF_CHAR_LEN;
    tmos_memcpy(usrProfCharValue, value, cpLen);
    if(cpLen < USR_PROF_CHAR_LEN)
        usrProfCharValue[cpLen] = 0;
    return SUCCESS;
}

uint16_t UsrProf_GetConnHandle(void)
{
    return usrProfConnHandle;
}

/* 连接状态入口：由 gapRole 连接/断开事件调用（见 peripheralStateNotificationCB）。
 * up=1 连接建立，0 连接断开。断开时清通知配置并复位连接句柄。 */
void UsrProf_OnConnState(uint16_t connHandle, uint8_t up)
{
    if(connHandle == LOOPBACK_CONNHANDLE)
        return;

    if(up)
    {
        usrProfConnHandle = connHandle;
    }
    else if(usrProfConnHandle == connHandle)
    {
        GATTServApp_InitCharCfg(connHandle, usrProfCharConfig);
        usrProfConnHandle = GAP_CONNHANDLE_INIT;
    }

    if(usrProfAppCBs && usrProfAppCBs->pfnUsrProfConnState)
        usrProfAppCBs->pfnUsrProfConnState(connHandle, up);
}

/* 打印 HEX 内容（前缀 + 逐字节十六进制） */
static void usrProfPrintHex(const char *tag, const uint8_t *p, uint16_t len)
{
    uint16_t i;
    log_i("%s (%u):", tag, (unsigned)len);
    for(i = 0; i < len; i++)
    {
        log_irawc("%02X ", p[i]);
    }
    log_irawc("\r\n");
}

/* 发送：打印 TX 内容并下发 FFF6 通知 */
bStatus_t UsrProf_Send(uint16_t connHandle, uint8_t *pValue, uint16_t len)
{
    attHandleValueNoti_t noti;
    uint16_t value = GATTServApp_ReadCharCfg(connHandle, usrProfCharConfig);

    if((value & GATT_CLIENT_CFG_NOTIFY) == 0)
    {
        log_w("notify not enabled (conn=0x%04X)", (unsigned)connHandle);
        return bleIncorrectMode;
    }
    if(len > USR_PROF_CHAR_LEN)
        len = USR_PROF_CHAR_LEN;

#if BOARD_CFG_USR_PROF_TX_LOG_EN
    usrProfPrintHex("TX", pValue, len);
#endif

    noti.len = len;
    noti.pValue = GATT_bm_alloc(connHandle, ATT_HANDLE_VALUE_NOTI, noti.len, NULL, 0);
    if(noti.pValue == NULL)
        return MSG_BUFFER_NOT_AVAIL;

    tmos_memcpy(noti.pValue, pValue, noti.len);
    noti.handle = usrProfAttrTbl[USR_PROF_CHAR_VALUE_POS].handle;

    if(GATT_Notification(connHandle, &noti, FALSE) != SUCCESS)
    {
        GATT_bm_free((gattMsg_t *)&noti, ATT_HANDLE_VALUE_NOTI);
        return bleInvalidRange;
    }
    return SUCCESS;
}

/*********************************************************************
 * 存/取回调
 *********************************************************************/
static bStatus_t usrProf_ReadAttrCB(uint16_t connHandle, gattAttribute_t *pAttr,
                                uint8_t *pValue, uint16_t *pLen, uint16_t offset,
                                uint16_t maxLen, uint8_t method)
{
    (void)connHandle;
    (void)method;

    if(pAttr->type.len != ATT_BT_UUID_SIZE)
    {
        *pLen = 0;
        return ATT_ERR_INVALID_HANDLE;
    }

    switch(BUILD_UINT16(pAttr->type.uuid[0], pAttr->type.uuid[1]))
    {
    case USR_PROF_CHAR_UUID:
        if(offset >= USR_PROF_CHAR_LEN)
            return ATT_ERR_INVALID_OFFSET;
        *pLen = MIN(maxLen, (USR_PROF_CHAR_LEN - offset));
        tmos_memcpy(pValue, &usrProfCharValue[offset], *pLen);
        break;

    default:
        return ATT_ERR_ATTR_NOT_FOUND;
    }
    return SUCCESS;
}

static bStatus_t usrProf_WriteAttrCB(uint16_t connHandle, gattAttribute_t *pAttr,
                                 uint8_t *pValue, uint16_t len, uint16_t offset,
                                 uint8_t method)
{
    (void)offset;

    if(pAttr->type.len != ATT_BT_UUID_SIZE)
        return ATT_ERR_INVALID_HANDLE;

    switch(BUILD_UINT16(pAttr->type.uuid[0], pAttr->type.uuid[1]))
    {
    case USR_PROF_CHAR_UUID:
        if(len > USR_PROF_CHAR_LEN)
            return ATT_ERR_INVALID_VALUE_SIZE;
        tmos_memcpy(usrProfCharValue, pValue, len);
        if(len < USR_PROF_CHAR_LEN)
            usrProfCharValue[len] = 0;

        /* 打印收到的写入内容（宏控制使能） */
#if BOARD_CFG_USR_PROF_RX_LOG_EN
        usrProfPrintHex("RX", pValue, len);
#endif

        /* 通知应用层收到写入（method：0x12=Write Req，0x52=Write Cmd） */
        if(usrProfAppCBs && usrProfAppCBs->pfnUsrProfChange)
            usrProfAppCBs->pfnUsrProfChange(connHandle, pValue, len, method);
        break;

    case GATT_CLIENT_CHAR_CFG_UUID:
        return GATTServApp_ProcessCCCWriteReq(connHandle, pAttr, pValue, len, offset,
                                              GATT_CLIENT_CFG_NOTIFY);

    default:
        return ATT_ERR_ATTR_NOT_FOUND;
    }
    return SUCCESS;
}

/********************************** (C) COPYRIGHT *******************************
 * File Name          : common_message.h
 * Description        : Common TMOS dynamic message module.
 *******************************************************************************/

#ifndef COMMON_MESSAGE_H
#define COMMON_MESSAGE_H

#include "CONFIG.h"

typedef struct
{
    tmos_event_hdr_t header;    /* TMOS 队列消息头，header.event 为 8 位业务消息类型。 */
    uint16_t length;            /* data 指向的数据长度，单位：字节。 */
    uint8_t *data;              /* 动态分配的数据副本；仅能由 CommonMessage_Destroy 释放。 */
} commonMessage_t;

/*
 * 复制 data 并发送给 targetTask。成功后消息所有权转移给目标任务；
 * event 是 8 位业务消息类型；发送者不能再次访问或销毁该消息。
 * length 为 0 时 data 可为 NULL。
 */
bStatus_t CommonMessage_Send(tmosTaskID targetTask, uint8_t event,
                             const uint8_t *data, uint16_t length);

/* 从当前任务的 TMOS 队列获取下一条消息；无消息时返回 NULL。 */
commonMessage_t *CommonMessage_Receive(tmosTaskID taskId);

/* 处理完成后销毁消息，依次释放动态 data 和消息体；允许传入 NULL。 */
void CommonMessage_Destroy(commonMessage_t *message);

#endif

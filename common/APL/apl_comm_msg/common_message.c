/********************************** (C) COPYRIGHT *******************************
 * File Name          : common_message.c
 * Description        : Common TMOS dynamic message module.
 *******************************************************************************/

#include "common_message.h"

/* 日志：本模块使用全局调试层；错误用 log_e（有关键信息），正常发送用 log_d（避免刷屏）。 */
#define DBG_TAG         "msg"
#define DBG_LVL         DBG_INFO
#include "debug_log.h"

bStatus_t CommonMessage_Send(tmosTaskID targetTask, uint8_t event,
                             const uint8_t *data, uint16_t length)
{
    bStatus_t status;
    commonMessage_t *message;

    if((length != 0U) && (data == NULL))
    {
        log_e("bad param");
        return INVALIDPARAMETER;
    }

    message = (commonMessage_t *)tmos_msg_allocate(sizeof(commonMessage_t));
    if(message == NULL)
    {
        log_e("alloc fail");
        return MSG_BUFFER_NOT_AVAIL;
    }

    message->header.event = event;
    message->header.status = 0;
    message->length = length;
    message->data = NULL;

    if(length != 0U)
    {
        /* 数据使用独立的动态块，消息排队期间不会引用发送者缓冲区。 */
        message->data = tmos_msg_allocate(length);
        if(message->data == NULL)
        {
            tmos_msg_deallocate((uint8_t *)message);
            log_e("data alloc fail");
            return MSG_BUFFER_NOT_AVAIL;
        }

        tmos_memcpy(message->data, (uint8_t *)data, length);
    }

    status = tmos_msg_send(targetTask, (uint8_t *)message);
    if(status != SUCCESS)
    {
        log_e("send fail st=%d", (int)status);
        CommonMessage_Destroy(message);
    }
    else
    {
        log_d("send ok evt=0x%02X", event);
    }

    return status;
}

commonMessage_t *CommonMessage_Receive(tmosTaskID taskId)
{
    return (commonMessage_t *)tmos_msg_receive(taskId);
}

void CommonMessage_Destroy(commonMessage_t *message)
{
    if(message == NULL)
    {
        return;
    }

    if(message->data != NULL)
    {
        tmos_msg_deallocate(message->data);
        message->data = NULL;
    }

    tmos_msg_deallocate((uint8_t *)message);
}

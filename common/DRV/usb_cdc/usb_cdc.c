/********************************** (C) COPYRIGHT *******************************
 * File Name          : usb_cdc.c
 * Description        : Minimal USB CDC ACM (virtual COM port) device for CH571.
 *******************************************************************************/

#include "usb_cdc.h"
#include "board_config.h"
#include "CH57x_common.h"

#define USB_PACKET_SIZE             64
#define CDC_SET_LINE_CODING         0x20
#define CDC_GET_LINE_CODING         0x21
#define CDC_SET_CONTROL_LINE_STATE  0x22

typedef struct __attribute__((packed))
{
    uint8_t request_type;
    uint8_t request;
    uint16_t value;
    uint16_t index;
    uint16_t length;
} USB_SetupRequest;

static const uint8_t device_descriptor[] =
{
    0x12, 0x01, 0x10, 0x01, 0x02, 0x00, 0x00, 0x40,
    0x86, 0x1A, 0x40, 0x80, 0x00, 0x01, 0x01, 0x02, 0x03, 0x01
};

static const uint8_t configuration_descriptor[] =
{
    0x09, 0x02, 0x43, 0x00, 0x02, 0x01, 0x00, 0x80, 0x32,
    0x09, 0x04, 0x00, 0x00, 0x01, 0x02, 0x02, 0x01, 0x00,
    0x05, 0x24, 0x00, 0x10, 0x01, 0x04, 0x24, 0x02, 0x02,
    0x05, 0x24, 0x06, 0x00, 0x01, 0x05, 0x24, 0x01, 0x00, 0x01,
    0x07, 0x05, 0x82, 0x03, 0x08, 0x00, 0x10,
    0x09, 0x04, 0x01, 0x00, 0x02, 0x0A, 0x00, 0x00, 0x00,
    0x07, 0x05, 0x01, 0x02, 0x40, 0x00, 0x00,
    0x07, 0x05, 0x81, 0x02, 0x40, 0x00, 0x00
};

static const uint8_t language_descriptor[] = {0x04, 0x03, 0x09, 0x04};
static const uint8_t manufacturer_descriptor[] = {0x08, 0x03, 'W', 0, 'C', 0, 'H', 0};
static const uint8_t product_descriptor[] =
    {0x14, 0x03, 'C', 0, 'H', 0, '5', 0, '7', 0, '1', 0, ' ', 0, 'C', 0, 'D', 0, 'C', 0};
static const uint8_t serial_descriptor[] =
    {0x10, 0x03, 'C', 0, 'H', 0, '5', 0, '7', 0, '1', 0, '0', 0, '0', 0};

__attribute__((aligned(4))) static uint8_t ep0_buffer[USB_PACKET_SIZE];
__attribute__((aligned(4))) static uint8_t ep1_buffer[USB_PACKET_SIZE * 2];
__attribute__((aligned(4))) static uint8_t ep2_buffer[USB_PACKET_SIZE];

static const uint8_t *ep0_reply;
static uint16_t ep0_remaining;
static uint8_t pending_address;
static uint8_t configuration;
static volatile uint8_t s_host_active;   /* 1=主机在拉取串口 IN（USB 串口活跃），用于决定阻塞/快发 */
static uint8_t waiting_line_coding;
static uint8_t line_coding[7] = {0x00, 0xC2, 0x01, 0x00, 0x00, 0x00, 0x08};
static uint8_t control_response[2];

static void USB_CDC_StatusIn(void)
{
    R8_UEP0_T_LEN = 0;
    R8_UEP0_CTRL = RB_UEP_R_TOG | RB_UEP_T_TOG | UEP_R_RES_NAK | UEP_T_RES_ACK;
}

static void USB_CDC_SendNextDescriptorPacket(void)
{
    uint8_t length = ep0_remaining > USB_PACKET_SIZE ? USB_PACKET_SIZE : (uint8_t)ep0_remaining;

    if(length != 0)
    {
        memcpy(ep0_buffer, ep0_reply, length);
        ep0_reply += length;
        ep0_remaining -= length;
    }
    R8_UEP0_T_LEN = length;
}

static void USB_CDC_Reply(const uint8_t *data, uint16_t length, uint16_t requested)
{
    ep0_reply = data;
    ep0_remaining = length > requested ? requested : length;
    USB_CDC_SendNextDescriptorPacket();
    R8_UEP0_CTRL = RB_UEP_R_TOG | RB_UEP_T_TOG | UEP_R_RES_ACK | UEP_T_RES_ACK;
}

/* Send one packet (up to 64 bytes). Returns 0 while the previous packet sends. */
uint8_t USB_CDC_Write(const uint8_t *data, uint8_t length)
{
    if(configuration == 0 || length > USB_PACKET_SIZE ||
       (R8_UEP1_CTRL & MASK_UEP_T_RES) != UEP_T_RES_NAK)
    {
        return 0;
    }
    memcpy(&ep1_buffer[USB_PACKET_SIZE], data, length);
    R8_UEP1_T_LEN = length;
    R8_UEP1_CTRL = (R8_UEP1_CTRL & ~MASK_UEP_T_RES) | UEP_T_RES_ACK;
    return 1;
}

/* 同步发送一包（最多 64 字节）。
 * 主机活跃（在拉取 IN，串口打开）时：等待上包发送完成并同步发本包，确保不丢数据；
 * 主机不活跃（未枚举/未开串口）时：直接发本包不等待，避免空转卡顿。 */
uint8_t USB_CDC_WriteBlocking(const uint8_t *data, uint8_t length)
{
    uint32_t guard;
    if(length > USB_PACKET_SIZE)
        length = USB_PACKET_SIZE;
    if(configuration == 0)
        return 0;
    /* 寄存器查询连接：总线挂起(SUSPEND置位)=无活跃主机（未连接/主机挂起），直接返回不做等待 */
    if(R8_USB_MIS_ST & RB_UMS_SUSPEND)
        return 0;

    if(s_host_active)
    {
        /* 主机在拉取：等待上一个 IN 周期（T_RES 变 NAK）完成，避免丢包；
         * 等待约覆盖 1 个 USB 轮询周期，无主机时 s_host_active=0 走快发不卡。 */
        guard = 50000;
        while(guard-- && (R8_UEP1_CTRL & MASK_UEP_T_RES) != UEP_T_RES_NAK)
        {
        }
        if((R8_UEP1_CTRL & MASK_UEP_T_RES) != UEP_T_RES_NAK)
        {
            s_host_active = 0;   /* 原判活跃但已停止拉取：转快发 */
            return 0;
        }
    }

    memcpy(&ep1_buffer[USB_PACKET_SIZE], data, length);
    R8_UEP1_T_LEN = length;
    R8_UEP1_CTRL = (R8_UEP1_CTRL & ~MASK_UEP_T_RES) | UEP_T_RES_ACK;
    return 1;
}

/* newlib _write() hook: split printf output into CDC bulk packets.
 * 阻塞发送：每包等待上一包发送完成，确保 printf 输出不丢字节（未配置/超时才放弃）。 */
int USB_CDC_PrintfWrite(const char *data, int length)
{
    int sent = 0;

    while(sent < length)
    {
        uint8_t packet_length = (length - sent > USB_PACKET_SIZE) ?
                                USB_PACKET_SIZE : (uint8_t)(length - sent);

        if(USB_CDC_WriteBlocking((const uint8_t *)&data[sent], packet_length) == 0)
        {
            break;
        }
        sent += packet_length;
    }
    return sent;
}

/* Override in application code to process received data.  Default: USB echo. */
__attribute__((weak)) void USB_CDC_OnReceive(const uint8_t *data, uint8_t length)
{
    (void)USB_CDC_Write(data, length);
}

static void USB_CDC_HandleSetup(void)
{
    const USB_SetupRequest *request = (const USB_SetupRequest *)ep0_buffer;
    const uint8_t *descriptor = 0;
    uint16_t length = 0;
    uint8_t type = (uint8_t)(request->value >> 8);
    uint8_t index = (uint8_t)request->value;

    waiting_line_coding = 0;
    if((request->request_type & USB_REQ_TYP_MASK) == USB_REQ_TYP_STANDARD)
    {
        switch(request->request)
        {
            case USB_GET_DESCRIPTOR:
                if(type == 1) { descriptor = device_descriptor; length = sizeof(device_descriptor); }
                else if(type == 2) { descriptor = configuration_descriptor; length = sizeof(configuration_descriptor); }
                else if(type == 3 && index == 0) { descriptor = language_descriptor; length = sizeof(language_descriptor); }
                else if(type == 3 && index == 1) { descriptor = manufacturer_descriptor; length = sizeof(manufacturer_descriptor); }
                else if(type == 3 && index == 2) { descriptor = product_descriptor; length = sizeof(product_descriptor); }
                else if(type == 3 && index == 3) { descriptor = serial_descriptor; length = sizeof(serial_descriptor); }
                else { goto stall; }
                USB_CDC_Reply(descriptor, length, request->length);
                break;
            case USB_SET_ADDRESS:
                pending_address = (uint8_t)request->value;
                USB_CDC_StatusIn();
                break;
            case USB_SET_CONFIGURATION:
                configuration = (uint8_t)request->value;
                USB_CDC_StatusIn();
                break;
            case USB_GET_CONFIGURATION:
                USB_CDC_Reply(&configuration, 1, request->length);
                break;
            case USB_GET_STATUS:
                control_response[0] = 0; control_response[1] = 0;
                USB_CDC_Reply(control_response, 2, request->length);
                break;
            case USB_GET_INTERFACE:
                control_response[0] = 0;
                USB_CDC_Reply(control_response, 1, request->length);
                break;
            case USB_SET_INTERFACE:
                USB_CDC_StatusIn();
                break;
            default:
                goto stall;
        }
    }
    else if((request->request_type & USB_REQ_TYP_MASK) == USB_REQ_TYP_CLASS)
    {
        if(request->request == CDC_GET_LINE_CODING)
        {
            USB_CDC_Reply(line_coding, sizeof(line_coding), request->length);
        }
        else if(request->request == CDC_SET_LINE_CODING && request->length == sizeof(line_coding))
        {
            waiting_line_coding = 1;
            R8_UEP0_CTRL = RB_UEP_R_TOG | RB_UEP_T_TOG | UEP_R_RES_ACK | UEP_T_RES_NAK;
        }
        else if(request->request == CDC_SET_CONTROL_LINE_STATE && request->length == 0)
        {
            USB_CDC_StatusIn();
        }
        else
        {
            goto stall;
        }
    }
    else
    {
        goto stall;
    }
    return;

stall:
    R8_UEP0_CTRL = RB_UEP_R_TOG | RB_UEP_T_TOG | UEP_R_RES_STALL | UEP_T_RES_STALL;
}

__attribute__((interrupt("WCH-Interrupt-fast")))
__attribute__((section(".highcode")))
void USB_IRQHandler(void)
{
    uint8_t status;

    if(R8_USB_INT_FG & RB_UIF_BUS_RST)
    {
        configuration = 0;
        pending_address = 0;
        R8_USB_DEV_AD = 0;
        R8_UEP0_CTRL = UEP_R_RES_ACK | UEP_T_RES_NAK;
        R8_UEP1_CTRL = RB_UEP_AUTO_TOG | UEP_R_RES_ACK | UEP_T_RES_NAK;
        R8_UEP2_CTRL = RB_UEP_AUTO_TOG | UEP_T_RES_NAK;
        R8_USB_INT_FG = RB_UIF_BUS_RST;
    }
    if(R8_USB_INT_FG & RB_UIF_SUSPEND)
    {
        R8_USB_INT_FG = RB_UIF_SUSPEND;
    }
    if((R8_USB_INT_FG & RB_UIF_TRANSFER) == 0)
    {
        return;
    }

    status = R8_USB_INT_ST;
    if(status & RB_UIS_SETUP_ACT)
    {
        USB_CDC_HandleSetup();
    }
    else if((status & (MASK_UIS_TOKEN | MASK_UIS_ENDP)) == (UIS_TOKEN_IN | 0))
    {
        if(pending_address != 0)
        {
            R8_USB_DEV_AD = pending_address;
            pending_address = 0;
        }
        if(ep0_remaining != 0)
        {
            USB_CDC_SendNextDescriptorPacket();
            R8_UEP0_CTRL ^= RB_UEP_T_TOG;
            R8_UEP0_CTRL = (R8_UEP0_CTRL & ~MASK_UEP_T_RES) | UEP_T_RES_ACK;
        }
        else
        {
            R8_UEP0_CTRL = RB_UEP_R_TOG | RB_UEP_T_TOG | UEP_R_RES_ACK | UEP_T_RES_NAK;
        }
    }
    else if((status & (MASK_UIS_TOKEN | MASK_UIS_ENDP)) == (UIS_TOKEN_OUT | 0))
    {
        if(waiting_line_coding && R8_USB_RX_LEN == sizeof(line_coding))
        {
            memcpy(line_coding, ep0_buffer, sizeof(line_coding));
            waiting_line_coding = 0;
            USB_CDC_StatusIn();
        }
        else
        {
            R8_UEP0_CTRL = RB_UEP_R_TOG | RB_UEP_T_TOG | UEP_R_RES_NAK | UEP_T_RES_NAK;
        }
    }
    else if((status & (MASK_UIS_TOKEN | MASK_UIS_ENDP)) == (UIS_TOKEN_OUT | 1))
    {
        if(R8_USB_INT_FG & RB_U_TOG_OK)
        {
            USB_CDC_OnReceive(ep1_buffer, R8_USB_RX_LEN);
        }
        R8_UEP1_CTRL = (R8_UEP1_CTRL & ~MASK_UEP_R_RES) | UEP_R_RES_ACK;
    }
    else if((status & (MASK_UIS_TOKEN | MASK_UIS_ENDP)) == (UIS_TOKEN_IN | 1))
    {
        s_host_active = 1;   /* 主机拉取了串口数据 → 判定主机活跃 */
        R8_UEP1_CTRL = (R8_UEP1_CTRL & ~MASK_UEP_T_RES) | UEP_T_RES_NAK;
    }
    R8_USB_INT_FG = RB_UIF_TRANSFER;
}

/* Call once after SetSysClock(); this module owns all USB device resources. */
void USB_CDC_Init(void)
{
    R8_USB_CTRL = 0;
    R8_UEP4_1_MOD = RB_UEP1_RX_EN | RB_UEP1_TX_EN;
    R8_UEP2_3_MOD = RB_UEP2_TX_EN;
    R16_UEP0_DMA = (uint16_t)(uint32_t)ep0_buffer;
    R16_UEP1_DMA = (uint16_t)(uint32_t)ep1_buffer;
    R16_UEP2_DMA = (uint16_t)(uint32_t)ep2_buffer;
    R8_UEP0_CTRL = UEP_R_RES_ACK | UEP_T_RES_NAK;
    R8_UEP1_CTRL = RB_UEP_AUTO_TOG | UEP_R_RES_ACK | UEP_T_RES_NAK;
    R8_UEP2_CTRL = RB_UEP_AUTO_TOG | UEP_T_RES_NAK;
    R8_USB_DEV_AD = 0;
    R16_PIN_ANALOG_IE |= RB_PIN_USB_IE | RB_PIN_USB_DP_PU;
    R8_USB_INT_FG = 0xFF;
    R8_USB_INT_EN = RB_UIE_SUSPEND | RB_UIE_BUS_RST | RB_UIE_TRANSFER;
    R8_USB_CTRL = RB_UC_DEV_PU_EN | RB_UC_INT_BUSY | RB_UC_DMA_EN;
    R8_UDEV_CTRL = RB_UD_PD_DIS | RB_UD_PORT_EN;
    PFIC_SetPriority(USB_IRQn, BOARD_CFG_USB_IRQ_PRIORITY);
    PFIC_EnableIRQ(USB_IRQn);
}

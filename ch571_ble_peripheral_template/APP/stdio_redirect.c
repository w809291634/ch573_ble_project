/********************************** (C) COPYRIGHT *******************************
 * File Name          : stdio_redirect.c
 * Description        : Project-local stdout redirection.
 *******************************************************************************/

#include "config.h"

#if (BOARD_CFG_STDIO_OUTPUT == BOARD_CFG_STDIO_OUTPUT_USB_CDC) || (BOARD_CFG_SHELL_ENABLE == 1)
#include "drv_usb_cdc/usb_cdc.h"
#endif

void Stdio_RedirectInit(void)
{
    /* Shell 固定使用 USB CDC；仅 UART stdout 且关闭 Shell 时无需初始化 USB。 */
#if (BOARD_CFG_STDIO_OUTPUT == BOARD_CFG_STDIO_OUTPUT_USB_CDC) || (BOARD_CFG_SHELL_ENABLE == 1)
    USB_CDC_Init();
#endif
#if BOARD_CFG_STDIO_OUTPUT == BOARD_CFG_STDIO_OUTPUT_UART0
    GPIOB_SetBits(bTXD0);
    GPIOB_ModeCfg(bTXD0, GPIO_ModeOut_PP_5mA);
    UART0_DefInit();
#elif BOARD_CFG_STDIO_OUTPUT == BOARD_CFG_STDIO_OUTPUT_UART1
    GPIOA_SetBits(bTXD1);
    GPIOA_ModeCfg(bTXD1, GPIO_ModeOut_PP_5mA);
    UART1_DefInit();
#elif BOARD_CFG_STDIO_OUTPUT == BOARD_CFG_STDIO_OUTPUT_UART2
    GPIOB_SetBits(bTXD2);
    GPIOB_ModeCfg(bTXD2, GPIO_ModeOut_PP_5mA);
    UART2_DefInit();
#elif BOARD_CFG_STDIO_OUTPUT == BOARD_CFG_STDIO_OUTPUT_UART3
    GPIOA_SetBits(bTXD3);
    GPIOA_ModeCfg(bTXD3, GPIO_ModeOut_PP_5mA);
    UART3_DefInit();
#endif
}

#if BOARD_CFG_STDIO_OUTPUT == BOARD_CFG_STDIO_OUTPUT_USB_CDC

int _write(int fd, char *buf, int size)
{
    (void)fd;
    return USB_CDC_PrintfWrite(buf, size);
}

#elif (BOARD_CFG_STDIO_OUTPUT == BOARD_CFG_STDIO_OUTPUT_UART0) || \
      (BOARD_CFG_STDIO_OUTPUT == BOARD_CFG_STDIO_OUTPUT_UART1) || \
      (BOARD_CFG_STDIO_OUTPUT == BOARD_CFG_STDIO_OUTPUT_UART2) || \
      (BOARD_CFG_STDIO_OUTPUT == BOARD_CFG_STDIO_OUTPUT_UART3)

int _write(int fd, char *buf, int size)
{
    int i;

    (void)fd;
    for(i = 0; i < size; i++)
    {
#if BOARD_CFG_STDIO_OUTPUT == BOARD_CFG_STDIO_OUTPUT_UART0
        while(R8_UART0_TFC == UART_FIFO_SIZE);
        R8_UART0_THR = *buf++;
#elif BOARD_CFG_STDIO_OUTPUT == BOARD_CFG_STDIO_OUTPUT_UART1
        while(R8_UART1_TFC == UART_FIFO_SIZE);
        R8_UART1_THR = *buf++;
#elif BOARD_CFG_STDIO_OUTPUT == BOARD_CFG_STDIO_OUTPUT_UART2
        while(R8_UART2_TFC == UART_FIFO_SIZE);
        R8_UART2_THR = *buf++;
#elif BOARD_CFG_STDIO_OUTPUT == BOARD_CFG_STDIO_OUTPUT_UART3
        while(R8_UART3_TFC == UART_FIFO_SIZE);
        R8_UART3_THR = *buf++;
#endif
    }
    return size;
}

#else
#error "Invalid BOARD_CFG_STDIO_OUTPUT value"
#endif

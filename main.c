/******************************************************************************
 * File Name:   main.c
 *
 * Description: This is the source code for the XMC MCU: UART DMA Ring Buffer
 *              Example for ModusToolbox. This example demonstrates how to
 *              receive data using DMA via a Universal Serial Interface Channel
 *              (USIC) and synchronize the processing with an OS task through a
 *              ring buffer. For simplicity, the OS task is emulated inside
 *              this example with a Systick Timer interrupt.
 *
 * Related Document: See README.md
 *
 *******************************************************************************
 *
 * Copyright (c) 2015-2022, Infineon Technologies AG
 * All rights reserved.
 *
 * Boost Software License - Version 1.0 - August 17th, 2003
 *
 * Permission is hereby granted, free of charge, to any person or organization
 * obtaining a copy of the software and accompanying documentation covered by
 * this license (the "Software") to use, reproduce, display, distribute,
 * execute, and transmit the Software, and to prepare derivative works of the
 * Software, and to permit third-parties to whom the Software is furnished to
 * do so, all subject to the following:
 *
 * The copyright notices in the Software and this entire statement, including
 * the above license grant, this restriction and the following disclaimer,
 * must be included in all copies of the Software, in whole or in part, and
 * all derivative works of the Software, unless such copies or derivative
 * works are solely in the form of machine-executable object code generated by
 * a source language processor.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE, TITLE AND NON-INFRINGEMENT. IN NO EVENT
 * SHALL THE COPYRIGHT HOLDERS OR ANYONE DISTRIBUTING THE SOFTWARE BE LIABLE
 * FOR ANY DAMAGES OR OTHER LIABILITY, WHETHER IN CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 *
 *****************************************************************************/

#include "cybsp.h"
#include "cy_retarget_io.h"

/*******************************************************************************
 * Defines
 *******************************************************************************/
/* Declarations for system timer emulating an OS task */
#define TICKS_PER_SECOND 1000
#define TICKS_WAIT 500

/* DMA Channel 2 */
#define GPDMA_CHANNEL_2 2

/* Declarations for ring buffer */
#define RING_BUFFER_SIZE 4096

/* Define macro to enable/disable printing of debug messages */
#define ENABLE_XMC_DEBUG_PRINT (0)

/* Define macro to set the loop count before printing debug messages */
#if ENABLE_XMC_DEBUG_PRINT
static bool TRIGGERED = false;
static bool LOOP_ENTER = false;
#endif

/*******************************************************************************
 * Global Variables
 *******************************************************************************/
/* Declaration of ring buffer */
static volatile uint8_t ring_buffer[RING_BUFFER_SIZE];
uint32_t *dst_ptr = (uint32_t *)&ring_buffer[0];

#if ( ( UC_SERIES == XMC43 ) || ( UC_SERIES == XMC44 ) )
uint32_t *src_ptr = (uint32_t *)&(XMC_UART1_CH0->RBUF);
#else
uint32_t *src_ptr = (uint32_t *)&(XMC_UART0_CH0->RBUF);
#endif

/* Strings for welcome message */
const char DELIMITER_STR[] = "************************************************\r\n";
const char APP_NAME[] = " DMA Ring buffer example\r\n";
const char APP_HELP1[] = "This example receives data from UART-RX.\r\nData is routed through a DMA ring buffer read by CPU.\r\nFinally the data is sent as echo to UART-TX.\r\n";
const char APP_HELP2[] = "Just start typing. What you type will be echoed below:\r\n";

/*******************************************************************************
 * Function Name: uart_transmit
 ********************************************************************************
 * Summary:
 * Helper function to transmit data on the UART.
 *
 * Parameters:
 *  XMC_USIC_CH_t *const channel: Pointer to USIC instance
 *  const uint8_t *data: Pointer to data for transmission
 *  uint32_t len: length of data for transmission
 *
 * Return:
 *  void
 *
 *******************************************************************************/
static void uart_transmit(XMC_USIC_CH_t *const channel, const uint8_t *data, uint32_t len)
{
    for (uint32_t i = 0; i < len; ++i, ++data)
    {
        XMC_UART_CH_Transmit(channel, *data);
    }
}

/*******************************************************************************
 * Function Name: SysTick_Handler
 ********************************************************************************
 * Summary:
 * Function called by system timer every millisecond.
 * Used inside this example to emulate an OS task which regularly checks
 * if new data was written by DMA to ring buffer.
 * Received data is echoed to the UART.
 *
 * Parameters:
 *  void
 *
 * Return:
 *  void
 *
 *******************************************************************************/
void SysTick_Handler(void)
{
    static uint32_t start = 0;

    /* Get pointer to last byte written by DMA to ringbuffer */
    uint32_t end = XMC_DMA_CH_GetTransferredData(XMC_DMA0, GPDMA_CHANNEL_2);

    /* Did the pointer proceed in the meanwhile? */
    if (start != end)
    {
        /* Has the ring buffer overflowed ? */
        if (start < end)
        {
            /* Process input data in linear buffer phase */
            uart_transmit(CYBSP_DEBUG_UART_HW, (const uint8_t *)& ring_buffer[start], end - start);
        }
        else
        {
            /* Send received data to UART
             * In overflow mode we have to process twice:
             *  - Process data until end of buffer
             *  - Process data until current position on top of buffer
             */
            uart_transmit(CYBSP_DEBUG_UART_HW, (const uint8_t *)&ring_buffer[start], RING_BUFFER_SIZE - start);
            uart_transmit(CYBSP_DEBUG_UART_HW, (const uint8_t *)&ring_buffer[0], end);
        }

        /* Set start pointer to the last read data */
        start = end;
    }
    #if ENABLE_XMC_DEBUG_PRINT
        TRIGGERED = true;
    #endif
}

/*******************************************************************************
 * Function Name: main
 ********************************************************************************
 * Summary:
 * This is the main function. The following operations are performed here:
 *    1. Welcome screen is parsed to the UART terminal.
 *    2. Initialize DMA to transfer received data from UART to ring buffer inside
 *       memory.
 *    3. Start SysTick-Handler to be called every 1ms. System timer is used for
 *       simplicity to emulate an OS-task.
 *
 * Parameters:
 *  void
 *
 * Return:
 *  int
 *
 *******************************************************************************/
int main(void)
{
    /* Initialize the device and board peripherals */
    cybsp_init();
    cy_retarget_io_init(CYBSP_DEBUG_UART_HW);

    #if ENABLE_XMC_DEBUG_PRINT
        printf("Init complete\r\n");
    #else
    /* Show welcome message on terminal during initialization */
    uart_transmit(CYBSP_DEBUG_UART_HW, (const uint8_t *)DELIMITER_STR, sizeof(DELIMITER_STR));
    uart_transmit(CYBSP_DEBUG_UART_HW, (const uint8_t *)APP_NAME, sizeof(APP_NAME));
    uart_transmit(CYBSP_DEBUG_UART_HW, (const uint8_t *)DELIMITER_STR, sizeof(DELIMITER_STR));
    uart_transmit(CYBSP_DEBUG_UART_HW, (const uint8_t *)APP_HELP1, sizeof(APP_HELP1));
    uart_transmit(CYBSP_DEBUG_UART_HW, (const uint8_t *)APP_HELP2, sizeof(APP_HELP2));
    #endif

    /* Enable DMA module */
    XMC_DMA_CH_Enable(XMC_DMA0, GPDMA_CHANNEL_2);

    /* System timer configuration */
    SysTick_Config(SystemCoreClock / TICKS_PER_SECOND);

    while (1)
        {
        #if ENABLE_XMC_DEBUG_PRINT
            if(TRIGGERED && !LOOP_ENTER)
            {
                printf("Systick handler triggered\r\n");
                LOOP_ENTER = true;
            }
        #endif
        }
}

/* [] END OF FILE */

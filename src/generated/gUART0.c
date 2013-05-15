//-------------------------------------------------------------------------------
// Copyright (c) 2012 by Silicon Laboratories
// All rights reserved. This program and the accompanying materials
// are made available under the terms of the Silicon Laboratories End User
// License Agreement which accompanies this distribution, and is available at
// http://developer.silabs.com/legal/version/v10/License_Agreement_v10.htm
//
//
// Original content and implementation provided by Silicon Laboratories
//-------------------------------------------------------------------------------

//==============================================================================
// WARNING:
//
// This file is auto-generated by AppBuilder and should not be modified.
// Any hand modifications will be lost if the project is regenerated.
//==============================================================================

// Include peripheral access modules used in this file
#include <si32_device.h>
#include "circular_buffer.h"
#include "gTIMER.h"
#include "gUART0.h"
#include "gCPU.h"

void UART0_rx_data_req_handler(void);
void UART0_tx_complete_handler(void);
//==============================================================================
// 1st Level Interrupt Handlers
//==============================================================================
void UART0_IRQHandler()
{
    if (SI32_UART_A_is_rx_frame_error_interrupt_pending(SI32_UART_0))
    {
       SI32_UART_A_clear_rx_frame_error_interrupt(SI32_UART_0);
    }
    if (SI32_UART_A_is_rx_parity_error_interrupt_pending(SI32_UART_0))
    {
       SI32_UART_A_clear_rx_parity_error_interrupt(SI32_UART_0);
    }
    if (SI32_UART_A_is_rx_overrun_interrupt_pending(SI32_UART_0))
    {
       SI32_UART_A_clear_rx_overrun_error_interrupt(SI32_UART_0);
    }
    if (SI32_UART_A_is_rx_data_request_interrupt_pending(SI32_UART_0))
    {
       SI32_UART_A_clear_rx_data_request_interrupt(SI32_UART_0);
    }
    if (SI32_UART_A_is_tx_smartcard_parity_error_interrupt_pending(SI32_UART_0))
    {
       SI32_UART_A_clear_tx_smartcard_parity_error_interrupt(SI32_UART_0);
    }
    if (SI32_UART_A_is_rx_fifo_error_interrupt_pending(SI32_UART_0))
    {
       SI32_UART_A_clear_rx_fifo_error_interrupt(SI32_UART_0);
    }
    if (SI32_UART_A_is_tx_fifo_error_interrupt_pending(SI32_UART_0))
    {
       SI32_UART_A_clear_tx_fifo_error_interrupt(SI32_UART_0);
    }

    if (SI32_UART_A_is_tx_data_request_interrupt_pending(SI32_UART_0))
    {
        UART0_rx_data_req_handler();
    }
    if (SI32_UART_A_is_tx_complete(SI32_UART_0))
    {
        UART0_tx_complete_handler();
    }
}


//==============================================================================
// Configuration Functions
//==============================================================================
/*
 * UART0 <---> EP2, RX circular buffer in[1]
 */
void UART0_rx_data_req_handler(void)
{
    uint32_t count;
    uint8_t rece_data;
    circular_buffer_pools_t * cb = circular_buffer_pointer(0x82); // EP2 IN

    stop_timer(1); // timer0H for EP2
    SI32_UART_A_clear_rx_data_request_interrupt(SI32_UART_0);
    count = SI32_UART_A_read_rx_fifo_count (SI32_UART_0);
    while(count--)
    {
        rece_data = SI32_UART_A_read_data_u8 (SI32_UART_0);
        circular_buffer_push(cb,&rece_data);
    }
    start_timer(1);
}

/*
 * UART0 <---> EP2, TX circular buffer out[1]
 */
void UART0_tx_complete_handler(void)
{
    uint8_t send_data;
    uint32_t count;
    circular_buffer_pools_t * cb = circular_buffer_pointer(0x02); // EP2 out

    SI32_UART_A_clear_tx_complete_interrupt (SI32_UART_0);
    if(!circular_buffer_is_empty(cb))
    {
        count = MIN(circular_buffer_count(cb),UART_MAX_FIFO_COUNT);

        while(count--)
        {
            circular_buffer_pop(cb,&send_data);
            SI32_UART_A_write_data_u8(SI32_UART_0, send_data);
        }
    }
}

uint8_t uart_get_byte()
{
	return SI32_UART_A_read_data_u8(SI32_UART_0);
}

void uart_send_byte(uint8_t data)
{
    // Block if the output buffer is full
    while (SI32_UART_A_read_tx_fifo_count(SI32_UART_0) >= 4);
    // Write character to the output buffer
    SI32_UART_A_write_data_u8(SI32_UART_0, data);
}

void uart_send_data(unsigned char *data, unsigned int count)
{
   while(count--)
   {
      // Block if the output buffer is full
      while (SI32_UART_A_read_tx_fifo_count(SI32_UART_0) >= 4);
      // Write character to the output buffer
      SI32_UART_A_write_data_u8(SI32_UART_0, *data++);
   }
}

uint32_t uart_get_data(uint8_t * data)
{
	uint32_t recv_bytes, count;
	recv_bytes = SI32_UART_A_read_rx_fifo_count(SI32_UART_0);
	count = recv_bytes;
	while(count--)
	{
		*data++ = SI32_UART_A_read_data_u8(SI32_UART_0);
	}
	return recv_bytes;
}

#if 0
int uart_get_data(unsigned char *data, unsigned int count)
{
   unsigned int time_out;
   while(count--)
   {
      time_out = msTicks + 100;
      while(SI32_UART_A_read_rx_fifo_count(SI32_UART_0) == 0)
      {
         if(time_out < msTicks)
         {
            return -1;
         }
      }
      *data++ = uart_get_byte();
   }
   return 0;
}
#endif
/*
 *   baud_rate: baud rate
 *   stop_bits: 0 - 1 stop bit; 1 - 1.5 stop bits; 2 - 2 stop bits
 *   parity: 0 - none; 1 - odd; 2 - even; 3 - mark; 4 - space
 *   data_bits: data bits(5,6,7,8 or 16)
 */
void uart_configuration(uint32_t baud_rate,uint8_t stop_bits,uint8_t parity,uint8_t data_bits)
{
    SI32_UART_A_disable_tx(SI32_UART_0);
    SI32_UART_A_disable_rx(SI32_UART_0);

    SI32_UART_A_set_rx_baudrate(SI32_UART_0, (SystemCoreClock / (2 * baud_rate)) - 1);
    SI32_UART_A_set_tx_baudrate(SI32_UART_0, (SystemCoreClock / (2 * baud_rate)) - 1);

    if(stop_bits < 3)
    {
    	// Add 1 to match with UART register definition
    	stop_bits += 1;
        SI32_UART_A_select_tx_stop_bits(SI32_UART_0, stop_bits);
        SI32_UART_A_select_rx_stop_bits(SI32_UART_0, stop_bits);
    }
    if(parity < 5)
    {
    	if(parity == 0)
    	{
    	    SI32_UART_A_disable_tx_parity_bit(SI32_UART_0);
    	    SI32_UART_A_disable_rx_parity_bit(SI32_UART_0);
    	}
    	else
    	{
    		// Sub 1 to match with UART register definition
    		parity -= 1;
    		SI32_UART_0->CONFIG_CLR = 0x07000700;
    		SI32_UART_0->CONFIG_SET = (parity << 8)|(parity << 24);
    	}
    }
    if((data_bits > 4) && (data_bits < 9))
    {
    	// Sub 5 to match with UART regsiter definition
    	data_bits -= 5;
		SI32_UART_0->CONFIG_CLR = 0x00600060;
		SI32_UART_0->CONFIG_SET = (parity << 5)|(parity << 21);
    }

    SI32_UART_A_enable_tx(SI32_UART_0);
    SI32_UART_A_enable_rx(SI32_UART_0);
}
void UART0_enter_default_mode_from_reset(void)
{
	SI32_UART_A_set_rx_baudrate(SI32_UART_0, (SystemCoreClock / (2 * 115200)) - 1);
    SI32_UART_A_set_tx_baudrate(SI32_UART_0, (SystemCoreClock / (2 * 115200)) - 1);

    // SETUP TX (8-bit, 1stop, no-parity)
    SI32_UART_A_select_tx_data_length(SI32_UART_0, 8);
    SI32_UART_A_enable_tx_start_bit(SI32_UART_0);
    SI32_UART_A_enable_tx_stop_bit(SI32_UART_0);
    SI32_UART_A_disable_tx_parity_bit(SI32_UART_0);
    SI32_UART_A_select_tx_stop_bits(SI32_UART_0, SI32_UART_A_STOP_BITS_1_BIT);

    // SETUP RX
    SI32_UART_A_select_rx_data_length(SI32_UART_0, 8);
    SI32_UART_A_enable_rx_start_bit(SI32_UART_0);
    SI32_UART_A_enable_rx_stop_bit(SI32_UART_0);
    SI32_UART_A_disable_rx_parity_bit(SI32_UART_0);
    SI32_UART_A_select_rx_stop_bits(SI32_UART_0, SI32_UART_A_STOP_BITS_1_BIT);
    SI32_UART_A_select_rx_fifo_threshold_1(SI32_UART_0);

    SI32_UART_A_clear_tx_complete_interrupt (SI32_UART_0);
    SI32_UART_A_clear_rx_data_request_interrupt (SI32_UART_0);
    SI32_UART_A_select_tx_complete_threshold_no_more_data (SI32_UART_0);
    SI32_UART_A_enable_tx_complete_interrupt (SI32_UART_0);
    SI32_UART_A_enable_rx_data_request_interrupt (SI32_UART_0);
    SI32_UART_A_enable_rx_error_interrupts (SI32_UART_0);

    SI32_UART_A_enable_rx (SI32_UART_0);
    SI32_UART_A_enable_tx (SI32_UART_0);

    NVIC_SetPriority (UART0_IRQn, UART0_InterruptPriority);
    NVIC_ClearPendingIRQ(UART0_IRQn);
    NVIC_EnableIRQ (UART0_IRQn);

}


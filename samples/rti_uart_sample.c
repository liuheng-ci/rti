/*
 * File      : rti_uart_sample.c
 * This file is part of RT-Thread RTOS
 * COPYRIGHT (C) 2006 - 2012, RT-Thread Development Team
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along
 *  with this program; if not, write to the Free Software Foundation, Inc.,
 *  51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 *
 * Change Logs:
 * Date           Author       Notes
 * 2018-06-05     flybreak     first version
 */

#include <rtthread.h>
#include <rtdevice.h>

#include "rti.h"

#ifdef RT_USING_FINSH
#include <finsh.h>
#endif

#ifndef PKG_RTI_UART_BAUD_RATE
#define PKG_RTI_UART_BAUD_RATE   BAUD_RATE_460800
#endif

#define TRANSIT_BUF_SIZE         (PKG_RTI_BUFFER_SIZE / 8)
#define RTI_UART_BAUD_RATE       (PKG_RTI_UART_BAUD_RATE)
#define RTI_UART_NAME            "rti_uart"

static rt_uint8_t transit_buf[TRANSIT_BUF_SIZE];
static rt_device_t rti_dev;

#define REV_START_FIRSE_FARAM_BYTE0 0x53
#define REV_START_FIRSE_FARAM_BYTE1 0x56
#define REV_START_SECOND_FARAM_BYTE 0x01
#define REV_STOP_FARAM_BYTE 0x02

#define REV_START_FIRST_FARAM_SIZE 4

static struct rt_semaphore rx_sem;
static char rti_rx_stack[1024];
static struct rt_thread rti_rx_th;

void rti_data_new_data_notify(void)
{
    int ret = rti_data_get(transit_buf, TRANSIT_BUF_SIZE);
    rt_device_write(rti_dev, 0, transit_buf, ret);
}

rt_err_t rt_ind(rt_device_t dev, rt_size_t size)
{
    //rti_data_new_data_notify_set_hook(rti_data_new_data_notify);
    //rti_start();
	rt_sem_release(&rx_sem);
    return 0;
}

static void rti_rx_entry(void *param)
{
	char ch;

	rt_uint8_t rx_cnt = 0;	
	rt_uint8_t rx_buff[REV_START_FIRST_FARAM_SIZE];

	while(1)
	{
		while (rt_device_read(rti_dev, -1, &ch, 1) != 1)
		{
			rx_cnt = 0;		
			rt_sem_take(&rx_sem, RT_WAITING_FOREVER);		
		}
		
		//rt_kprintf("%02X ",ch);
		
		rx_buff[rx_cnt++] = ch;			
		
		if(rx_cnt==REV_START_FIRST_FARAM_SIZE)
		{
			if(rx_buff[0] == REV_START_FIRSE_FARAM_BYTE0 && rx_buff[1] == REV_START_FIRSE_FARAM_BYTE1)
			{
				rti_data_new_data_notify_set_hook(rti_data_new_data_notify);
				rti_start();
			}
			
			rx_cnt = 0;
		}		
		
		if(ch == REV_START_SECOND_FARAM_BYTE)
		{
			rti_data_new_data_notify_set_hook(rti_data_new_data_notify);
			rti_start();
		}
		
		if(ch == REV_STOP_FARAM_BYTE)
		{					
			rti_stop();
			rti_data_new_data_notify_set_hook(NULL);
		}
	}
}

void rti_uart_sample(void)
{
    struct serial_configure config = RT_SERIAL_CONFIG_DEFAULT;

    rti_dev = rt_console_get_device();

#ifdef RT_USING_FINSH
    finsh_set_device(RTI_UART_NAME);
#endif
    rt_console_set_device(RTI_UART_NAME);

    rti_dev->open_flag &= ~RT_DEVICE_FLAG_STREAM;

    config.baud_rate = RTI_UART_BAUD_RATE;
    rt_device_control(rti_dev, RT_DEVICE_CTRL_CONFIG, &config);

    rt_device_set_rx_indicate(rti_dev, rt_ind);
    rt_device_open(rti_dev, RT_DEVICE_OFLAG_RDWR | RT_DEVICE_FLAG_INT_RX);
	
	rt_thread_init(&rti_rx_th,"rti_rx",rti_rx_entry,RT_NULL,&rti_rx_stack[0],sizeof(rti_rx_stack),20 - 1, 20);
    rt_thread_startup(&rti_rx_th);
}
#ifdef RT_USING_FINSH
MSH_CMD_EXPORT(rti_uart_sample, start record by uart.);
#endif

/*
 * rti uart device
 */

#define RTI_UART_BUF             (128)
static char uart_buf[RTI_UART_BUF];
static struct rt_serial_device rti_serial;

static rt_err_t rti_uart_configure(struct rt_serial_device *serial, struct serial_configure *cfg)
{
    return RT_EOK;
}
static rt_err_t rti_uart_control(struct rt_serial_device *serial, int cmd, void *arg)
{
    return RT_EOK;
}
static int rti_uart_putc(struct rt_serial_device *serial, char c)
{
    static int index = 0;
    uart_buf[index++] = c;
    if (c == '\n' || index == RTI_UART_BUF - 1)
    {
        uart_buf[index] = '\0';
        rti_print(uart_buf);
        index = 0;
    }
    return 1;
}
static int rti_uart_getc(struct rt_serial_device *serial)
{
    return -1;
}

static const struct rt_uart_ops rti_uart_ops =
{
    .configure = rti_uart_configure,
    .control = rti_uart_control,
    .putc = rti_uart_putc,
    .getc = rti_uart_getc,
    .dma_transmit = RT_NULL
};

static int rti_uart_init(void)
{
    struct serial_configure config = RT_SERIAL_CONFIG_DEFAULT;
    rt_err_t result = 0;

    /* init rti_uart object */
    rti_serial.ops    = &rti_uart_ops;
    rti_serial.config = config;

    /* register rti_uart device */
    result = rt_hw_serial_register(&rti_serial, RTI_UART_NAME,
                                   RT_DEVICE_FLAG_RDWR | RT_DEVICE_FLAG_INT_RX,
                                   NULL);
    return result;
}
INIT_APP_EXPORT(rti_uart_init);

/*
    nuvoTon N76E003 MCU driver

    Copyright  (C)  2016 - 2017 Topband. Ltd.

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be a reference
    to you, when you are integrating the nuvoTon's N76E003 IC into your system,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    General Public License for more details.

    Author: shenhaibo
    Version: 1.0.0
    Release Date: 2018/12/4
*/


#ifndef _CHIPONE_N76E003_H_
#define _CHIPONE_N76E003_H_

#include <linux/kernel.h>
#include <linux/hrtimer.h>
#include <linux/i2c.h>
#include <linux/input.h>
#include <linux/module.h>
#include <linux/delay.h>
#include <linux/i2c.h>
#include <linux/proc_fs.h>
#include <linux/string.h>
#include <linux/uaccess.h>
#include <linux/vmalloc.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/gpio.h>
#include <linux/regulator/consumer.h>
#include <linux/major.h>
#include <linux/kdev_t.h>
#include <linux/of_gpio.h>
#ifdef CONFIG_HAS_EARLYSUSPEND
#include <linux/earlysuspend.h>
#endif
#include <linux/usb.h>
#include <linux/power_supply.h>
#include <linux/miscdevice.h>

#define N76E003_I2C_NAME      "n76e003"
#define N76E003_DRIVER_VERSION  "1.0.0"

#define N76E003_ADDR_LENGTH      1
#define I2C_MAX_TRANSFER_SIZE   255
#define RETRY_MAX_TIMES         3

// Register address
#define REG_CHIPID                  0x00
#define REG_HEARTBEAT               0x01
#define REG_SET_UPTIME              0x02
#define REG_SWITCH_WATCHDOG       0x03
#define REG_SET_WATCHDOG_DURATION  0x04

#define DATA_HEARTBEAT              0x55
#define DATA_ON                     0x01
#define DATA_OFF                    0x02


// ioctl cmd
#define N76E003_IOC_MAGIC  'k'

#define N76E003_IOC_HEARTBEAT               _IO(N76E003_IOC_MAGIC, 1)
#define N76E003_IOC_SET_UPTIME              _IOW(N76E003_IOC_MAGIC, 2, int)
#define N76E003_IOC_SWITCH_WATCHDOG         _IOW(N76E003_IOC_MAGIC, 3, int)
#define N76E003_IOC_SET_WATCHDOG_DURATION   _IOW(N76E003_IOC_MAGIC, 4, int)

#define N76E003_IOC_MAXNR 4

struct n76e003_data {
    struct i2c_client *client;
    struct miscdevice n76e003_device;
};

#endif /*_CHIPONE_N76E003_H_*/


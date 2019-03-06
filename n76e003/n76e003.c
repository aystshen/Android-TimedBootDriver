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

    Notes:
    1. IIC address of N76E003: 0x61.
    2. The IIC rate should not exceed 100KHz.

    Author: shenhaibo
    Version: 1.0.0
    Release Date: 2018/12/4
*/

#include <linux/irq.h>
#include <linux/platform_device.h>
#include <linux/pinctrl/consumer.h>
#include <linux/input/mt.h>
#include <linux/slab.h>
#include <linux/fs.h>
#include "n76e003.h"

/*******************************************************
    Function:
    Write data to the i2c slave device.
    Input:
    client: i2c device.
    buf[0]: write start address.
    buf[1~len-1]: data buffer
    len: N76E003_ADDR_LENGTH + write bytes count
    Output:
    numbers of i2c_msgs to transfer:
        0: succeed, otherwise: failed
 *********************************************************/
int n76e003_i2c_write(struct i2c_client *client, u8 *buf, int len)
{
    unsigned int pos = 0, transfer_length = 0;
    u8 address = buf[0];
    unsigned char put_buf[64];
    int retry, ret = 0;
    struct i2c_msg msg = {
        .addr = client->addr,
        .flags = !I2C_M_RD,
    };

    if(likely(len < sizeof(put_buf))) {
        /* code optimize,use stack memory*/
        msg.buf = &put_buf[0];
    } else {
        msg.buf = kmalloc(len > I2C_MAX_TRANSFER_SIZE
                          ? I2C_MAX_TRANSFER_SIZE : len, GFP_KERNEL);

        if(!msg.buf)
            return -ENOMEM;
    }

    len -= N76E003_ADDR_LENGTH;

    while(pos != len) {
        if(unlikely(len - pos > I2C_MAX_TRANSFER_SIZE - N76E003_ADDR_LENGTH))
            transfer_length = I2C_MAX_TRANSFER_SIZE - N76E003_ADDR_LENGTH;
        else
            transfer_length = len - pos;

        msg.buf[0] = address;
        msg.len = transfer_length + N76E003_ADDR_LENGTH;
        memcpy(&msg.buf[N76E003_ADDR_LENGTH], &buf[N76E003_ADDR_LENGTH + pos], transfer_length);

        for(retry = 0; retry < RETRY_MAX_TIMES; retry++) {
            if(likely(i2c_transfer(client->adapter, &msg, 1) == 1)) {
                pos += transfer_length;
                address += transfer_length;
                break;
            }

            dev_info(&client->dev, "I2C write retry[%d]\n", retry + 1);
            udelay(2000);
        }

        if(unlikely(retry == RETRY_MAX_TIMES)) {
            dev_err(&client->dev,
                    "I2c write failed,dev:%02x,reg:%02x,size:%u\n",
                    client->addr, address, len);
            ret = -EAGAIN;
            goto write_exit;
        }
    }

write_exit:

    if(len + N76E003_ADDR_LENGTH >= sizeof(put_buf))
        kfree(msg.buf);

    return ret;
}

/*******************************************************
    Function:
    Read data from the i2c slave device.
    Input:
    client: i2c device.
    buf[0]: read start address.
    buf[1~len-1]: data buffer
    len: N76E003_ADDR_LENGTH + read bytes count
    Output:
    numbers of i2c_msgs to transfer:
        0: succeed, otherwise: failed
 *********************************************************/
int n76e003_i2c_read(struct i2c_client *client, u8 *buf, int len)
{
    unsigned int transfer_length = 0;
    unsigned int pos = 0;
    u8 address = buf[0];
    unsigned char get_buf[64], addr_buf[2];
    int retry, ret = 0;
    struct i2c_msg msgs[] = {
        {
            .addr = client->addr,
            .flags = !I2C_M_RD,
            .buf = &addr_buf[0],
            .len = N76E003_ADDR_LENGTH,
        }, {
            .addr = client->addr,
            .flags = I2C_M_RD,
        }
    };

    len -= N76E003_ADDR_LENGTH;

    if(likely(len < sizeof(get_buf))) {
        /* code optimize, use stack memory */
        msgs[1].buf = &get_buf[0];
    } else {
        msgs[1].buf = kzalloc(len > I2C_MAX_TRANSFER_SIZE
                              ? I2C_MAX_TRANSFER_SIZE : len, GFP_KERNEL);

        if(!msgs[1].buf)
            return -ENOMEM;
    }

    while(pos != len) {
        if(unlikely(len - pos > I2C_MAX_TRANSFER_SIZE))
            transfer_length = I2C_MAX_TRANSFER_SIZE;
        else
            transfer_length = len - pos;

        msgs[0].buf[0] = address;
        msgs[1].len = transfer_length;

        for(retry = 0; retry < RETRY_MAX_TIMES; retry++) {
            if(likely(i2c_transfer(client->adapter, msgs, 2) == 2)) {
                memcpy(&buf[N76E003_ADDR_LENGTH + pos], msgs[1].buf, transfer_length);
                pos += transfer_length;
                address += transfer_length;
                break;
            }

            dev_info(&client->dev, "I2c read retry[%d]:0x%x\n",
                     retry + 1, address);
            udelay(2000);
        }

        if(unlikely(retry == RETRY_MAX_TIMES)) {
            dev_err(&client->dev,
                    "I2c read failed,dev:%02x,reg:%02x,size:%u\n",
                    client->addr, address, len);
            ret = -EAGAIN;
            goto read_exit;
        }
    }

read_exit:

    if(len >= sizeof(get_buf))
        kfree(msgs[1].buf);

    return ret;
}

int n76e003_write(struct i2c_client *client, u8 addr, u8 data)
{
    u8 buf[2] = {addr, data};
    int ret = -1;

    ret = n76e003_i2c_write(client, buf, 2);

    return ret;
}

u8 n76e003_read(struct i2c_client *client, u8 addr)
{
    u8 buf[2] = {addr};
    int ret = -1;

    ret = n76e003_i2c_read(client, buf, 2);

    if(ret == 0) {
        return buf[1];
    } else {
        return 0;
    }
}

static int n76e003_parse_dt(struct device *dev,
                              struct n76e003_data *pdata)
{
    return 0;
}

static int n76e003_request_io_port(struct n76e003_data *n76e003)
{
    return 0;
}

void n76e003_reset_guitar(struct i2c_client *client)
{

}

static int n76e003_i2c_test(struct i2c_client *client)
{
    u8 retry = 0;
    u8 chip_id = 0;
    int ret = -EAGAIN;

    while(retry++ < 3) {
        chip_id = n76e003_read(client, REG_CHIPID);
        // N76E003 i2c test success chipid: 0x76
        dev_info(&client->dev, "N76E003 chipid: 0x%x\n", chip_id);

        if (chip_id != 0x76) {
            dev_err(&client->dev, "N76E003 i2c test failed time %d\n", retry);
            continue;
        }

        ret = 0;
        break;
    }

    return ret;
}

void n76e003_heartbeat(struct i2c_client *client)
{
    n76e003_write(client, REG_HEARTBEAT, DATA_HEARTBEAT);
}

void n76e003_set_uptime(struct i2c_client *client, unsigned long time)
{
    u8 buf[5] = {REG_SET_UPTIME, (time>>24&0xff), (time>>16&0xff), (time>>8&0xff), (time&0xff)};
    n76e003_i2c_write(client, buf, 5);
}

void n76e003_switch_watchdog(struct i2c_client *client, int onoff)
{
    if (onoff > 0) {
        n76e003_write(client, REG_SWITCH_WATCHDOG, DATA_ON);
    } else {
        n76e003_write(client, REG_SWITCH_WATCHDOG, DATA_OFF);
    }
}

void n76e003_set_watchdog_duration(struct i2c_client *client, unsigned long time)
{
    u8 buf[3] = {REG_SET_WATCHDOG_DURATION, (time>>8&0xff), (time&0xff)};
    n76e003_i2c_write(client, buf, 3);
}

static int n76e003_dev_open(struct inode *inode, struct file *filp)
{
	int ret = 0;

	struct n76e003_data *n76e003 = container_of(filp->private_data,
							   struct n76e003_data,
							   n76e003_device);
	filp->private_data = n76e003;
	dev_info(&n76e003->client->dev,
		 "device node major=%d, minor=%d\n", imajor(inode), iminor(inode));

	return ret;
}


static long n76e003_dev_ioctl(struct file *pfile,
					 unsigned int cmd, unsigned long arg)
{
    int ret = 0;
    int data = 0;
	struct n76e003_data *n76e003 = pfile->private_data;

    if (_IOC_TYPE(cmd) != N76E003_IOC_MAGIC) 
        return -EINVAL;
    if (_IOC_NR(cmd) > N76E003_IOC_MAXNR) 
        return -EINVAL;

    if (_IOC_DIR(cmd) & _IOC_READ)
        ret = !access_ok(VERIFY_WRITE, (void *)arg, _IOC_SIZE(cmd));
    else if (_IOC_DIR(cmd) & _IOC_WRITE)
        ret = !access_ok(VERIFY_READ, (void *)arg, _IOC_SIZE(cmd));
    if (ret) 
        return -EFAULT;

    dev_info(&n76e003->client->dev,
                 "%s, (%x, %lx):\n", __func__, cmd,
                 arg);
    
	switch (cmd) {
    	case N76E003_IOC_HEARTBEAT:
            n76e003_heartbeat(n76e003->client);
    		break;
        
    	case N76E003_IOC_SET_UPTIME:
            if (copy_from_user(&data, (int *)arg, sizeof(int))) {
                dev_err(&n76e003->client->dev, 
                    "%s, copy from user failed\n", __func__);
                return -EFAULT;
            }
    		n76e003_set_uptime(n76e003->client, data);
    		break;

        case N76E003_IOC_SWITCH_WATCHDOG:
            if (copy_from_user(&data, (int *)arg, sizeof(int))) {
                dev_err(&n76e003->client->dev, 
                    "%s, copy from user failed\n", __func__);
                return -EFAULT;
            }
    		n76e003_switch_watchdog(n76e003->client, data);
    		break;

        case N76E003_IOC_SET_WATCHDOG_DURATION:
            if (copy_from_user(&data, (int *)arg, sizeof(int))) {
                dev_err(&n76e003->client->dev, 
                    "%s, copy from user failed\n", __func__);
                return -EFAULT;
            }
    		n76e003_set_watchdog_duration(n76e003->client, data);
    		break;
            
    	default:
    		return -EINVAL;
	}

	return 0;
}


static const struct file_operations n76e003_dev_fops = {
	.owner = THIS_MODULE,
    .open = n76e003_dev_open,
	.unlocked_ioctl = n76e003_dev_ioctl
};

static int n76e003_probe(struct i2c_client * client, const struct i2c_device_id * id)
{
    int ret = -1;
    struct n76e003_data *n76e003;

    /* do NOT remove these logs */
    dev_info(&client->dev, "N76E003 Driver Version: %s\n", N76E003_DRIVER_VERSION);
    dev_info(&client->dev, "N76E003 I2C Address: 0x%02x\n", client->addr);

    if(!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
        dev_err(&client->dev, "Failed check I2C functionality");
        return -ENODEV;
    }

    n76e003 = devm_kzalloc(&client->dev, sizeof(*n76e003), GFP_KERNEL);

    if(n76e003 == NULL) {
        dev_err(&client->dev, "Failed alloc n76e003 memory");
        return -ENOMEM;
    }

    if(client->dev.of_node) {
        ret = n76e003_parse_dt(&client->dev, n76e003);

        if(ret < 0) {
            dev_err(&client->dev, "Failed parse dn76e003\n");
            goto exit_free_client_data;
        }
    }

    n76e003->client = client;
    i2c_set_clientdata(client, n76e003);

    ret = n76e003_request_io_port(n76e003);

    if(ret < 0) {
        dev_err(&client->dev, "Failed request IO port\n");
        goto exit_free_client_data;
    }

    n76e003_reset_guitar(client);

    ret = n76e003_i2c_test(client);
    ret = 0;
    if(ret < 0) {
        dev_err(&client->dev, "Failed communicate with IC use I2C\n");
        goto exit_free_io_port;
    }

    n76e003->n76e003_device.minor = MISC_DYNAMIC_MINOR;
	n76e003->n76e003_device.name = "n76e003";
	n76e003->n76e003_device.fops = &n76e003_dev_fops;

	ret = misc_register(&n76e003->n76e003_device);
	if (ret) {
		dev_err(&client->dev, "Failed misc_register\n");
		goto exit_misc_register;
	}

    dev_info(&client->dev, "N76E003 setup finish.\n");

    return 0;
    
exit_misc_register:

exit_free_io_port:

exit_free_client_data:
    devm_kfree(&client->dev, n76e003);
    i2c_set_clientdata(client, NULL);

    return ret;
}

static int n76e003_remove(struct i2c_client * client)
{
    struct n76e003_data *n76e003 = i2c_get_clientdata(client);

    dev_info(&client->dev, "goodix n76e003 driver removed");

    misc_deregister(&n76e003->n76e003_device);
    i2c_set_clientdata(client, NULL);
    devm_kfree(&client->dev, n76e003);

    return 0;
}

static const struct of_device_id n76e003_match_table[] = {
    {.compatible = "nuvoton,n76e003",},
    { },
};

static const struct i2c_device_id n76e003_device_id[] = {
    { N76E003_I2C_NAME, 0 },
    { }
};

static struct i2c_driver n76e003_driver = {
    .probe      = n76e003_probe,
    .remove     = n76e003_remove,
    .id_table   = n76e003_device_id,
    .driver = {
        .name     = N76E003_I2C_NAME,
        .owner    = THIS_MODULE,
        .of_match_table = n76e003_match_table,
    },
};

static int __init n76e003_init(void)
{
    s32 ret;

    pr_info("nuvoTon N76E003 driver installing....\n");
    ret = i2c_add_driver(&n76e003_driver);

    return ret;
}

static void __exit n76e003_exit(void)
{
    pr_info("nuvoTon N76E003 driver exited\n");
    i2c_del_driver(&n76e003_driver);
}

module_init(n76e003_init);
module_exit(n76e003_exit);

MODULE_AUTHOR("ayst.shen@foxmail.com");
MODULE_DESCRIPTION("nuvoTon N76E003 Driver");
MODULE_LICENSE("GPL V2");


#include <linux/types.h>
#include <linux/cdev.h>
#include <linux/kernel.h>
#include <linux/delay.h>
#include <linux/ide.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/of_address.h>
#include <linux/of_gpio.h>
#include <linux/gpio.h>
#include <linux/i2c.h>

#define DEV_NAME "ap3216c"
#define DEV_CNT  1

#define AP3216C_SYSTEMCONG    0x00
#define AP3216C_INTSTATUS     0x01
#define AP3216C_INTCLEAR      0x02
#define AP3216C_IRDATALOW     0x0a
#define AP3216C_IRDATAHIGH    0x0b
#define AP3216C_ALSDATALOW    0x0c
#define AP3216C_ALSDATAHIGH   0x0d
#define AP3216C_PSDATALOW     0x0e
#define AP3216C_PSDATAHIGH    0x0f

struct ap3216c_dev {
    dev_t devid;
    unsigned int major;
    unsigned int minor;
    struct cdev cdev;
    struct class *pclass;
    struct device *pdevice;
    void *private_data;
    unsigned short ir;
    unsigned short als;
    unsigned short ps;
};

static struct ap3216c_dev ap3216c;

static int ap3216c_read_regs(struct ap3216c_dev *dev, u8 reg, u8 *data, int len )
{
    int ret;
    struct i2c_msg msg[2];
    struct i2c_client *client = (struct i2c_client *)dev->private_data;

    msg[0].addr = client->addr;
    msg[0].flags = 0; //发送数据
    msg[0].buf = &reg; //读取的首地址
    msg[0].len = 1;

    msg[1].addr = client->addr;
    msg[1].flags = I2C_M_RD; //发送数据
    msg[1].buf = data; //读取的首地址
    msg[1].len = len;

    ret = i2c_transfer(client->adapter, msg, 2);

    if (ret == 2)
        ret = 0;

    return ret;
}

static unsigned char ap3216c_read_reg(struct ap3216c_dev *dev, u8 reg)
{
    u8 data = 0;

    ap3216c_read_regs(dev, reg, &data, 1);
    
    return data;
}

static void ap3216c_readdata(struct ap3216c_dev *dev)
{
    int i = 0;
    unsigned char buf[6];

    for (; i< 6; i++)
        buf[i] = ap3216c_read_reg(dev, AP3216C_IRDATALOW + i);

    if (buf[0] & 0x80)
        dev->ir = 0;
    else
        dev->ir = ((unsigned short)buf[1] << 2) | (buf[0] & 0x03);

    dev->als = ((unsigned short)buf[3] << 8) | buf[2];

    if (buf[4] & 0x40)
        dev->ps = 0;
    else
        dev->ps = ((unsigned short)(buf[5] & 0x3f) << 4) | (buf[4] & 0x0f);
}

static ssize_t ap3216c_read(struct file *filp, char __user *buf,
    size_t sz, loff_t *offt)
{
    short data[3];
    struct ap3216c_dev *dev = (struct ap3216c_dev*)filp->private_data;

    ap3216c_readdata(dev);

    data[0] = dev->ir;
    data[1] = dev->als;
    data[2] = dev->ps;

    copy_to_user(buf, data, sizeof(data));

    return 0;
}

static int ap3216c_write_regs(struct ap3216c_dev *dev, u8 reg, u8 *buf, int len)
{
    u8 b[256] = { 0 };
    struct i2c_msg msg;
    struct i2c_client * client = (struct i2c_client *)dev->private_data;
    b[0] = reg; //寄存器首地址
    memcpy(&b[1], buf, len);

    msg.addr = client->addr; //设备地址
    msg.flags = 0; //写数据
    msg.buf = b;
    msg.len = len + 1;

    return i2c_transfer(client->adapter, &msg, 1); //此处的1表示要发送的msg数量
}

static void ap3216c_write_reg(struct ap3216c_dev *dev, u8 reg, u8 data)
{
    u8 buf = 0;
    buf = data;
    ap3216c_write_regs(dev, reg, &buf, 1);
}

static int ap3216c_open(struct inode *node, struct file *filp)
{
    filp->private_data = &ap3216c; 

    ap3216c_write_reg(&ap3216c, AP3216C_SYSTEMCONG, 0x04);
    mdelay(50);
    ap3216c_write_reg(&ap3216c, AP3216C_SYSTEMCONG, 0x03);

    printk("open ap3216c\n");

    return 0;
}

static int ap3216c_release(struct inode *node, struct file *filp)
{
    return 0;
}

static struct file_operations ap3216c_ops = {
    .owner   = THIS_MODULE,
    .read    = ap3216c_read,
    .open    = ap3216c_open,
    .release = ap3216c_release,
};

static const struct i2c_device_id ap3216c_id[] = {
    { "alientek,ap3216c", 0 },
    { }
};

/*有设备树方式匹配表*/
static const struct of_device_id ap3216c_of_match[] = {
    { .compatible = "alientek,ap3216c" },
    { }
};

static int ap3216c_probe(struct i2c_client *client,
    const struct i2c_device_id *device_id)
{
    if (alloc_chrdev_region(&ap3216c.devid, 0, 1, DEV_NAME)) {
        printk("alloc_chrdev_region: allocate devid faild\r\n");
        return -1;
    }

    ap3216c.major = MAJOR(ap3216c.devid);
    ap3216c.minor = MINOR(ap3216c.devid);

    printk("major:%d, minor:%d\r\n", ap3216c.major, ap3216c.minor);

    cdev_init(&ap3216c.cdev, &ap3216c_ops);
    if (cdev_add(&ap3216c.cdev, ap3216c.devid, 1)) {
        printk("cdev_init: init dev faild\r\n");
        return -1;
    }

    ap3216c.pclass = class_create(THIS_MODULE, DEV_NAME);

	if(IS_ERR(ap3216c.pclass))
		return PTR_ERR(ap3216c.pclass);

    ap3216c.pdevice = device_create(ap3216c.pclass, NULL, ap3216c.devid, NULL, DEV_NAME);

	if (IS_ERR(ap3216c.pdevice))
		return PTR_ERR(ap3216c.pdevice);

    ap3216c.private_data = client;

    return 0;
}

static int ap3216c_remove(struct i2c_client *client)
{
    cdev_del(&ap3216c.cdev);
    unregister_chrdev_region(ap3216c.devid, 1);
    device_destroy(ap3216c.pclass, ap3216c.devid);
    class_destroy(ap3216c.pclass);
    return 0;
}

static struct i2c_driver ap3216c_driver = {
    .probe  = ap3216c_probe,
    .remove = ap3216c_remove,
    .driver = {
        .owner          = THIS_MODULE,
        .name           = "ap3216c",
        .of_match_table = ap3216c_of_match,
    },
    .id_table = ap3216c_id,
};

static int ap3216c_init(void)
{
    return i2c_add_driver(&ap3216c_driver);
}

static void ap3216c_exit(void)
{
   i2c_del_driver(&ap3216c_driver);
}

module_init(ap3216c_init);
module_exit(ap3216c_exit);
MODULE_AUTHOR("Wenjie Wang <ww107587@gmail.com>");
MODULE_DESCRIPTION("AP3216 Driver");
MODULE_LICENSE("GPL");

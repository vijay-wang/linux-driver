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
#include <linux/spi/spi.h>

#define DEV_NAME "icm20608"
#define DEV_CNT  1

#define AP3216C_SYSTEMCONG        0x00
#define AP3216C_INTSTATUS         0x01
#define AP3216C_INTCLEAR          0x02
#define AP3216C_IRDATALOW         0x0a
#define AP3216C_IRDATAHIGH        0x0b
#define AP3216C_ALSDATALOW        0x0c
#define AP3216C_ALSDATAHIGH       0x0d
#define AP3216C_PSDATALOW         0x0e
#define AP3216C_PSDATAHIGH        0x0f

struct icm20608_dev {
        dev_t devid;
        unsigned int major;
        unsigned int minor;
        struct cdev cdev;
        struct class *pclass;
        struct device *pdevice;
        void *private_data;
        signed int gyro_x;
        signed int gyro_y;
        signed int gyro_z;
        signed int accel_x;
        signed int accel_y;
        signed int accel_z;
        signed int temp;
};

static struct icm20608_dev icm20608;

//static int icm20608_read_regs(struct icm20608_dev *dev, u8 reg, u8 *data, int len )
//{
//        return 0;
//}
//
//static unsigned char icm20608_read_reg(struct icm20608_dev *dev, u8 reg)
//{
//   
//        return 0;
//}
//
//static void icm20608_readdata(struct icm20608_dev *dev)
//{
//}
//
static ssize_t icm20608_read(struct file *filp, char __user *buf,
        size_t sz, loff_t *offt)
{

        return 0;
}

//static int icm20608_write_regs(struct icm20608_dev *dev, u8 reg, u8 *buf, int len)
//{
//        return 0;
//}
//
//static void icm20608_write_reg(struct icm20608_dev *dev, u8 reg, u8 data)
//{
//}

static int icm20608_open(struct inode *node, struct file *filp)
{
        filp->private_data = &icm20608; 
        printk("open icm20608\n");

        return 0;
}

static int icm20608_release(struct inode *node, struct file *filp)
{
        return 0;
}

static struct file_operations icm20608_ops = {
	.owner   = THIS_MODULE,
	.read    = icm20608_read,
	.open    = icm20608_open,
        .release = icm20608_release,
};

static const struct spi_device_id icm20608_id[] = {
        { "alientek,icm20608", 0 },
        { }
};

/*有设备树方式匹配表*/
static const struct of_device_id icm20608_of_match[] = {
        { .compatible = "alientek,icm20608" },
        { }
};

static int icm20608_probe(struct spi_device *spidev)
{
        if (alloc_chrdev_region(&icm20608.devid, 0, 1, DEV_NAME)) {
                printk("alloc_chrdev_region: allocate devid faild\r\n");
                return -1;
        }

        icm20608.major = MAJOR(icm20608.devid);
        icm20608.minor = MINOR(icm20608.devid);

        printk("major:%d, minor:%d\r\n", icm20608.major, icm20608.minor);

        cdev_init(&icm20608.cdev, &icm20608_ops);
        if (cdev_add(&icm20608.cdev, icm20608.devid, 1)) {
                printk("cdev_init: init dev faild\r\n");
                return -1;
        }

        icm20608.pclass = class_create(THIS_MODULE, DEV_NAME);

	if(IS_ERR(icm20608.pclass))
		return PTR_ERR(icm20608.pclass);

        icm20608.pdevice = device_create(icm20608.pclass, NULL, icm20608.devid, NULL, DEV_NAME);

	if (IS_ERR(icm20608.pdevice))
		return PTR_ERR(icm20608.pdevice);


        return 0;
}

static int icm20608_remove(struct spi_device *spidev)
{
        cdev_del(&icm20608.cdev);
        unregister_chrdev_region(icm20608.devid, 1);
        device_destroy(icm20608.pclass, icm20608.devid);
        class_destroy(icm20608.pclass);
        return 0;
}

static struct spi_driver icm20608_driver = {
        .probe  = icm20608_probe,
        .remove = icm20608_remove,
        .driver = {
                .owner = THIS_MODULE,
                .name  = "icm20608",
                .of_match_table = icm20608_of_match,
        },
        .id_table = icm20608_id,
};

static int icm20608_init(void)
{
        return spi_register_driver(&icm20608_driver);
}

static void icm20608_exit(void)
{
        spi_unregister_driver(&icm20608_driver);
}

module_init(icm20608_init);
module_exit(icm20608_exit);
MODULE_AUTHOR("Wenjie Wang <ww107587@gmail.com>");
MODULE_DESCRIPTION("AP3216 Driver");
MODULE_LICENSE("GPL");

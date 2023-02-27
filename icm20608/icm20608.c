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
#include <linux/gfp.h>

#define DEV_NAME "icm20608"
#define DEV_CNT  1

/* 加速度输出 */
#define ICM20_ACCEL_XOUT_H          0x3B
#define ICM20_ACCEL_XOUT_L          0x3C
#define ICM20_ACCEL_YOUT_H          0x3D
#define ICM20_ACCEL_YOUT_L          0x3E
#define ICM20_ACCEL_ZOUT_H          0x3F
#define ICM20_ACCEL_ZOUT_L          0x40

/* 温度输出 */
#define ICM20_TEMP_OUT_H            0x41
#define ICM20_TEMP_OUT_L            0x42

/* 陀螺仪输出 */
#define ICM20_GYRO_XOUT_H           0x43
#define ICM20_GYRO_XOUT_L           0x44
#define ICM20_GYRO_YOUT_H           0x45
#define ICM20_GYRO_YOUT_L           0x46
#define ICM20_GYRO_ZOUT_H           0x47
#define ICM20_GYRO_ZOUT_L           0x48

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

static int icm20608_write_regs(struct icm20608_dev *dev, u8 reg,
        u8 *data, int len)
{
        int ret = -1;
        unsigned char txdata[1];
        unsigned char *rxdata;
        struct spi_message m;
        struct spi_transfer *t;
        struct spi_device *spi = (struct spi_device*)dev->private_data;

        t = kzalloc(sizeof(struct spi_transfer), GFP_KERNEL);

        if (!t) {
                return -ENOMEM;
        }

        rxdata = kzalloc(sizeof(char) * len, GFP_KERNEL);

        if(!rxdata) {
                goto out1; 
        }

        txdata[0] = reg & ~0x80;
        memcpy(txdata + 1, data, len);
        t->tx_buf = txdata;
        t->len = len + 1;
        spi_message_init(&m);
        spi_message_add_tail(t, &m);
        ret = spi_sync(spi, &m);

        if (ret) {
            goto out2;
        }

out2:
        kfree(rxdata);

out1:
        kfree(t);


        return ret;
}

static unsigned char icm20608_read_regs(struct icm20608_dev *dev, u8 reg,
        void *buf, int len)
{
        int ret = -1;
        unsigned char txdata[1];
        unsigned char *rxdata;
        struct spi_message m;
        struct spi_transfer *t;
        struct spi_device *spi = (struct spi_device*)dev->private_data;

        t = kzalloc(sizeof(struct spi_transfer), GFP_KERNEL);

        if (!t) {
                return -ENOMEM;
        }

        rxdata = kzalloc(sizeof(char) * len, GFP_KERNEL);

        if(!rxdata) {
                goto out1; 
        }

        txdata[0] = reg | 0x80;
        t->tx_buf = txdata;
        t->rx_buf = rxdata;
        t->len = len + 1;
        spi_message_init(&m);
        spi_message_add_tail(t, &m);
        ret = spi_sync(spi, &m);

        if (ret) {
            goto out2;
        }

        memcpy(buf, rxdata + 1, len);

out2:
        kfree(rxdata);

out1:
        kfree(t);

        return ret;
}

static void icm20608_write_onereg(struct icm20608_dev *dev, u8 reg, u8 value)
{
    u8 buf = value;
    icm20608_write_regs(dev, reg, &buf, 1);
}

static unsigned char icm20608_read_onereg(struct icm20608_dev *dev, u8 reg)
{
    u8 data = 0;
    icm20608_read_regs(dev, reg, &data, 1);
    return data;
}

static void icm20608_readdata(struct icm20608_dev *dev)
{
        unsigned char data[14] = { 0 };

        icm20608_read_regs(dev, ICM20_ACCEL_XOUT_H, data, 14);

        dev->gyro_x  = (signed short)((data[0] << 8) | data[1]);
        dev->gyro_y  = (signed short)((data[2] << 8) | data[3]);
        dev->gyro_z  = (signed short)((data[4] << 8) | data[5]);
        dev->temp    = (signed short)((data[6] << 8) | data[7]);
        dev->accel_x = (signed short)((data[8] << 8) | data[9]);
        dev->accel_y = (signed short)((data[10] << 8) | data[11]);
        dev->accel_z = (signed short)((data[12] << 8) | data[13]);

}

static ssize_t icm20608_read(struct file *filp, char __user *buf,
        size_t sz, loff_t *offt)
{
        signed int data[7];
        int ret;
        struct icm20608_dev *dev = (struct icm20608_dev *)filp->private_data;

        icm20608_readdata(dev);

        data[0] = dev->gyro_x;
        data[1] = dev->gyro_y;
        data[2] = dev->gyro_z;
        data[3] = dev->accel_x;
        data[4] = dev->accel_y;
        data[5] = dev->accel_z;
        data[6] = dev->temp;

        ret = copy_to_user(buf, data, sizeof(data));

        return sizeof(data);
}

static void icm20608_reginit(void)
{
        unsigned char who_am_i;

        icm20608_write_onereg(&icm20608, 0x6b, 0x80);
        mdelay(100);
        icm20608_write_onereg(&icm20608, 0x6b, 0x01);
        icm20608_write_onereg(&icm20608, 0x6B, 0x01); // 唤醒设备
        icm20608_write_onereg(&icm20608, 0x1B, 0x18); // 设置陀螺仪量程为2000dps
        icm20608_write_onereg(&icm20608, 0x1C, 0x18); // 设置加速度计量程为16g)))

        // 读取WHO_AM_I寄存器
        who_am_i = icm20608_read_onereg(&icm20608, 0x75);
        printk(KERN_INFO "ICM20608 WHO_AM_I = 0x%x\n", who_am_i);

}

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

    spidev->mode = SPI_MODE_0; /*CPOL=0, CPHA=0*/
    spi_setup(spidev);
    icm20608.private_data = spidev;

    icm20608_reginit();

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
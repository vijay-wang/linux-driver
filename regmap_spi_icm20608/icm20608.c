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
#include <linux/regmap.h>

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
        struct regmap *regmap;
        struct regmap_config regmap_config;
};

static void icm20608_write_onereg(struct icm20608_dev *dev, u8 reg, u8 value)
{
    regmap_write(dev->regmap, reg, value);
}

static unsigned char icm20608_read_onereg(struct icm20608_dev *dev, u8 reg)
{
    int data = 0;
    regmap_read(dev->regmap, reg, &data);
    return (u8)data;
}

static void icm20608_readdata(struct icm20608_dev *dev)
{
        unsigned char data[14] = { 0 };

        regmap_bulk_read(dev->regmap, ICM20_ACCEL_XOUT_H, data, 14);

        dev->accel_x = (signed short)((data[0] << 8) | data[1]);
        dev->accel_y = (signed short)((data[2] << 8) | data[3]);
        dev->accel_z = (signed short)((data[4] << 8) | data[5]);
        dev->temp    = (signed short)((data[6] << 8) | data[7]);
        dev->gyro_x  = (signed short)((data[8] << 8) | data[9]);
        dev->gyro_y  = (signed short)((data[10] << 8) | data[11]);
        dev->gyro_z  = (signed short)((data[12] << 8) | data[13]);
}

static ssize_t icm20608_read(struct file *filp, char __user *buf,
        size_t sz, loff_t *offt)
{
        signed int data[7];
        int ret;
        struct cdev *cdev = filp->f_path.dentry->d_inode->i_cdev;
        struct icm20608_dev *dev = container_of(cdev, struct icm20608_dev, cdev);

        icm20608_readdata(dev);

        data[0] = dev->gyro_x;
        data[1] = dev->gyro_y;
        data[2] = dev->gyro_z;
        data[3] = dev->accel_x;
        data[4] = dev->accel_y;
        data[5] = dev->accel_z;
        data[6] = dev->temp;

        ret = copy_to_user(buf, data, sizeof(data));

        //return sizeof(data);
        return 0;
}

static void icm20608_reginit(struct icm20608_dev *dev)
{
        unsigned char who_am_i;

        icm20608_write_onereg(dev, 0x6b, 0x80);
        mdelay(100);
        icm20608_write_onereg(dev, 0x6b, 0x01);
        mdelay(100);

        icm20608_write_onereg(dev, 0x19, 0x00);                /*输出速率是内部采样率                */
        icm20608_write_onereg(dev, 0x1b, 0x18); // 设置陀螺仪量程为2000dps
        icm20608_write_onereg(dev, 0x1c, 0x18); // 设置加速度计量程为16g)))

        icm20608_write_onereg(dev, 0x1a, 0x04);        /* 陀螺仪低通滤波BW=20Hz                */
        icm20608_write_onereg(dev, 0x1d, 0x04); /* 加速度计低通滤波BW=21.2Hz            */
        icm20608_write_onereg(dev, 0x6c, 0x00);    /* 打开加速度计和陀螺仪所有轴               */
        icm20608_write_onereg(dev, 0x1e, 0x00);   /* 关闭低功耗                       */
        icm20608_write_onereg(dev, 0x23, 0x00);       /* 关闭FIFO                     */

        // 读取WHO_AM_I寄存器
        who_am_i = icm20608_read_onereg(dev, 0x75);
        printk(KERN_INFO "ICM20608 WHO_AM_I = 0x%x\n", who_am_i);

}

static int icm20608_open(struct inode *node, struct file *filp)
{
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
        struct icm20608_dev *icm20608 = devm_kzalloc(&spidev->dev, sizeof(*icm20608), GFP_KERNEL);   

        if(!icm20608)
                return -ENOMEM;

        icm20608->regmap_config.reg_bits = 8;
        icm20608->regmap_config.val_bits = 8;
        icm20608->regmap_config.read_flag_mask = 0x80;

        icm20608->regmap = regmap_init_spi(spidev, &icm20608->regmap_config);
        if(IS_ERR(icm20608->regmap))
                return PTR_ERR(icm20608->regmap);

        if (alloc_chrdev_region(&icm20608->devid, 0, 1, DEV_NAME)) {
                printk("alloc_chrdev_region: allocate devid faild\r\n");
                goto del_regmap;
        }

        icm20608->major = MAJOR(icm20608->devid);
        icm20608->minor = MINOR(icm20608->devid);

        printk("major:%d, minor:%d\r\n", icm20608->major, icm20608->minor);

        cdev_init(&icm20608->cdev, &icm20608_ops);
        if (cdev_add(&icm20608->cdev, icm20608->devid, 1)) {
                printk("cdev_init: init dev faild\r\n");
                goto del_unregister;
        }

        icm20608->pclass = class_create(THIS_MODULE, DEV_NAME);

        if(IS_ERR(icm20608->pclass))
                goto del_cdev;

        icm20608->pdevice = device_create(icm20608->pclass, NULL, icm20608->devid, NULL, DEV_NAME);

        if (IS_ERR(icm20608->pdevice))
                goto destroy_class;

        spidev->mode = SPI_MODE_0; /*CPOL=0, CPHA=0*/
        spi_setup(spidev);
        icm20608->private_data = spidev;

        icm20608_reginit(icm20608);

        return 0;

destroy_class:
        device_destroy(icm20608->pclass, icm20608->devid);

del_cdev:
        cdev_del(&icm20608->cdev);

del_unregister:
        unregister_chrdev_region(icm20608->devid, 1);

del_regmap:
        regmap_exit(icm20608->regmap);

        return -EIO;
}

static int icm20608_remove(struct spi_device *spidev)
{
        struct icm20608_dev *icm20608 = spi_get_drvdata(spidev);   
        cdev_del(&icm20608->cdev);
        unregister_chrdev_region(icm20608->devid, 1);
        device_destroy(icm20608->pclass, icm20608->devid);
        class_destroy(icm20608->pclass);
        regmap_exit(icm20608->regmap);

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

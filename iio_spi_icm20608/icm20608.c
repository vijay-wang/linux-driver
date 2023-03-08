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
#include <linux/iio/iio.h>
#include <linux/iio/sysfs.h>
#include <linux/iio/buffer.h>
#include <linux/iio/trigger.h>
#include <linux/iio/triggered_buffer.h>
#include <linux/iio/trigger_consumer.h>
#include <linux/unaligned/be_byteshift.h>
#include "icm20608.h"

#define  ICM20608_NAME "icm20608"
#define DEV_CNT  1
#define ICM20608_TEMP_OFFSET	     0
#define ICM20608_TEMP_SCALE		     326800000

enum inv_icm20608_scan {
	INV_ICM20608_SCAN_ACCL_X,
	INV_ICM20608_SCAN_ACCL_Y,
	INV_ICM20608_SCAN_ACCL_Z,
	INV_ICM20608_SCAN_TEMP,
	INV_ICM20608_SCAN_GYRO_X,
	INV_ICM20608_SCAN_GYRO_Y,
	INV_ICM20608_SCAN_GYRO_Z,
	INV_ICM20608_SCAN_TIMESTAMP,
};

#define ICM20608_CHAN(_type, _channel2, _index)                 \
        {                                                       \
                .type = _type,                                  \
                .modified = 1,                                  \
                .channel2 = _channel2,                          \
                .info_mask_separate = BIT(IIO_CHAN_INFO_RAW)    \
                        | BIT(IIO_CHAN_INFO_CALIBBIAS),         \
                .info_mask_shared_by_type = BIT(IIO_CHAN_INFO_SCALE),    \
                .scan_index = _index,                           \
                .scan_type = {                                  \
                        .sign = 's',                            \
                        .realbits = 16,                         \
                        .storagebits = 16,                      \
                        .shift = 0,                             \
                        .endianness = IIO_BE,                   \
                },                                              \
        }

struct icm20608_dev {
        struct spi_device *spi;
        struct mutex lock;
        struct regmap *regmap;
        struct regmap_config regmap_config;
};

static const int accel_scale_icm20608[] = {61035, 122070, 244140, 488281};
static const int gyro_scale_icm20608[] = {7629, 15258, 30517, 61035};

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

static int icm20608_sensor_show(struct icm20608_dev *dev, int reg,
				   int axis, int *val)
{
        int ind, result;
        __be16 d;

        ind = (axis - IIO_MOD_X) * 2;
        result = regmap_bulk_read(dev->regmap, reg + ind, (u8 *)&d, 2);
        if (result)
                return -EINVAL;
        *val = (short)be16_to_cpup(&d);

        return IIO_VAL_INT;
}

static int icm20608_read_channel_data(struct iio_dev *indio_dev,
					 struct iio_chan_spec const *chan,
					 int *val)
{
        struct icm20608_dev *dev = iio_priv(indio_dev);
        int ret = 0;

        switch (chan->type) {
        case IIO_ANGL_VEL:	/* 读取陀螺仪数据 */
                ret = icm20608_sensor_show(dev, ICM20_GYRO_XOUT_H, chan->channel2, val);  /* channel2为X、Y、Z轴 */
                break;
        case IIO_ACCEL:		/* 读取加速度计数据 */
                ret = icm20608_sensor_show(dev, ICM20_ACCEL_XOUT_H, chan->channel2, val); /* channel2为X、Y、Z轴 */
                break;
        case IIO_TEMP:		/* 读取温度 */
                ret = icm20608_sensor_show(dev, ICM20_TEMP_OUT_H, IIO_MOD_X, val);  
                break;
        default:
                ret = -EINVAL;
                break;
        }
        return ret;
}

static int icm20608_read_raw(struct iio_dev *indio_dev,
			struct iio_chan_spec const *chan, int *val,
			int *val2, long mask)
{
        struct icm20608_dev *dev = iio_priv(indio_dev);   
        int ret = 0;
        unsigned char regdata = 0;

        switch (mask) {
        case IIO_CHAN_INFO_RAW:								/* 读取ICM20608加速度计、陀螺仪、温度传感器原始值 */
                mutex_lock(&dev->lock);								/* 上锁 			*/
                ret = icm20608_read_channel_data(indio_dev, chan, val); 	/* 读取通道值 */
                mutex_unlock(&dev->lock);							/* 释放锁 			*/
                return ret;
        case IIO_CHAN_INFO_SCALE:
                switch (chan->type) {
                case IIO_ANGL_VEL:
                        mutex_lock(&dev->lock);
                        regdata = (icm20608_read_onereg(dev, ICM20_GYRO_CONFIG) & 0X18) >> 3;
                        *val  = 0;
                        *val2 = gyro_scale_icm20608[regdata];
                        mutex_unlock(&dev->lock);
                        return IIO_VAL_INT_PLUS_MICRO;	/* 值为val+val2/1000000 */
                case IIO_ACCEL:
                        mutex_lock(&dev->lock);
                        regdata = (icm20608_read_onereg(dev, ICM20_ACCEL_CONFIG) & 0X18) >> 3;
                        *val = 0;
                        *val2 = accel_scale_icm20608[regdata];;
                        mutex_unlock(&dev->lock);
                        return IIO_VAL_INT_PLUS_NANO;/* 值为val+val2/1000000000 */
                case IIO_TEMP:					
                        *val = ICM20608_TEMP_SCALE/ 1000000;
                        *val2 = ICM20608_TEMP_SCALE % 1000000;
                        return IIO_VAL_INT_PLUS_MICRO;	/* 值为val+val2/1000000 */
                default:
                        return -EINVAL;
                }
                return ret;
        case IIO_CHAN_INFO_OFFSET:		/* ICM20608温度传感器offset值 */
                switch (chan->type) {
                case IIO_TEMP:
                        *val = ICM20608_TEMP_OFFSET;
                        return IIO_VAL_INT;
                default:
                        return -EINVAL;
                }
                return ret;
        case IIO_CHAN_INFO_CALIBBIAS:	/* ICM20608加速度计和陀螺仪校准值 */
                switch (chan->type) {
                case IIO_ANGL_VEL:		/* 陀螺仪的校准值 */
                        mutex_lock(&dev->lock);
                        ret = icm20608_sensor_show(dev, ICM20_XG_OFFS_USRH, chan->channel2, val);
                        mutex_unlock(&dev->lock);
                        return ret;
                case IIO_ACCEL:			/* 加速度计的校准值 */
                        mutex_lock(&dev->lock);	
                        ret = icm20608_sensor_show(dev, ICM20_XA_OFFSET_H, chan->channel2, val);
                        mutex_unlock(&dev->lock);
                        return ret;
                default:
                        return -EINVAL;
                }
            
        default:
                return ret -EINVAL;
        }
}

static int icm20608_write_gyro_scale(struct icm20608_dev *dev, int val)
{
        int result, i;
        u8 d;

        for (i = 0; i < ARRAY_SIZE(gyro_scale_icm20608); i++) {
                if (gyro_scale_icm20608[i] == val) {
                        d = (i << 3);
                        result = regmap_write(dev->regmap, ICM20_GYRO_CONFIG, d);
                        if (result)
                            return result;
                        return 0;
                }
        }

        return -EINVAL;
}

static int icm20608_write_accel_scale(struct icm20608_dev *dev, int val)
{
	int result, i;
	u8 d;

	for (i = 0; i < ARRAY_SIZE(accel_scale_icm20608); ++i) {
		if (accel_scale_icm20608[i] == val) {
			d = (i << 3);
			result = regmap_write(dev->regmap, ICM20_ACCEL_CONFIG, d);
			if (result)
				return result;
			return 0;
		}
	}
	return -EINVAL;
}

static int icm20608_sensor_set(struct icm20608_dev *dev, int reg,
				int axis, int val)
{
	int ind, result;
	__be16 d = cpu_to_be16(val);

	ind = (axis - IIO_MOD_X) * 2;
	result = regmap_bulk_write(dev->regmap, reg + ind, (u8 *)&d, 2);
	if (result)
		return -EINVAL;

	return 0;
}

static int icm20608_write_raw(struct iio_dev *indio_dev,
			 struct iio_chan_spec const *chan, int val,
			 int val2, long mask)
{
        struct icm20608_dev *dev = iio_priv(indio_dev);  
        int ret = 0;

        switch (mask) {
        case IIO_CHAN_INFO_SCALE: 
                switch (chan->type) {
                case IIO_ANGL_VEL:
                        mutex_lock(&dev->lock);
                        ret = icm20608_write_gyro_scale(dev, val2);
                        mutex_unlock(&dev->lock);
                case IIO_ACCEL:
                        mutex_lock(&dev->lock);
                        ret = icm20608_write_accel_scale(dev, val2);
                        mutex_unlock(&dev->lock);
                default:
                        ret = -EINVAL;
                        break;
                }
                break;
        case IIO_CHAN_INFO_CALIBBIAS:
                switch (chan->type) {
                case IIO_ANGL_VEL:		/* 设置陀螺仪校准值 */
                    mutex_lock(&dev->lock);
                    ret = icm20608_sensor_set(dev, ICM20_XG_OFFS_USRH,
                                                chan->channel2, val);
                    mutex_unlock(&dev->lock);
                    break;
                case IIO_ACCEL:			/* 加速度计校准值 */
                    mutex_lock(&dev->lock);
                    ret = icm20608_sensor_set(dev, ICM20_XA_OFFSET_H,
                                                 chan->channel2, val);
                    mutex_unlock(&dev->lock);
                    break;
                default:
                    ret = -EINVAL;
                    break;
                }
        default:
                ret = -EINVAL;
                break;
        }
        
        return ret;
}

static int icm20608_write_raw_get_fmt(struct iio_dev *indio_dev,
			 struct iio_chan_spec const *chan, long mask)
{
    	switch (mask) {
        case IIO_CHAN_INFO_SCALE:
            switch (chan->type) {
            case IIO_ANGL_VEL:		/* 用户空间写的陀螺仪分辨率数据要乘以1000000 */
                return IIO_VAL_INT_PLUS_MICRO;
            default:				/* 用户空间写的加速度计分辨率数据要乘以1000000000 */
                return IIO_VAL_INT_PLUS_NANO;
            }
        default:
            return IIO_VAL_INT_PLUS_MICRO;
        }
        return -EINVAL;
}

static const struct spi_device_id icm20608_id[] = {
        { "alientek,icm20608", 0 },
        { }
};

/*有设备树方式匹配表*/
static const struct of_device_id icm20608_of_match[] = {
        { .compatible = "alientek,icm20608" },
        { }
};

static const struct iio_info icm20608_info = {
        .read_raw          = icm20608_read_raw,
        .write_raw         = icm20608_write_raw,
        .write_raw_get_fmt = &icm20608_write_raw_get_fmt
};

static const struct iio_chan_spec icm20608_channels[] = {
        {
                .type = IIO_TEMP,
                .info_mask_separate = BIT(IIO_CHAN_INFO_RAW)
                        | BIT(IIO_CHAN_INFO_OFFSET)
                        | BIT(IIO_CHAN_INFO_SCALE),
                .scan_index = INV_ICM20608_SCAN_TEMP,
                .scan_type = {
                        .sign = 's', //s signed , u unsigned
                        .realbits = 16,
                        .storagebits = 16,
                        .shift = 0,
                        .endianness = IIO_BE,
                },
        },

        ICM20608_CHAN(IIO_ANGL_VEL, IIO_MOD_X, INV_ICM20608_SCAN_GYRO_X),	/* 陀螺仪X轴 */
        ICM20608_CHAN(IIO_ANGL_VEL, IIO_MOD_Y, INV_ICM20608_SCAN_GYRO_Y),	/* 陀螺仪Y轴 */
        ICM20608_CHAN(IIO_ANGL_VEL, IIO_MOD_Z, INV_ICM20608_SCAN_GYRO_Z),	/* 陀螺仪Z轴 */

        ICM20608_CHAN(IIO_ACCEL, IIO_MOD_Y, INV_ICM20608_SCAN_ACCL_Y),	/* 加速度X轴 */
        ICM20608_CHAN(IIO_ACCEL, IIO_MOD_X, INV_ICM20608_SCAN_ACCL_X),	/* 加速度Y轴 */
        ICM20608_CHAN(IIO_ACCEL, IIO_MOD_Z, INV_ICM20608_SCAN_ACCL_Z),	/* 加速度Z轴 */
};

static int icm20608_probe(struct spi_device *spi)
{
        int ret;
        struct icm20608_dev *dev;
        struct iio_dev *indio_dev;

        indio_dev = devm_iio_device_alloc(&spi->dev, sizeof(*dev));
        if(!indio_dev)
                return -ENOMEM;

        dev = iio_priv(indio_dev);
        dev ->spi = spi;
        spi_set_drvdata(spi, indio_dev);
        mutex_init(&dev->lock);

        indio_dev->dev.parent = &spi->dev;
        indio_dev->info = &icm20608_info;
        indio_dev->name = ICM20608_NAME;
        indio_dev->modes = INDIO_DIRECT_MODE;
        indio_dev->channels = icm20608_channels;
        indio_dev->num_channels = ARRAY_SIZE(icm20608_channels);

        ret = iio_device_register(indio_dev);
        if(ret < 0) {
                dev_err(&spi->dev, "iio_device_register failed\n");
                goto err_iio_register;
        }

        dev->regmap_config.reg_bits = 8;
        dev->regmap_config.val_bits = 8;
        dev->regmap_config.read_flag_mask = 0x80;

        dev->regmap = regmap_init_spi(spi, &dev->regmap_config);
        if(IS_ERR(dev->regmap)) {
                ret = PTR_ERR(dev->regmap);
                goto err_regmap_init;
        }

        spi->mode = SPI_MODE_0; /*CPOL=0, CPHA=0*/
        spi_setup(spi);

        icm20608_reginit(dev);

        return 0;

err_regmap_init:
        iio_device_unregister(indio_dev);

err_iio_register:
        return ret;
}

static int icm20608_remove(struct spi_device *spi)
{
        struct iio_dev *indio_dev = spi_get_drvdata(spi);   
        struct icm20608_dev *dev;

        dev = iio_priv(indio_dev);

        regmap_exit(dev->regmap);
        iio_device_unregister(indio_dev);

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
MODULE_DESCRIPTION("ICM20608 Driver");
MODULE_LICENSE("GPL");

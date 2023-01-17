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
#include <linux/atomic.h>


#define KEY0_NAME "key0" 

enum key0_state_t {
	KEY0_DOWN,
	KEY0_UP,
};

struct key0_dev {
	dev_t devid;
	unsigned int major;
	unsigned int minor;
	struct cdev cdev;
	struct class *pclass;
	struct device *pdevice;
	struct device_node *nd; /*设备树节点*/	
	int gpio_key0; /*number of gpio pin*/
	atomic_t key_val;
};

struct key0_dev key;

static int open_key0(struct inode *inode, struct file *filp)
{
	filp->private_data = &key;
	return 0;
}

static int release_key0(struct inode *inode, struct file *filp)
{
	return 0;
}

static ssize_t read_key0(struct file *filp, char __user *buf,
		size_t cnt, loff_t *offt)
{
	unsigned int value;
	unsigned char ret_buf[1];
	struct key0_dev *pdev = filp->private_data;
	if (gpio_get_value(pdev->gpio_key0) == 0) {
		atomic_set(&(pdev->key_val), 1);
	} else {
		atomic_set(&(pdev->key_val), 0);
	}

	value = atomic_read(&(pdev->key_val));

	if (value == 0)
		ret_buf[0] = '0';

	if (value == 1)
		ret_buf[0] = '1';

	if(copy_to_user(buf, ret_buf, sizeof(ret_buf)))
		return -EFAULT;
	return 1;
}

static ssize_t write_key0(struct file *filp, const char __user *buf,
		size_t cnt, loff_t *offt)
{
	return 0;
}


static struct file_operations key0_fops = {
	.owner = THIS_MODULE,
	.open = open_key0,
	.release = release_key0,
	.read = read_key0,
	.write = write_key0
};

static int __init key0_init(void)
{
	int ret;
	const char *str;
	struct property *proper;



	/*get device node of key0*/
	key.nd = of_find_node_by_path("/pinctrlgpiokey0");

	if (key.nd == NULL) {
		printk("key0: can't find pinctrlgpiokey0 node!\r\n");
		return -EINVAL;
	} else {
		printk("key0: find pinctrlgpiokey0 node!\r\n");
	}

	/*print compatible property*/
	proper = of_find_property(key.nd, "compatible", NULL);

	if (proper == NULL) {
		printk("key0: can't find compatible property!\r\n");
		return -EINVAL;
	} else {
		printk("key0: find compatile property [compatible = %s]\r\n", (char*)proper->value);
	}

	/*print status property*/
	ret = of_property_read_string(key.nd, "status", &str);

	if (ret < 0) {
		printk("key0: read status failed!\r\n");
		return -EINVAL;
	} else {
		printk("key0: [status = %s]\r\n", str);
	}

	/*get the number of gpio pin*/
	key.gpio_key0 = of_get_named_gpio(key.nd, "key0-gpio", 0);

	if (key.gpio_key0 < 0) {
		printk("key0: get gpio-key0 failed!\r\n");
		return -EINVAL;
	} else {
		printk("key0: key0-gpio number: %d\r\n", key.gpio_key0);
	}

	/*set direction*/
	ret = gpio_direction_input(key.gpio_key0);

	if (ret < 0) {
		printk("key0: set direction of key0 faild!\r\n");
		return -EINVAL;
	}

	/*register dev number*/
	if(alloc_chrdev_region(&key.devid, 0, 1, KEY0_NAME)) {
		printk(KERN_ERR "key0: allocate devid for pinctrlgpiokey0 faild!\r\n");
		return -EFAULT;
	}

	key.major = MAJOR(key.devid);
	key.minor = MINOR(key.devid);

	printk(KERN_INFO "key0: allocate devid for new_key0 major:[%u] moinor:[%u]\r\n", key.major, key.minor);

	/*Initializes cdev, remembering fops, make cdev ready to add to systerm with cdev_add.*/
	cdev_init(&key.cdev, &key0_fops);
	cdev_add(&key.cdev, key.devid, 1);

	/*auto mmnode /dev/pinctrlgpiokey0 */
	key.pclass = class_create(THIS_MODULE, KEY0_NAME);

	if(IS_ERR(key.pclass))
		return PTR_ERR(key.pclass);

	key.pdevice = device_create(key.pclass, NULL, key.devid, NULL, KEY0_NAME);

	if (IS_ERR(key.pdevice))
		return PTR_ERR(key.pdevice);

	printk(KERN_INFO "key0: mknode /dev/%s for module %s\r\n", KEY0_NAME, KEY0_NAME);
	printk(KERN_INFO "key0: load %s success!\r\n", KEY0_NAME);

	return 0;	
}

static void __exit key0_exit(void)
{
	cdev_del(&key.cdev);
	unregister_chrdev_region(key.devid, 1);

	device_destroy(key.pclass, key.devid);
	class_destroy(key.pclass);

	printk(KERN_INFO "key0: %s module has been removed!\r\n", KEY0_NAME);
}

module_init(key0_init);
module_exit(key0_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("wang");

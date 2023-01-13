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


#define BEEP_NAME "pinctrlgpio_beep" 

#define GPIO1_DR		(0x0209c000)

static void __iomem *MAP_GPIO1_DR;

enum beep_state_t {
	BEEP_OFF,
	BEEP_ON,
};

struct pinctrlgpiobeep_dev {
	dev_t devid;
	unsigned int major;
	unsigned int minor;
	struct cdev cdev;
	struct class *pclass;
	struct device *pdevice;
	struct device_node *nd; /*设备树节点*/	
	int gpio_beep; /*number of gpio pin*/
};

struct pinctrlgpiobeep_dev pinctrlgpiobeep;

static int open_beep(struct inode *inode, struct file *filp)
{
	filp->private_data = &pinctrlgpiobeep;
	return 0;
}

static int release_beep(struct inode *inode, struct file *filp)
{
	return 0;
}

static ssize_t read_beep(struct file *filp, char __user *buf,
		size_t cnt, loff_t *offt)
{
	u32 val;
	char beep_state[2] = { 0	};

	val = readl(MAP_GPIO1_DR);
	val = (val >> 3);
	val &= (0x00000001);

	if (val == 0) {
		beep_state[0] = '1';
	}

	if (val == 1) {
		beep_state[0] = '0';
	}

	if(copy_to_user(buf, beep_state, sizeof(beep_state))) {
		printk(KERN_ERR "beep read faild!\r\n");
		return -1;
	}

	return strlen(beep_state);
}

static ssize_t write_beep(struct file *filp, const char __user *buf,
		size_t cnt, loff_t *offt)
{
	char user_data[2] = { '\0' };
	struct pinctrlgpiobeep_dev *beep_dev = filp->private_data;

	if(copy_from_user(user_data, buf, 2)) {
		
		printk(KERN_ERR "copy frome user  faild!\r\n");
		return -1;
	}


	if (user_data[0] == '1') {
		/*open beep*/
		gpio_set_value(beep_dev->gpio_beep, 0);
		printk(KERN_INFO "beep change state to %c\r\n", user_data[0]);
	}

	if (user_data[0] == '0') {
		/*close beep*/
		gpio_set_value(beep_dev->gpio_beep, 1);
		printk(KERN_INFO "beep change state to %c\r\n", user_data[0]);
	}

	//在4.15内核的echo 写的时候传到字符串到底层驱动，会超出实际想写入长度，并且传到驱动后会出现乱码,
	//因此copy_from_user的时候会覆盖掉user_data[1]中的\0，导致strlen(user_data)会出现随机长短
	//因此返回写入长度的时候不能用strlen,否则会报错
	return (cnt > (sizeof(user_data) - 1) ? (sizeof(user_data) - 1) : cnt);
}


static struct file_operations beep_fops = {
	.owner = THIS_MODULE,
	.open = open_beep,
	.release = release_beep,
	.read = read_beep,
	.write = write_beep
};

static int __init init_beep(void)
{
	int ret;
	const char *str;
	struct property *proper;

	MAP_GPIO1_DR = ioremap(GPIO1_DR, 4);

	/*get device node of beep*/
	pinctrlgpiobeep.nd = of_find_node_by_path("/pinctrlgpiobeep");

	if (pinctrlgpiobeep.nd == NULL) {
		printk("beep: can't find pinctrlgpiobeep node!\r\n");
		return -EINVAL;
	} else {
		printk("beep: find pinctrlgpiobeep node!\r\n");
	}

	/*print compatible property*/
	proper = of_find_property(pinctrlgpiobeep.nd, "compatible", NULL);

	if (proper == NULL) {
		printk("beep: can't find compatible property!\r\n");
		return -EINVAL;
	} else {
		printk("beep: find compatile property [compatible = %s]\r\n", (char*)proper->value);
	}

	/*print status property*/
	ret = of_property_read_string(pinctrlgpiobeep.nd, "status", &str);

	if (ret < 0) {
		printk("beep: read status failed!\r\n");
		return -EINVAL;
	} else {
		printk("beep: [status = %s]\r\n", str);
	}

	/*get the number of gpio pin*/
	pinctrlgpiobeep.gpio_beep = of_get_named_gpio(pinctrlgpiobeep.nd, "beep-gpio", 0);

	if (pinctrlgpiobeep.gpio_beep < 0) {
		printk("beep: get gpio-beep failed!\r\n");
		return -EINVAL;
	} else {
		printk("beep: beep-gpio number: %d\r\n", pinctrlgpiobeep.gpio_beep);
	}

	/*set direction*/
	ret = gpio_direction_output(pinctrlgpiobeep.gpio_beep, 1);

	if (ret < 0) {
		printk("beep: set direction of beep faild!\r\n");
		return -EINVAL;
	}

	/*register dev number*/
	if(alloc_chrdev_region(&pinctrlgpiobeep.devid, 0, 1, BEEP_NAME)) {
		printk(KERN_ERR "beep: allocate devid for pinctrlgpiobeep faild!\r\n");
		return -EFAULT;
	}

	pinctrlgpiobeep.major = MAJOR(pinctrlgpiobeep.devid);
	pinctrlgpiobeep.minor = MINOR(pinctrlgpiobeep.devid);

	printk(KERN_INFO "beep: allocate devid for new_beep major:[%u] moinor:[%u]\r\n", pinctrlgpiobeep.major, pinctrlgpiobeep.minor);

	/*Initializes cdev, remembering fops, make cdev ready to add to systerm with cdev_add.*/
	cdev_init(&pinctrlgpiobeep.cdev, &beep_fops);
	cdev_add(&pinctrlgpiobeep.cdev, pinctrlgpiobeep.devid, 1);

	/*auto mmnode /dev/pinctrlgpiobeep */
	pinctrlgpiobeep.pclass = class_create(THIS_MODULE, BEEP_NAME);

	if(IS_ERR(pinctrlgpiobeep.pclass))
		return PTR_ERR(pinctrlgpiobeep.pclass);

	pinctrlgpiobeep.pdevice = device_create(pinctrlgpiobeep.pclass, NULL, pinctrlgpiobeep.devid, NULL, BEEP_NAME);

	if (IS_ERR(pinctrlgpiobeep.pdevice))
		return PTR_ERR(pinctrlgpiobeep.pdevice);

	printk(KERN_INFO "beep: mknode /dev/%s for module %s\r\n", BEEP_NAME, BEEP_NAME);
	printk(KERN_INFO "beep: load %s success!\r\n", BEEP_NAME);

	return 0;	
}

static void __exit exit_beep(void)
{
	cdev_del(&pinctrlgpiobeep.cdev);
	unregister_chrdev_region(pinctrlgpiobeep.devid, 1);

	device_destroy(pinctrlgpiobeep.pclass, pinctrlgpiobeep.devid);
	class_destroy(pinctrlgpiobeep.pclass);

	printk(KERN_INFO "beep: %s module has been removed!\r\n", BEEP_NAME);
}

module_init(init_beep);
module_exit(exit_beep);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("wang");

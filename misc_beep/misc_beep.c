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
#include <linux/miscdevice.h>

#define GPIO5_DR		(0x020ac000)

static void __iomem *MAP_GPIO5_DR;

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

	val = readl(MAP_GPIO5_DR);
	val = (val >> 1);
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

struct miscdevice miscbeep = {
    .name = "misc_beep",
    .minor = 3,
    .fops = &beep_fops,
};

static int __init init_beep(void)
{
	int ret;
	const char *str;
	struct property *proper;

	MAP_GPIO5_DR = ioremap(GPIO5_DR, 4);

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

    if (misc_register(&miscbeep)) {
        printk("misc device register failed!\r\n");
        return -EFAULT; 
    }

	return 0;	
}

static void __exit exit_beep(void)
{
	iounmap(MAP_GPIO5_DR);

    if (misc_deregister(&miscbeep)) {
        printk("misc device deregister failed!\r\n");
    }

	printk(KERN_INFO "misc_beep module has been removed!\r\n");
}

module_init(init_beep);
module_exit(exit_beep);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("wang");

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
#include <linux/platform_device.h>


#define LED_NAME "pinctrlgpio_led" 

#define GPIO1_DR		(0x0209c000)

static void __iomem *MAP_GPIO1_DR;

enum led_state_t {
	LED_OFF,
	LED_ON,
};

struct pinctrlgpioled_dev {
	dev_t devid;
	unsigned int major;
	unsigned int minor;
	struct cdev cdev;
	struct class *pclass;
	struct device *pdevice;
	struct device_node *nd; /*设备树节点*/	
	int gpio_led; /*number of gpio pin*/
};

struct pinctrlgpioled_dev pinctrlgpioled;

static int open_led(struct inode *inode, struct file *filp)
{
	filp->private_data = &pinctrlgpioled;
	return 0;
}

static int release_led(struct inode *inode, struct file *filp)
{
	return 0;
}

#if 0
static void led_on(struct pinctrlgpioled_dev *dev)
{
	//struct pinctrlgpioled_dev *led_dev = dev;
	gpio_set_value(dev->gpio_led, 0);
}

static void led_off(struct pinctrlgpioled_dev *dev)
{
	//struct pinctrlgpioled_dev *led_dev = dev;
	gpio_set_value(dev->gpio_led, 1);
}

static void led_switch(enum led_state_t state, struct pinctrlgpioled_dev *dev)
{
	if (state == LED_OFF)
		led_off(dev);
		
	if (state == LED_ON)
		led_on(dev);
}
#endif

static ssize_t read_led(struct file *filp, char __user *buf,
		size_t cnt, loff_t *offt)
{
	u32 val;
	char led_state[2] = { 0	};

	val = readl(MAP_GPIO1_DR);
	val = (val >> 3);
	val &= (0x00000001);

	if (val == 0) {
		led_state[0] = '1';
	}

	if (val == 1) {
		led_state[0] = '0';
	}

	if(copy_to_user(buf, led_state, sizeof(led_state))) {
		printk(KERN_ERR "led read faild!\r\n");
		return -1;
	}

	return strlen(led_state);
}

static ssize_t write_led(struct file *filp, const char __user *buf,
		size_t cnt, loff_t *offt)
{
	char user_data[2] = { '\0' };
	struct pinctrlgpioled_dev *led_dev = filp->private_data;

	if(copy_from_user(user_data, buf, 2)) {
		
		printk(KERN_ERR "copy frome user  faild!\r\n");
		return -1;
	}


	if (user_data[0] == '1') {
	//	led_switch(LED_ON, led_dev);
		/*open led*/
		gpio_set_value(led_dev->gpio_led, 0);
		printk(KERN_INFO "led change state to %c\r\n", user_data[0]);
	}

	if (user_data[0] == '0') {
	//	led_switch(LED_OFF, led_dev);
		/*close led*/
		gpio_set_value(led_dev->gpio_led, 1);
		printk(KERN_INFO "led change state to %c\r\n", user_data[0]);
	}

	//在4.15内核的echo 写的时候传到字符串到底层驱动，会超出实际想写入长度，并且传到驱动后会出现乱码,
	//因此copy_from_user的时候会覆盖掉user_data[1]中的\0，导致strlen(user_data)会出现随机长短
	//因此返回写入长度的时候不能用strlen,否则会报错
	return (cnt > (sizeof(user_data) - 1) ? (sizeof(user_data) - 1) : cnt);
}

static struct file_operations led_fops = {
	.owner = THIS_MODULE,
	.open = open_led,
	.release = release_led,
	.read = read_led,
	.write = write_led
};

static int probe_led(struct platform_device *dev)
{
    int ret;
	const char *str;
	struct property *proper;

	MAP_GPIO1_DR = ioremap(GPIO1_DR, 4);

	/*get device node of led*/
	pinctrlgpioled.nd = of_find_node_by_path("/pinctrlgpioled");

	if (pinctrlgpioled.nd == NULL) {
		printk("can't find pinctrlgpioled node!\r\n");
		return -EINVAL;
	} else {
		printk("find pinctrlgpioled node!\r\n");
	}

	/*print compatible property*/
	proper = of_find_property(pinctrlgpioled.nd, "compatible", NULL);

	if (proper == NULL) {
		printk("can't find compatible property!\r\n");
		return -EINVAL;
	} else {
		printk("find compatile property [compatible = %s]\r\n", (char*)proper->value);
	}

	/*print status property*/
	ret = of_property_read_string(pinctrlgpioled.nd, "status", &str);

	if (ret < 0) {
		printk("read status failed!\r\n");
		return -EINVAL;
	} else {
		printk("[status = %s]\r\n", str);
	}

	/*get the number of gpio pin*/
	pinctrlgpioled.gpio_led = of_get_named_gpio(pinctrlgpioled.nd, "led-gpio", 0);

	if (pinctrlgpioled.gpio_led < 0) {
		printk("get gpio-led failed!\r\n");
		return -EINVAL;
	} else {
		printk("led-gpio number: %d\r\n", pinctrlgpioled.gpio_led);
	}

	/*set direction*/
	ret = gpio_direction_output(pinctrlgpioled.gpio_led, 1);

	if (ret < 0) {
		printk("set direction of led faild!\r\n");
		return -EINVAL;
	}

	/*register dev number*/
	if(alloc_chrdev_region(&pinctrlgpioled.devid, 0, 1, LED_NAME)) {
		printk(KERN_ERR "allocate devid for new_led faild!\r\n");
		return -EFAULT;
	}

	pinctrlgpioled.major = MAJOR(pinctrlgpioled.devid);
	pinctrlgpioled.minor = MINOR(pinctrlgpioled.devid);

	printk(KERN_INFO "allocate devid for new_led major:[%u] moinor:[%u]\r\n", pinctrlgpioled.major, pinctrlgpioled.minor);

	/*Initializes cdev, remembering fops, make cdev ready to add to systerm with cdev_add.*/
	cdev_init(&pinctrlgpioled.cdev, &led_fops);
	cdev_add(&pinctrlgpioled.cdev, pinctrlgpioled.devid, 1);

	/*auto mmnode /dev/pinctrlgpioled */
	pinctrlgpioled.pclass = class_create(THIS_MODULE, LED_NAME);

	if(IS_ERR(pinctrlgpioled.pclass))
		return PTR_ERR(pinctrlgpioled.pclass);

	pinctrlgpioled.pdevice = device_create(pinctrlgpioled.pclass, NULL, pinctrlgpioled.devid, NULL, LED_NAME);

	if (IS_ERR(pinctrlgpioled.pdevice))
		return PTR_ERR(pinctrlgpioled.pdevice);

	printk(KERN_INFO "mknode /dev/%s for module %s\r\n", LED_NAME, LED_NAME);
	printk(KERN_INFO "load %s success!\r\n", LED_NAME);

	return 0;
}

static int remove_led(struct platform_device *dev)
{
 	cdev_del(&pinctrlgpioled.cdev);
	unregister_chrdev_region(pinctrlgpioled.devid, 1);

	device_destroy(pinctrlgpioled.pclass, pinctrlgpioled.devid);
	class_destroy(pinctrlgpioled.pclass);

	printk(KERN_INFO "led module has been removed!\r\n");   

    return 0;
}

static struct of_device_id led_of_match[] = {
        { .compatible = "pinctrlgpioled" },
        { }
};

static struct platform_driver led_driver = {
    .driver = {
        .name = "plf_led",
        .of_match_table = led_of_match,
    },
    .probe = probe_led,
    .remove = remove_led,
};

static int __init init_led(void)
{
    return platform_driver_register(&led_driver);
}

static void __exit exit_led(void)
{
    platform_driver_unregister(&led_driver);
}

module_init(init_led);
module_exit(exit_led);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("wang");

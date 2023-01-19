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
#include <asm/ioctl.h>

#define CLOSE_CMD 	(_IO(0Xef, 0x0))
#define OPEN_CMD 	(_IO(0Xef, 0x1))
#define SETPERIOD_CMD 	(_IO(0Xef, 0x2))

#define LED_NAME 	"timer_led" 
#define GPIO1_DR	(0x0209c000)

static void __iomem *MAP_GPIO1_DR;

struct pinctrlgpioled_dev {
	dev_t devid;
	unsigned int major;
	unsigned int minor;
	struct cdev cdev;
	struct class *pclass;
	struct device *pdevice;
	struct device_node *nd; /*设备树节点*/	
	int gpio_led; /*number of gpio pin*/
	int timeperiod;
	struct timer_list timer;
	spinlock_t lock;
};

struct pinctrlgpioled_dev pinctrlgpioled;

static int open_led(struct inode *inode, struct file *filp)
{
	filp->private_data = &pinctrlgpioled;
	return 0;
}

static long unlocked_ioctl_led(struct file *filp, 
		unsigned int cmd, unsigned long arg)
{
	int timeperiod;
	unsigned long flags;
	struct pinctrlgpioled_dev *dev = filp->private_data;

	switch (cmd) {
		case CLOSE_CMD:
			del_timer_sync(&dev->timer);
			break;			

		case OPEN_CMD:
			spin_lock_irqsave(&dev->lock, flags);
			timeperiod = dev->timeperiod;
			spin_unlock_irqrestore(&dev->lock, flags);
			mod_timer(&dev->timer, jiffies + msecs_to_jiffies(timeperiod));
			break;
			
		case SETPERIOD_CMD:
			spin_lock_irqsave(&dev->lock, flags);
			dev->timeperiod = arg;
			spin_unlock_irqrestore(&dev->lock, flags);
			mod_timer(&dev->timer, jiffies + msecs_to_jiffies(arg));
			break;

		default:
			break;
	}
	return 0;
}

static int release_led(struct inode *inode, struct file *filp)
{
	return 0;
}

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
#if 0
	char user_data[2] = { '\0' };
	struct pinctrlgpioled_dev *led_dev = filp->private_data;

	if(copy_from_user(user_data, buf, 2)) {
		
		printk(KERN_ERR "copy frome user  faild!\r\n");
		return -1;
	}


	if (user_data[0] == '1') {
		/*open led*/
		gpio_set_value(led_dev->gpio_led, 0);
		printk(KERN_INFO "led change state to %c\r\n", user_data[0]);
	}

	if (user_data[0] == '0') {
		/*close led*/
		gpio_set_value(led_dev->gpio_led, 1);
		printk(KERN_INFO "led change state to %c\r\n", user_data[0]);
	}

	//在4.15内核的echo 写的时候传到字符串到底层驱动，会超出实际想写入长度，并且传到驱动后会出现乱码,
	//因此copy_from_user的时候会覆盖掉user_data[1]中的\0，导致strlen(user_data)会出现随机长短
	//因此返回写入长度的时候不能用strlen,否则会报错
	return (cnt > (sizeof(user_data) - 1) ? (sizeof(user_data) - 1) : cnt);
#endif
	return 0;
}

void timer_function(unsigned long arg)
{
	struct pinctrlgpioled_dev *dev = (struct pinctrlgpioled_dev *)arg;
	static int state = 1; /*led default to close*/

	state = !state;
	gpio_set_value(dev->gpio_led, state);

	/*restart timer*/
	mod_timer(&dev->timer, jiffies + msecs_to_jiffies(dev->timeperiod));
}


static struct file_operations led_fops = {
	.owner = THIS_MODULE,
	.open = open_led,
	.release = release_led,
	.read = read_led,
	.write = write_led,
 	.unlocked_ioctl = unlocked_ioctl_led
};

static int __init init_led(void)
{
	int ret;
	const char *str;
	struct property *proper;

	spin_lock_init(&pinctrlgpioled.lock);

	MAP_GPIO1_DR = ioremap(GPIO1_DR, 4);

	pinctrlgpioled.timeperiod = 1000; /*defaut to 1s*/

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

	/*Initialize the timer*/
	init_timer(&pinctrlgpioled.timer);
	pinctrlgpioled.timer.function = timer_function;
	pinctrlgpioled.timer.data = (unsigned long)&pinctrlgpioled;
	ioctl

	return 0;	
}

static void __exit exit_led(void)
{
	gpio_set_value(pinctrlgpioled.gpio_led, 1);
	del_timer_sync(&pinctrlgpioled.timer);
	cdev_del(&pinctrlgpioled.cdev);
	unregister_chrdev_region(pinctrlgpioled.devid, 1);
	device_destroy(pinctrlgpioled.pclass, pinctrlgpioled.devid);
	class_destroy(pinctrlgpioled.pclass);
	printk(KERN_INFO "led module has been removed!\r\n");
}

module_init(init_led);
module_exit(exit_led);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("wang");

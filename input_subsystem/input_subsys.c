#include <linux/module.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/fs.h>
#include <linux/of.h>
#include <linux/interrupt.h>
#include <linux/of_irq.h>
#include <linux/of_gpio.h>
#include <linux/gpio.h>
#include <linux/irqreturn.h>
#include <linux/timer.h>
#include <linux/input.h>
#include <asm/uaccess.h>

#define KEY0_NAME "key0"
#define KEY0IRQ_CNT 1
#define DEV_NAME "key0"

typedef struct {
	unsigned int irq_num;
	irq_handler_t handler;
} key0irq_t;

struct key0_dev {
	int gpio_key0;
	unsigned int major;
	unsigned int minor;
	struct cdev cdev;
	struct class *pclass;
	struct device_node *node;
	struct timer_list timer;
    struct input_dev *inputdev;
	dev_t devid;
	key0irq_t key0irq;
	atomic_t key0_val;
};

struct key0_dev key0;

static int key0_open(struct inode *inode, struct file *filp)
{
	filp->private_data = &key0;
	return 0;	
}

ssize_t key0_read(struct file *filp, char __user *buf, size_t cnt, loff_t *offt)
{
	struct key0_dev *dev = filp->private_data;
	unsigned char key_val;

	key_val = atomic_read(&dev->key0_val);

	if (copy_to_user(buf, &key_val, 1)) {
		printk("%s: copy_to_user failed\r\n", __func__);
	}

	return 1;
}

struct file_operations fops = {
	.owner = THIS_MODULE,
	.open = key0_open,
	.read = key0_read,
};

static int init_gpio1_io18(void)
{
	key0.node = of_find_node_by_path("/pinctrlgpiokey0");
	if (key0.node == NULL) {
		printk("%s: Find node by path failed\r\n", __func__);
		return -1;
	} else {
		printk("Found pinctrlgpiokey0 by path\r\n");
	}

	/*获取引脚编号*/
	key0.gpio_key0 = of_get_named_gpio(key0.node, "key0-gpio", 0);
	if (key0.gpio_key0 < 0) {
		printk("%s: can't get number of gpio1_18\r\n", __func__);	
		return -1;
	}

	/*设置gpio为输出*/
	gpio_request(key0.gpio_key0, "key0");
	if (gpio_direction_input(key0.gpio_key0) < 0) {
		printk("%s: set direction of gpio1_18 failed\r\n", __func__);	
		return -1;
	}

	return 0;
}

static void timer_func(unsigned long arg)
{
	struct key0_dev *dev = (struct key0_dev *)arg;

	if (gpio_get_value(dev->gpio_key0) == 0) {	/*按键按下*/
        input_report_key(key0.inputdev, KEY_0, 1);
        input_sync(key0.inputdev);
	} else {
        input_report_key(key0.inputdev, KEY_0, 0);
        input_sync(key0.inputdev);
	}
}

static irqreturn_t key0_handler(int irq, void *dev_id)
{
	mod_timer(&((struct key0_dev *)dev_id)->timer, jiffies + msecs_to_jiffies(10));
	return IRQ_RETVAL(IRQ_HANDLED);	
}

static int init_irq(void)
{
	key0.key0irq.handler = key0_handler;
	key0.key0irq.irq_num = irq_of_parse_and_map(key0.node, 0);
	printk("%s: gpio1_io18 irq number: %d\r\n", __func__, key0.key0irq.irq_num);	
	if (request_irq(key0.key0irq.irq_num, key0.key0irq.handler,
			 IRQF_TRIGGER_FALLING|IRQF_TRIGGER_RISING, "key0_irq", &key0)) {
		printk("%s: request irq failed\r\n", __func__);	
		return -1;
	}
	
	return 0;
}

static int __init key0_init(void)
{
	/*初始化gpio*/
	if (init_gpio1_io18() < 0) {
		printk("%s: init gpio1_io18 failed\r\n", __func__);	
		return -1;	
	}

	/*初始化中断*/
	init_irq();

	/*初始化定时器*/
	key0.timer.function = timer_func;
	key0.timer.data = (unsigned long)(&key0);
	init_timer(&key0.timer);

    key0.inputdev = input_allocate_device();

    key0.inputdev->name = "keyinput";
    key0.inputdev->evbit[0] = BIT_MASK(EV_KEY) | BIT_MASK(EV_REP);
    input_set_capability(key0.inputdev, EV_KEY, KEY_0);

    if (input_register_device(key0.inputdev)) {
		printk("%s: register input device failed\r\n", __func__);	
        return -1;
    }

	return 0;
}

static void __exit key0_exit(void)
{
	del_timer_sync(&key0.timer);
	free_irq(key0.key0irq.irq_num, &key0);
	gpio_free(key0.gpio_key0);
    input_unregister_device(key0.inputdev);
    input_free_device(key0.inputdev);
}	

module_init(key0_init);
module_exit(key0_exit);
MODULE_LICENSE("GPL");
MODULE_AUTHOR("wang");

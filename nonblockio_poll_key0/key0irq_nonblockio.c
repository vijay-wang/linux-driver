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
#include <linux/sched.h>
#include <linux/poll.h>
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
	wait_queue_head_t r_wait;
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

    DECLARE_WAITQUEUE(wait, current);
    
    if (filp->f_flags & O_NONBLOCK) {
        if (atomic_read(&key0.key0_val) == 0)
            return -EAGAIN; 
    } else {
        add_wait_queue(&dev->r_wait, &wait);
        set_current_state(TASK_INTERRUPTIBLE);
        schedule();
        if (signal_pending(current))
            goto signal_wakeup_error; 
        set_current_state(TASK_RUNNING);
        remove_wait_queue(&dev->r_wait, &wait);   
    }

	key_val = atomic_read(&dev->key0_val);

	if (copy_to_user(buf, &key_val, 1)) {
		printk("%s: copy_to_user failed\r\n", __func__);
        goto data_error;
	}

	return 1;

signal_wakeup_error:
    set_current_state(TASK_RUNNING);
    remove_wait_queue(&dev->r_wait, &wait);
    return -ERESTARTSYS;

data_error:
    return -EINVAL;
}

unsigned int key0_poll(struct file *filp, struct poll_table_struct *wait)
{
        struct key0_dev *dev = filp->private_data;
        int mask = 0;

        poll_wait(filp, &dev->r_wait, wait);

        if (atomic_read(&dev->key0_val) == '1')
            mask = POLLIN;

        return mask;
}

struct file_operations fops = {
	.owner = THIS_MODULE,
	.open = key0_open,
	.read = key0_read,
    .poll = key0_poll,
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
		atomic_set(&dev->key0_val, '1');
	} else {
		atomic_set(&dev->key0_val, '0');
	}

//    wake_up_interruptible(&dev->r_wait);
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

	/*注册设备号*/
	if (!alloc_chrdev_region(&key0.devid, 0, KEY0IRQ_CNT, KEY0_NAME)) {
		key0.major = MAJOR(key0.devid);
		key0.minor = MINOR(key0.devid);
	}

	/*注册字符设备*/
	cdev_init(&key0.cdev, &fops);
	cdev_add(&key0.cdev, key0.devid, KEY0IRQ_CNT);

	/*创建类*/
	key0.pclass = class_create(THIS_MODULE, KEY0_NAME);

	/*创建设备*/
	device_create(key0.pclass, NULL, key0.devid, NULL, DEV_NAME);

	/*初始化gpio*/
	init_gpio1_io18();
	
	/*初始化中断*/
	init_irq();

	/*初始化定时器*/
	key0.timer.function = timer_func;
	key0.timer.data = (unsigned long)(&key0);
	init_timer(&key0.timer);

	/*初始化按键值为0，表示未按下*/
	atomic_set(&key0.key0_val, '0');

	init_waitqueue_head(&key0.r_wait);

	return 0;
}

static void __exit key0_exit(void)
{
	del_timer_sync(&key0.timer);
	free_irq(key0.key0irq.irq_num, &key0);
	gpio_free(key0.gpio_key0);
	cdev_del(&key0.cdev);
	unregister_chrdev_region(key0.devid, 1);
	device_destroy(key0.pclass, key0.devid);
	class_destroy(key0.pclass);
}	

module_init(key0_init);
module_exit(key0_exit);
MODULE_LICENSE("GPL");
MODULE_AUTHOR("wang");

#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/delay.h>
#include <linux/ide.h>
#include <linux/init.h>
#include <linux/cdev.h>
#include <linux/module.h>
#include <linux/platform_device.h>


#define LED_MAJOR 200
#define LED_NAME "plf_led" 

enum led_state_t{
	LED_OFF,
	LED_ON,
};

struct led_dev {
    dev_t devid;            /* 设备号 */
    struct cdev cdev;       /* cdev */
    struct class *pclass;    /* 类 */
    struct device *pdevice;  /* 设备 */
    int major;              /* 主设备号 */    
    int minor;              /* 主设备号 */    
};

struct led_dev newled;

static void __iomem *MAP_SW_MUX_GPIO1_IO03;
static void __iomem *MAP_SW_PAD_GPIO1_IO03;
static void __iomem *MAP_CCM_CCGR1;
static void __iomem *MAP_GPIO1_GDIR;
static void __iomem *MAP_GPIO1_DR;

static void set_pin(volatile void *addr, u32 set_val, int shift_bits)
{
	u32 val;

	val = readl(addr);
	val &= ~(set_val << shift_bits);
	val |= (set_val << shift_bits);
	writel(val, addr);
}

static void led_on(void)
{
	u32 val;

	val = readl(MAP_GPIO1_DR);
	val &= ~(0x00000001 << 3);
	val |= (0x00000000 << 3);
	writel(val, MAP_GPIO1_DR);
}

static void led_off(void)
{
	u32 val;

	val = readl(MAP_GPIO1_DR);
	val &= ~(0x00000001 << 3);
	val |= (0x00000001 << 3);
	writel(val, MAP_GPIO1_DR);
}

static void led_switch(enum led_state_t state)
{
	if (state == LED_OFF)
		led_off();
		
	if (state == LED_ON)
		led_on();
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
	char user_data[2] = {0};

	if(copy_from_user(user_data, buf, 2)) {
		
		printk(KERN_ERR "copy frome user  faild!\r\n");
		return -1;
	}


	if (user_data[0] == '1') {
		led_switch(LED_ON);
		printk(KERN_INFO "led change state to %c\r\n", user_data[0]);
	}

	if (user_data[0] == '0') {
		led_switch(LED_OFF);
		printk(KERN_INFO "led change state to %c\r\n", user_data[0]);
	}

	return strlen(user_data);
}


static struct file_operations led_fops = {
	.owner = THIS_MODULE,
	.read = read_led,
	.write = write_led
};

static int remove_led(struct platform_device *dev)
{
   	iounmap(MAP_SW_MUX_GPIO1_IO03);
	iounmap(MAP_SW_PAD_GPIO1_IO03);
	iounmap(MAP_CCM_CCGR1);
	iounmap(MAP_GPIO1_GDIR);
	iounmap(MAP_GPIO1_DR);

	cdev_del(&newled.cdev);
	unregister_chrdev_region(newled.devid, 1);

	device_destroy(newled.pclass, newled.devid);
	class_destroy(newled.pclass);

	printk(KERN_INFO "led module has been removed!\r\n");
    
    return 0;
}

static int probe_led(struct platform_device *dev)
{
	u32 val;
    int i = 0;
    struct resource *ledsource[5];
    int ressize[5];

    for (; i < 5; i++) {
        ledsource[i] = platform_get_resource(dev, IORESOURCE_MEM, i);
        
        if (!ledsource[i]) {
            dev_err(&dev->dev, "No MEM resource for always on\n");
            return -ENXIO;
        }
        ressize[i] = resource_size(ledsource[i]);
    }

	MAP_SW_MUX_GPIO1_IO03 = ioremap(ledsource[0]->start, ressize[0]);
	MAP_SW_PAD_GPIO1_IO03 = ioremap(ledsource[1]->start, ressize[1]);
	MAP_CCM_CCGR1 = ioremap(ledsource[2]->start, ressize[2]);
	MAP_GPIO1_GDIR = ioremap(ledsource[3]->start, ressize[3]);
	MAP_GPIO1_DR = ioremap(ledsource[4]->start, ressize[4]);

	/*enable gpio1 clk*/
	set_pin(MAP_CCM_CCGR1, 0x00000003,  26);

	/*set IOMUX*/
	set_pin(MAP_SW_MUX_GPIO1_IO03, 0x00000005, 0);
	set_pin(MAP_SW_PAD_GPIO1_IO03, 0x000010b0, 0);

	/*set output direction*/
	set_pin(MAP_GPIO1_GDIR, 0x00000001, 3);

	/*disable led default*/
	val = readl(MAP_GPIO1_DR);
	val &= ~(0x00000001 << 3);
	val |= (0x00000001 << 3);
	writel(val, MAP_GPIO1_DR);

	if(alloc_chrdev_region(&newled.devid, 0, 1, LED_NAME)) {
		printk(KERN_ERR "allocate devid for new_led faild!\r\n");
		return -EFAULT;
	}

	newled.major = MAJOR(newled.devid);
	newled.minor = MINOR(newled.devid);

	printk(KERN_INFO "allocate devid for new_led major:[%u] moinor:[%u]\r\n", newled.major, newled.minor);

	/*Initializes cdev, remembering fops, make cdev ready to add to systerm with cdev_add.*/
	cdev_init(&newled.cdev, &led_fops);
	cdev_add(&newled.cdev, newled.devid, 1);

	/*auto mmnode /dev/newled */
	newled.pclass = class_create(THIS_MODULE, LED_NAME);

	if(IS_ERR(newled.pclass))
		return PTR_ERR(newled.pclass);

	newled.pdevice = device_create(newled.pclass, NULL, newled.devid, NULL, LED_NAME);

	if (IS_ERR(newled.pdevice))
		return PTR_ERR(newled.pdevice);

	printk(KERN_INFO "mknode /dev/%s for module %s\r\n", LED_NAME, LED_NAME);
	printk(KERN_INFO "load %s success!\r\n", LED_NAME);

    return 0;
}

static struct platform_driver led_driver = {
    .driver = {
        .name = "platform_led",
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

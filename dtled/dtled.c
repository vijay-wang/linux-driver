#include <linux/types.h>
#include <linux/cdev.h>
#include <linux/kernel.h>
#include <linux/delay.h>
#include <linux/ide.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/of_address.h>


#define LED_NAME "dt_led" 

#define SW_MUX_GPIO1_IO03	(0x020e0068)
#define SW_PAD_GPIO1_IO03 	(0x020e02f4)
#define CCM_CCGR1		(0x020c406c)
#define GPIO1_GDIR		(0x0209c004)
#define GPIO1_DR		(0x0209c000)



enum led_state_t {
	LED_OFF,
	LED_ON,
};

struct dtled_dev {
	dev_t devid;
	unsigned int major;
	unsigned int minor;
	struct cdev cdev;
	struct class *pclass;
	struct device *pdevice;
	struct device_node *nd; /*设备树节点*/	
};

struct dtled_dev dtled;

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

static int open_led(struct inode *inode, struct file *filp)
{
	return 0;
}

static int release_led(struct inode *inode, struct file *filp)
{
	return 0;
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
	char user_data[2] = { '\0' };

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

static int __init init_led(void)
{
	u32 val;
	int ret;
	u32 regdata[10];
	const char *str;
	struct property *proper;

	dtled.nd = of_find_node_by_path("/alphaled");

	if (dtled.nd == NULL) {
		printk("can't find alphaled node!\r\n");
		return -EINVAL;
	} else {
		printk("find alphaled node!\r\n");
	}

	proper = of_find_property(dtled.nd, "compatible", NULL);

	if (proper == NULL) {
		printk("can't find compatible property!\r\n");
	} else {
		printk("find compatile property [compatible = %s]\r\n", (char*)proper->value);
	}

	ret = of_property_read_string(dtled.nd, "status", &str);

	if (ret < 0) {
		printk("read status failed!\r\n");
	} else {
		printk("[status = %s]\r\n", str);
	}

	ret = of_property_read_u32_array(dtled.nd, "reg", regdata, 10);

	if (ret < 0) {
		printk("reg property read faild!\r\n");
	} else {
		u8 i = 0;
		printk("reg data:\r\n");
		for (; i < 10; i++) {
			printk("[%#X]", regdata[i]);
			printk("\r\n");
		}
	}

#if 0
	MAP_SW_MUX_GPIO1_IO03 = ioremap(regdata[0], regdata[1]);
	MAP_SW_PAD_GPIO1_IO03 = ioremap(regdata[2], regdata[3]);
	MAP_CCM_CCGR1 = ioremap(regdata[4], regdata[5]);
	MAP_GPIO1_GDIR = ioremap(regdata[6], regdata[7]);
	MAP_GPIO1_DR = ioremap(regdata[8], regdata[9]);
#else
	MAP_SW_MUX_GPIO1_IO03 = of_iomap(dtled.nd, 0);
	MAP_SW_PAD_GPIO1_IO03 = of_iomap(dtled.nd, 1);
	MAP_CCM_CCGR1 = of_iomap(dtled.nd, 2);
	MAP_GPIO1_GDIR = of_iomap(dtled.nd, 3);
	MAP_GPIO1_DR = of_iomap(dtled.nd, 4);
#endif

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
	

	if(alloc_chrdev_region(&dtled.devid, 0, 1, LED_NAME)) {
		printk(KERN_ERR "allocate devid for new_led faild!\r\n");
		return -EFAULT;
	}

	dtled.major = MAJOR(dtled.devid);
	dtled.minor = MINOR(dtled.devid);

	printk(KERN_INFO "allocate devid for new_led major:[%u] moinor:[%u]\r\n", dtled.major, dtled.minor);

	/*Initializes cdev, remembering fops, make cdev ready to add to systerm with cdev_add.*/
	cdev_init(&dtled.cdev, &led_fops);
	cdev_add(&dtled.cdev, dtled.devid, 1);

	/*auto mmnode /dev/dtled */
	dtled.pclass = class_create(THIS_MODULE, LED_NAME);

	if(IS_ERR(dtled.pclass))
		return PTR_ERR(dtled.pclass);

	dtled.pdevice = device_create(dtled.pclass, NULL, dtled.devid, NULL, LED_NAME);

	if (IS_ERR(dtled.pdevice))
		return PTR_ERR(dtled.pdevice);

	printk(KERN_INFO "mknode /dev/%s for module %s\r\n", LED_NAME, LED_NAME);
	printk(KERN_INFO "load %s success!\r\n", LED_NAME);

	return 0;	
}

static void __exit exit_led(void)
{
	iounmap(MAP_SW_MUX_GPIO1_IO03);
	iounmap(MAP_SW_PAD_GPIO1_IO03);
	iounmap(MAP_CCM_CCGR1);
	iounmap(MAP_GPIO1_GDIR);
	iounmap(MAP_GPIO1_DR);

	cdev_del(&dtled.cdev);
	unregister_chrdev_region(dtled.devid, 1);

	device_destroy(dtled.pclass, dtled.devid);
	class_destroy(dtled.pclass);

	printk(KERN_INFO "led module has been removed!\r\n");
}

module_init(init_led);
module_exit(exit_led);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("wang");

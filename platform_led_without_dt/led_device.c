#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/delay.h>
#include <linux/ide.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/platform_device.h>


#define REGISTER_LENTH 4

enum led_state_t{
	LED_OFF,
	LED_ON,
};

#define SW_MUX_GPIO1_IO03	(0x020e0068)
#define SW_PAD_GPIO1_IO03 	(0x020e02f4)
#define CCM_CCGR1		(0x020c406c)
#define GPIO1_GDIR		(0x0209c004)
#define GPIO1_DR		(0x0209c000)

static struct resource led_resources[] = {
    [0] = {
        .start = SW_MUX_GPIO1_IO03,
        .end = SW_MUX_GPIO1_IO03 + REGISTER_LENTH - 1,
        .flags = IORESOURCE_MEM,
    },

    [1] = {
        .start = SW_PAD_GPIO1_IO03,
        .end = SW_PAD_GPIO1_IO03 + REGISTER_LENTH - 1,
        .flags = IORESOURCE_MEM,
    },

    [2] = {
        .start = CCM_CCGR1,
        .end = CCM_CCGR1 + REGISTER_LENTH - 1,
        .flags = IORESOURCE_MEM,
    },

    [3] = {
        .start = GPIO1_GDIR,
        .end = GPIO1_GDIR + REGISTER_LENTH - 1,
        .flags = IORESOURCE_MEM,
    },
    
    [4] = {
        .start = GPIO1_DR,
        .end = GPIO1_DR + REGISTER_LENTH - 1,
        .flags = IORESOURCE_MEM,
    }
};

static void led_release(struct device *dev)
{
   printk("led device released!\r\n");
}

static struct platform_device led_device = {
    .name = "platform_led",
    .id = -1,
    .dev = {
        .release = &led_release,
    },
    .num_resources = ARRAY_SIZE(led_resources),
    .resource = led_resources,
};

static int __init init_led(void)
{

	printk(KERN_INFO "load led device success!\r\n");

	return platform_device_register(&led_device);
}

static void __exit exit_led(void)
{
    platform_device_unregister(&led_device);
	printk(KERN_INFO "led device removed!\r\n");
}

module_init(init_led);
module_exit(exit_led);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("wang");

#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/delay.h>
#include <linux/ide.h>
#include <linux/init.h>
#include <linux/module.h>


#define CHDEVBASE_MAJOR 200
#define CHDEVBASE_NAME "chdevbase" 

static char readbuf[100];
static char writebuf[100];
static char kernel_data[] = {"kernel data!"};

static int open_chdevbase(struct inode *inode, struct file *filp)
{
	return 0;
}

static size_t read_chdevbase(struct file *filp, char __user *buf,
		size_t cnt, loff_t *offt)
{
	memcpy(readbuf, kernel_data, sizeof(kernel_data));
	if(copy_to_user(buf, kernel_data, cnt)) {
		printk(KERN_ERR "chdevbase read faild!\r\n");
		return -EFAULT;
	}
	else
		printk(KERN_INFO "chdevbase read %u byte(s) : %s\r\n", sizeof(kernel_data), readbuf);

	return sizeof(kernel_data));
}

static size_t write_chdevbase(struct file *filp, const char __user *buf,
		size_t cnt, loff_t *offt)
{
	if(copy_from_user(writebuf, buf, cnt)) {
		
		printk(KERN_ERR "chdevbase write faild!\r\n");
		return -EFAULT;
	}
	else
		printk(KERN_INFO "chdevbase kernel rcv %u byte(s) : %s\r\n", cnt, writebuf);

	return cnt
}

static int release_chdevbase(struct inode *inode, struct file *filp)
{
	return 0;
}

static struct file_operation chdevbase_fops = {
	.owner = THIS_MODULE,
	.open = open_chdevbase,
	.release = release_chdevbase,
	.read = read_chdevbase,
	.write = write_chdevbase,
};

static int __init init_chdevbase(void)
{
	if(register_chrdev(CHDEVBASE_MAJOR, CHDEVBASE_NAME, &chdevbase_fops) < 0) {
		printk(KERN_ERR "chdevbase register chdevbase driver faild!\r\n");
		return -EFAULT;
	}

	printk(KERN_INFO "chdevbase load chdevbase driver success!\r\n");

	return 0;	
}

static void __exit exit_chdevbase(void)
{
	unregister_chrdev(CHDEVBASE_MAJOR, CHDEVBASE_NAME);
	printk(KERN_INFO "chdevbase removed chdevbase!\r\n");
}

module_init(init_chdevbase);
module_exit(exit_chdevbase);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("wang");

#include <linux/module.h>
#include <linux/genhd.h>
#include <linux/blkdev.h>
#include <linux/spinlock.h>
#include <linux/hdreg.h>

#define RAMDISK_SIZE (2*1024*1024)
#define RAMDISK_NAME "ramdisk"
#define RAMDISK_MINOR 3

struct ramdisk_dev {
	struct gendisk *ramdisk;
	spinlock_t lock;
	int major;
	struct request_queue *queue;
	void *ramdisk_buf;
};

struct ramdisk_dev ramdisk;

static int ramdisk_open(struct block_device *dev, fmode_t mode)
{
	printk("open ramdisk\r\n");	
	return 0;
}

static void ramdisk_release(struct gendisk *dev, fmode_t mode)
{
	printk("release ramdisk\r\n");	
}

static int ramdisk_getgeo(struct block_device *dev, struct hd_geometry *geo)
{
	geo->heads = 2;
	geo->cylinders = 32;
	geo->sectors = RAMDISK_SIZE / (2*32*512);
	return 0;
}

static void ramdisk_transfer(struct request *req)
{
	unsigned long start = blk_rq_pos(req) << 9;
	unsigned long len = blk_rq_cur_bytes(req);
	
	void *buffer = bio_data(req->bio);
	
	if (rq_data_dir(req) == READ)
		memcpy(buffer, ramdisk.ramdisk_buf + start, len);
	else if (rq_data_dir(req) == WRITE)
		memcpy(ramdisk.ramdisk_buf + start, buffer, len);
}

void ramdisk_rq_proc(struct request_queue *q)
{
	int err = 0;
	struct request *req;
	
        req = blk_fetch_request(q);	

	while (req != NULL) {
		ramdisk_transfer(req);
	
		if (!__blk_end_request_cur(req, err))		
			req = blk_fetch_request(q);	
	}
}

static const struct block_device_operations ramdisk_fops = {
	.owner = THIS_MODULE,
	.open = ramdisk_open,
	.release = ramdisk_release,
	.getgeo = ramdisk_getgeo,
};

int __init ramdisk_init(void)
{
	spin_lock_init(&ramdisk.lock);

	ramdisk.ramdisk_buf = vmalloc(RAMDISK_SIZE);
	if (!ramdisk.ramdisk_buf) {
		printk("%s:alloc space for disk failed\r\n",__func__);
	}
	printk("%s:alloc space for disk, start addr:%p\r\n",__func__, ramdisk.ramdisk_buf);

	ramdisk.major = register_blkdev(0, RAMDISK_NAME);
	if (ramdisk.major < 0) {
		printk("%s:register block device failed\r\n",__func__);
		goto register_blkdev_failed;	
	}
	printk("%s:register block device, major:%d\r\n",__func__, ramdisk.major);

	ramdisk.ramdisk = alloc_disk(RAMDISK_MINOR);
	if(!ramdisk.ramdisk) {
		printk("%s:allocated disk failed\r\n",__func__);
		goto alloc_disk_failed;
	}

	ramdisk.queue = blk_init_queue(ramdisk_rq_proc, &ramdisk.lock);
	if (!ramdisk.queue) {
		printk("%s:init request queue failed\r\n",__func__);
		goto blk_init_queue_failed;
	}

	ramdisk.ramdisk->first_minor = 0;
	ramdisk.ramdisk->fops = &ramdisk_fops;
	ramdisk.ramdisk->queue = ramdisk.queue;
	ramdisk.ramdisk->major = ramdisk.major;
	ramdisk.ramdisk->private_data = &ramdisk;
	set_capacity(ramdisk.ramdisk, RAMDISK_SIZE/512);
	sprintf(ramdisk.ramdisk->disk_name, RAMDISK_NAME);

	add_disk(ramdisk.ramdisk);
	return 0;

blk_init_queue_failed:
	del_gendisk(ramdisk.ramdisk);

alloc_disk_failed:
	unregister_blkdev(ramdisk.major, RAMDISK_NAME);

register_blkdev_failed:
	vfree(ramdisk.ramdisk_buf);

	return -EINVAL;
}


void __exit ramdisk_exit(void)
{
	del_gendisk(ramdisk.ramdisk);
	put_disk(ramdisk.ramdisk);
	blk_cleanup_queue(ramdisk.queue);
	unregister_blkdev(ramdisk.major, RAMDISK_NAME);
	vfree(ramdisk.ramdisk_buf);
	printk("remove ramdisk\r\n");
}


module_init(ramdisk_init);
module_exit(ramdisk_exit);
MODULE_AUTHOR("Wenjie Wang <ww107587@gmail.com>");
MODULE_DESCRIPTION("AP3216 Driver");
MODULE_LICENSE("GPL");

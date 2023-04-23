#include <linux/module.h>
#include <linux/genhd.h>
#include <linux/blkdev.h>
#include <linux/spinlock.h>
#include <linux/hdreg.h>

#define RAMDISK_SIZE (2*1024*1024)
#define RAMDISK_NAME "ramdisk_mk_request"
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

static void cb_mk_req_fn(struct request_queue *q, struct bio *bio)
{
	int offset;
	struct bio_vec bvec;
	struct bvec_iter iter;
	unsigned long len = 0;
	
	offset = (bio->bi_iter.bi_sector) << 9;

	bio_for_each_segment(bvec, bio, iter) {
		char *ptr = page_address(bvec.bv_page) + bvec.bv_offset;
		len = bvec.bv_len;

		if(bio_data_dir(bio) == READ)
			memcpy(ptr, ramdisk.ramdisk_buf + offset, len);
		else if(bio_data_dir(bio) == WRITE)
			memcpy(ramdisk.ramdisk_buf + offset, ptr, len);
		offset += len;
	}

	set_bit(BIO_UPTODATE, &bio->bi_flags);
	bio_endio(bio, 0);
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

	ramdisk.queue = blk_alloc_queue(GFP_KERNEL);
	if (!ramdisk.queue) {
		printk("%s:alloc request queue failed\r\n",__func__);
		goto blk_alloc_queue;
	}

	blk_queue_make_request(ramdisk.queue, cb_mk_req_fn);

	ramdisk.ramdisk->first_minor = 0;
	ramdisk.ramdisk->fops = &ramdisk_fops;
	ramdisk.ramdisk->queue = ramdisk.queue;
	ramdisk.ramdisk->major = ramdisk.major;
	ramdisk.ramdisk->private_data = &ramdisk;
	set_capacity(ramdisk.ramdisk, RAMDISK_SIZE/512);
	sprintf(ramdisk.ramdisk->disk_name, RAMDISK_NAME);

	add_disk(ramdisk.ramdisk);
	return 0;

blk_alloc_queue:
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

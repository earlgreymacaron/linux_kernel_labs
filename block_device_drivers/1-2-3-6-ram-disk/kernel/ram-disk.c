/*
 * SO2 - Block device drivers lab (#7)
 * Linux - Exercise #1, #2, #3, #6 (RAM Disk)
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>

#include <linux/genhd.h>
#include <linux/fs.h>
#include <linux/blkdev.h>
#include <linux/bio.h>
#include <linux/vmalloc.h>

MODULE_DESCRIPTION("Simple RAM Disk");
MODULE_AUTHOR("SO2");
MODULE_LICENSE("GPL");


#define KERN_LOG_LEVEL		KERN_ALERT

#define MY_BLOCK_MAJOR		240
#define MY_BLKDEV_NAME		"mybdev"
#define MY_BLOCK_MINORS		1
#define NR_SECTORS		128

#define KERNEL_SECTOR_SIZE	512

/* Use bios for read/write requests */
#define USE_BIO_TRANSFER	1


static struct my_block_dev {
	spinlock_t lock;
	struct request_queue *queue;
	struct gendisk *gd;
	u8 *data;
	size_t size;
} g_dev;

static int my_block_open(struct block_device *bdev, fmode_t mode)
{
	return 0;
}

static void my_block_release(struct gendisk *gd, fmode_t mode)
{
}

static const struct block_device_operations my_block_ops = {
	.owner = THIS_MODULE,
	.open = my_block_open,
	.release = my_block_release
};

static void my_block_transfer(struct my_block_dev *dev, sector_t sector,
		unsigned long len, char *buffer, int dir)
{
	unsigned long offset = sector * KERNEL_SECTOR_SIZE;

	/* check for read/write beyond end of block device */
	if ((offset + len) > dev->size)
		return;

	/* Read/write to dev buffer depending on dir */
	if (dir == 0) { //read 
		memcpy(buffer, dev->data + offset, len);
	} else if (dir == 1) { // write
		memcpy(dev->data + offset, buffer, len);	
	} else {
		printk(KERN_ERR "invalid transfer direction %d\n", dir);
		return;
	}

}

/* to transfer data using bio structures enable USE_BIO_TRANFER */
#if USE_BIO_TRANSFER == 1
static void my_xfer_request(struct my_block_dev *dev, struct request *req)
{
	struct req_iterator iter; 
	struct bio_vec bvec; 
	sector_t sector;
	char *buf;
	unsigned long offset; 
	size_t len; 

	/* Iterate segments */
	rq_for_each_segment(bvec, req, iter){
		/* Copy bio data to device buffer */
		sector = iter.iter.bi_sector; 
		offset = bvec.bv_offset;
		len = bvec.bv_len;

		buf = kmap_atomic(bvec.bv_page);
		my_block_transfer(dev, sector, len, buf + offset, bio_data_dir(iter.bio));
		kunmap_atomic(buf);
	}
}
#endif

static void my_block_request(struct request_queue *q)
{
	struct request *rq;
	struct my_block_dev *dev = q->queuedata;

	while (1) {

		/* Fetch request */
		rq = blk_fetch_request(q);
		if (!rq) break; 

		/* Check fs request */
		if (blk_rq_is_passthrough(rq)) {
			printk (KERN_NOTICE "Skip non-fs request\n");
			__blk_end_request_all(rq, -EIO);
			continue;
		}

		/* Print request information */
		pr_info("request received\n");
		pr_info(" -> start_sector=%d\n", (int) blk_rq_pos(rq));
		pr_info(" -> total_size=%d\n", blk_rq_bytes(rq)); 
		pr_info(" -> data_size=%d\n", blk_rq_cur_bytes(rq));
		pr_info(" -> direction=%d (0 for read, 1 for write)\n", rq_data_dir(rq));

#if USE_BIO_TRANSFER == 1
		/* Process the request by calling my_xfer_request */
		my_xfer_request(dev, rq);
#else
		/* Process the request by calling my_block_transfer */
		my_block_transfer(dev, blk_rq_pos(rq), blk_rq_cur_bytes(rq), bio_data(rq->bio), rq_data_dir(rq));
#endif

		/* End request successfully */
		__blk_end_request_all(rq, 0);
	}
}

static int create_block_device(struct my_block_dev *dev)
{
	int err;

	dev->size = NR_SECTORS * KERNEL_SECTOR_SIZE;
	dev->data = vmalloc(dev->size);
	if (dev->data == NULL) {
		printk(KERN_ERR "vmalloc: out of memory\n");
		err = -ENOMEM;
		goto out_vmalloc;
	}

	/* initialize the I/O queue */
	spin_lock_init(&dev->lock);
	dev->queue = blk_init_queue(my_block_request, &dev->lock);
	if (dev->queue == NULL) {
		printk(KERN_ERR "blk_init_queue: out of memory\n");
		err = -ENOMEM;
		goto out_blk_init;
	}
	blk_queue_logical_block_size(dev->queue, KERNEL_SECTOR_SIZE);
	dev->queue->queuedata = dev;

	/* initialize the gendisk structure */
	dev->gd = alloc_disk(MY_BLOCK_MINORS);
	if (!dev->gd) {
		printk(KERN_ERR "alloc_disk: failure\n");
		err = -ENOMEM;
		goto out_alloc_disk;
	}

	dev->gd->major = MY_BLOCK_MAJOR;
	dev->gd->first_minor = 0;
	dev->gd->fops = &my_block_ops;
	dev->gd->queue = dev->queue;
	dev->gd->private_data = dev;
	snprintf(dev->gd->disk_name, DISK_NAME_LEN, "myblock");
	set_capacity(dev->gd, NR_SECTORS);

	add_disk(dev->gd);

	return 0;

out_alloc_disk:
	blk_cleanup_queue(dev->queue);
out_blk_init:
	vfree(dev->data);
out_vmalloc:
	return err;
}

static int __init my_block_init(void)
{
	int err = 0;

	/* Register block device */
	err = register_blkdev(MY_BLOCK_MAJOR, MY_BLKDEV_NAME);
	if (err < 0) {
		printk(KERN_ERR "unable to register mybdev block device\n");
		return err;
	}

	/* Create block device using create_block_device */
	create_block_device(&g_dev);

	return 0;

out:
	/* Unregister block device in case of an error */
	unregister_blkdev(MY_BLOCK_MAJOR, MY_BLKDEV_NAME);
	return err;
}

static void delete_block_device(struct my_block_dev *dev)
{
	if (dev->gd) {
		del_gendisk(dev->gd);
		put_disk(dev->gd);
	}
	if (dev->queue)
		blk_cleanup_queue(dev->queue);
	if (dev->data)
		vfree(dev->data);
}

static void __exit my_block_exit(void)
{
	/* Cleanup block device using delete_block_device */
	delete_block_device(&g_dev);

	/* Unregister block device */
	unregister_blkdev(MY_BLOCK_MAJOR, MY_BLKDEV_NAME);
}

module_init(my_block_init);
module_exit(my_block_exit);

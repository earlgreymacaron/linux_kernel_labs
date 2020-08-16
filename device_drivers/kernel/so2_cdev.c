/*
 * Character device drivers lab
 *
 * All tasks
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/uaccess.h>
#include <linux/sched.h>
#include <linux/wait.h>

#include "../include/so2_cdev.h"

MODULE_DESCRIPTION("SO2 character device");
MODULE_AUTHOR("SO2");
MODULE_LICENSE("GPL");

#define LOG_LEVEL	KERN_INFO

#define MY_MAJOR		42
#define MY_MINOR		0
#define NUM_MINORS		1
#define MODULE_NAME		"so2_cdev"
#define MESSAGE			"hello\n"
#define IOCTL_MESSAGE		"Hello ioctl"

#ifndef BUFSIZ
#define BUFSIZ		4096
#endif


struct so2_device_data {
	struct cdev cdev;
	atomic_t accessed;
	char buffer[BUFSIZ];
	size_t size;
};

struct so2_device_data devs[NUM_MINORS];

static int so2_cdev_open(struct inode *inode, struct file *file)
{
	struct so2_device_data *data;

	pr_info("%s opened successfully\n", MODULE_NAME);
	
	data = container_of(inode->i_cdev, struct so2_device_data, cdev);

	file->private_data = data;

	if (atomic_cmpxchg(&data->accessed,0,1) != 0) 
		return -EBUSY;

	//set_current_state(TASK_INTERRUPTIBLE);
	//schedule_timeout(10);

	return 0;
}

static int
so2_cdev_release(struct inode *inode, struct file *file)
{
	/* print message when the device file is closed. */
	pr_info("%s released successfully\n", MODULE_NAME);

#ifndef EXTRA
	struct so2_device_data *data =
		(struct so2_device_data *) file->private_data;

	atomic_set(&data->accessed,0);
#endif
	return 0;
}

static ssize_t
so2_cdev_read(struct file *file, char __user *user_buffer, size_t size, loff_t *offset)
{
	struct so2_device_data *data = (struct so2_device_data *) file->private_data;
	
	size_t to_read;

	if (data->size - *offset < size) 
		to_read = data->size - *offset;
	else 
		to_read = size;

	if (copy_to_user(user_buffer, data->buffer + *offset, to_read) < 0)
		return -EINVAL;

	*offset += to_read;

	return to_read;
}

static ssize_t
so2_cdev_write(struct file *file, const char __user *user_buffer, size_t size, loff_t *offset)
{
	struct so2_device_data *data = (struct so2_device_data *) file->private_data;

	size_t to_write; 

	if(*offset + size < BUFSIZ)
		to_write = size;
	else
		to_write = BUFSIZ - *offset;
	
	if (copy_from_user(data->buffer + *offset, user_buffer, to_write) < 0)
		return -EINVAL;

	data->size += to_write;
	*offset += to_write;

	return to_write;
}

static long
so2_cdev_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	struct so2_device_data *data =
		(struct so2_device_data *) file->private_data;
	int ret = 0;
	int remains;

	switch (cmd) {
		case MY_IOCTL_PRINT: 
			pr_info("%s\n", IOCTL_MESSAGE);
		default:
			ret = -EINVAL;
	}

	return ret;
}

static const struct file_operations so2_fops = {
	.owner = THIS_MODULE,
	.open = so2_cdev_open,
	.release = so2_cdev_release,
	.read = so2_cdev_read,
	.write = so2_cdev_write,
	.unlocked_ioctl = so2_cdev_ioctl,
};

static int so2_cdev_init(void)
{
	int err;
	int i;

	/* register char device region for MY_MAJOR and NUM_MINORS starting at MY_MINOR */
	err = register_chrdev_region(MKDEV(MY_MAJOR, MY_MINOR), NUM_MINORS, MODULE_NAME);
	if (err != 0){
		pr_info("error code: %d\n",err);    
		return err;
	}
	pr_info("%s successfully registered\n", MODULE_NAME);

	for (i = 0; i < NUM_MINORS; i++) {
		memset(&devs[i].buffer, 0, BUFSIZ);
		memcpy(&devs[i].buffer, MESSAGE, sizeof(MESSAGE));
		devs[i].size = sizeof(MESSAGE);
		atomic_set(&devs[i].accessed, 0);
		cdev_init(&devs[i].cdev, &so2_fops);
		cdev_add(&devs[i].cdev, MKDEV(MY_MAJOR, i), 1);
	}

	return 0;
}

static void so2_cdev_exit(void)
{
	int i;

	for (i = 0; i < NUM_MINORS; i++) {
		/* delete cdev from kernel core */
		cdev_del(&devs[i].cdev);
	}

	/* unregister char device region, for MY_MAJOR and NUM_MINORS starting at MY_MINOR */
	unregister_chrdev_region(MKDEV(MY_MAJOR, MY_MINOR), NUM_MINORS);
	pr_info("%s successfully unregistered\n", MODULE_NAME);
}

module_init(so2_cdev_init);
module_exit(so2_cdev_exit);

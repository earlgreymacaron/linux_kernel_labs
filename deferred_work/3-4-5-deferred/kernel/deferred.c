/*
 * SO2 - Lab 6 - Deferred Work
 *
 * Exercises #3, #4, #5: deferred work
 *
 * Code skeleton.
 */

#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/sched.h>
#include <linux/slab.h>
#include <linux/uaccess.h>
#include <linux/sched/task.h>
#include "../include/deferred.h"

#define MY_MAJOR		42
#define MY_MINOR		0
#define MODULE_NAME		"deferred"

#define TIMER_TYPE_NONE		-1
#define TIMER_TYPE_SET		0
#define TIMER_TYPE_ALLOC	1
#define TIMER_TYPE_MON		2
#define TIMER_TYPE_ACCT		3

MODULE_DESCRIPTION("Deferred work character device");
MODULE_AUTHOR("SO2");
MODULE_LICENSE("GPL");

struct mon_proc {
	struct task_struct *task;
	struct list_head list;
};

static struct my_device_data {
	struct cdev cdev;
	struct timer_list timer;
	unsigned int flag;
	struct work_struct work;
	struct list_head plist;
	spinlock_t plist_lock;
} dev;

static void alloc_io(void)
{
	set_current_state(TASK_INTERRUPTIBLE);
	schedule_timeout(5 * HZ);
	pr_info("Yawn! I've been sleeping for 5 seconds.\n");
}

static struct mon_proc *get_proc(pid_t pid)
{
	struct task_struct *task;
	struct mon_proc *p;

	rcu_read_lock();
	task = pid_task(find_vpid(pid), PIDTYPE_PID);
	rcu_read_unlock();
	if (!task)
		return ERR_PTR(-ESRCH);

	p = kmalloc(sizeof(p), GFP_ATOMIC);
	if (!p)
		return ERR_PTR(-ENOMEM);

	get_task_struct(task);
	p->task = task;

	return p;
}


void work_handler(struct work_struct *work);

/* Define work handler */
void work_handler(struct work_struct *work)
{
	alloc_io();
}

//#define ALLOC_IO_DIRECT
/* TODO 3: undef ALLOC_IO_DIRECT*/

static void timer_handler(struct timer_list *tl)
{
	/* Implement timer handler */
	struct list_head *i, *n;
	struct mon_proc *mproc;
	struct task_struct *t = current;
	struct my_device_data *d = container_of(tl, struct my_device_data, timer);

	if (d->flag == TIMER_TYPE_SET) {
		pr_info("[timer_handler] ]timer handler beeps (pid=%d,comm=%s)\n", t->pid, t->comm);
	} else if (d->flag == TIMER_TYPE_ALLOC) {
		//alloc_io();
		schedule_work(&d->work);
	} else if (d->flag == TIMER_TYPE_MON) {
		/* Iterate the list and check the proccess state */
		spin_lock_bh(&d->plist_lock);
		list_for_each_safe(i, n, &d->plist) {
			/* If task is dead print info ... */
			mproc = list_entry(i, struct mon_proc, list);
			if (mproc->task->state == TASK_DEAD) {
				pr_info("[timer_handler] (pid=%d,comm=%s) is dead\n", mproc->task->pid, mproc->task->comm);
				/* Decrement task usage counter ... */
				put_task_struct(mproc->task);
				/* Remove it from the list ... */
				list_del(i);
				/* Free the struct mon_proc */
				kfree(mproc);
			}
		}
		spin_unlock_bh(&d->plist_lock);
		
		mod_timer(&d->timer, jiffies * HZ);

	} else {
		pr_info("Invalid flag\n");
	}
}

static int deferred_open(struct inode *inode, struct file *file)
{
	struct my_device_data *my_data =
		container_of(inode->i_cdev, struct my_device_data, cdev);
	file->private_data = my_data;
	pr_info("[deferred_open] Device opened\n");
	return 0;
}

static int deferred_release(struct inode *inode, struct file *file)
{
	pr_info("[deferred_release] Device released\n");
	return 0;
}

static long deferred_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	struct my_device_data *my_data = (struct my_device_data*) file->private_data;
	struct mon_proc *mproc;

	pr_info("[deferred_ioctl] Command: %s\n", ioctl_command_to_string(cmd));

	switch (cmd) {
		case MY_IOCTL_TIMER_SET:
			/* Set flag */
			my_data->flag = TIMER_TYPE_SET;
			/* Schedule timer */
			mod_timer(&my_data->timer, jiffies + arg * HZ);
			break;
		case MY_IOCTL_TIMER_CANCEL:
			/* Cancel timer */
			del_timer_sync(&my_data->timer);
			break;
		case MY_IOCTL_TIMER_ALLOC:
			/* Set flag and schedule timer */
			my_data->flag = TIMER_TYPE_ALLOC;
			mod_timer(&my_data->timer, jiffies + arg * HZ);
			break;
		case MY_IOCTL_TIMER_MON:
		{
			/* Use get_proc() and add task to list */
			mproc = get_proc(arg);	
			if (IS_ERR_OR_NULL(mproc)) { 
				//return PTR_ERR(mproc);
				pr_info("[deferred_ioctl] invalid pid %d\n", arg);
				return -EFAULT;
			}
			spin_lock_bh(&my_data->plist_lock);
			list_add(&mproc->list, &my_data->plist);
			spin_unlock_bh(&my_data->plist_lock);
			
			/* Set flag and schedule timer */
			my_data->flag = TIMER_TYPE_MON;
			mod_timer(&my_data->timer, jiffies * HZ);
			
			break;
		}
		default:
			return -ENOTTY;
	}
	return 0;
}

struct file_operations my_fops = {
	.owner = THIS_MODULE,
	.open = deferred_open,
	.release = deferred_release,
	.unlocked_ioctl = deferred_ioctl,
};

static int deferred_init(void)
{
	int err;

	pr_info("[deferred_init] Init module\n");
	err = register_chrdev_region(MKDEV(MY_MAJOR, MY_MINOR), 1, MODULE_NAME);
	if (err) {
		pr_info("[deffered_init] register_chrdev_region: %d\n", err);
		return err;
	}

	/* Initialize flag. */
	dev.flag = TIMER_TYPE_NONE;

	/* Initialize work. */
	INIT_WORK(&dev.work, work_handler);

	/* Initialize lock and list. */
	INIT_LIST_HEAD(&dev.plist);
	spin_lock_init(&dev.plist_lock);

	cdev_init(&dev.cdev, &my_fops);
	cdev_add(&dev.cdev, MKDEV(MY_MAJOR, MY_MINOR), 1);

	/* Initialize timer. */
	timer_setup(&dev.timer, timer_handler, 0);

	return 0;
}

static void deferred_exit(void)
{
	struct mon_proc *p, *mproc;
	struct list_head *i, *n;

	pr_info("[deferred_exit] Exit module\n" );

	cdev_del(&dev.cdev);
	unregister_chrdev_region(MKDEV(MY_MAJOR, MY_MINOR), 1);

	/* Cleanup: make sure the timer is not running after exiting. */
	del_timer_sync(&dev.timer);
	
	/* Cleanup: make sure the work handler is not scheduled. */
	cancel_work_sync(&dev.work);

	/* Cleanup the monitered process list */
	spin_lock_bh(&dev.plist_lock);
	list_for_each_safe(i, n, &dev.plist) {
		mproc = list_entry(i, struct mon_proc, list);
		/* Decrement task usage counter ... */
		put_task_struct(mproc->task);
		/* Remove it from the list ... */
		list_del(i);
		/* Free the struct mon_proc */
		kfree(mproc);
	}
	spin_unlock_bh(&dev.plist_lock);
}

module_init(deferred_init);
module_exit(deferred_exit);

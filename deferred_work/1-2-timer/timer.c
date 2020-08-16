/*
 * Deferred Work
 *
 * Exercise #1, #2: simple timer
 */

#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/sched.h>

MODULE_DESCRIPTION("Simple kernel timer");
MODULE_AUTHOR("SO2");
MODULE_LICENSE("GPL");

#define TIMER_TIMEOUT	1

static struct timer_list timer;

int sec = 0;

static void timer_handler(struct timer_list *tl)
{
	/* Print a message */
	pr_info("Timer Handler invoked after %d seconds!!\n", ++sec);
	mod_timer(&timer, jiffies + TIMER_TIMEOUT*HZ);
}

static int __init timer_init(void)
{
	pr_info("[timer_init] Init module\n");

	/* Initialize timer */
	timer_setup(&timer, timer_handler, 0);
	
	/* Schedule timer for the first time */
	mod_timer(&timer, jiffies + TIMER_TIMEOUT*HZ);

	return 0;
}

static void __exit timer_exit(void)
{
	pr_info("[timer_exit] Exit module\n");

	/* Cleanup; make sure the timer is not running after we exit */
	del_timer_sync(&timer);
}

module_init(timer_init);
module_exit(timer_exit);

#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/sched.h>
#include <linux/sched/signal.h>

MODULE_DESCRIPTION("List current processes");
MODULE_AUTHOR("Kernel Hacker");
MODULE_LICENSE("GPL");

static int my_proc_init(void)
{
	struct task_struct *p;
	p = current;
	pr_info("process : %s (pid=%d)\n", p->comm, p->pid);
			
	for_each_process(p)
		pr_info("%s (pid=%d)\n", p->comm, p->pid);

	return 0;
}

static void my_proc_exit(void)
{
	struct task_struct *p;

	p = current;
	pr_info("process : %s (pid=%d)\n", p->comm, p->pid);
}

module_init(my_proc_init);
module_exit(my_proc_exit);

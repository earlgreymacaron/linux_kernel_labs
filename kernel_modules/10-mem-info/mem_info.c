#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/sched.h>
#include <linux/sched/signal.h>
#include <linux/mm_types.h>

MODULE_DESCRIPTION("Inspect memory of current processes");
MODULE_AUTHOR("YSP");
MODULE_LICENSE("GPL");

static int my_proc_init(void)
{
	struct task_struct *p;
	struct vm_area_struct *vm_area;
	p = current;

	pr_info("module installed\n");
	pr_info("process : %s (pid=%d)\n", p->comm, p->pid);


	vm_area = p->mm->mmap;
	
	pr_info("Memory Area:\n");
	while(vm_area) {
		pr_info("\t0x%lx - 0x%lx\n", vm_area->vm_start, vm_area->vm_end);
		vm_area = vm_area->vm_next;
	}

	vm_area = p->active_mm->mmap;
	
	pr_info("Active Area:\n");
	while(vm_area) {
		pr_info("\t0x%lx - 0x%lx\n", vm_area->vm_start, vm_area->vm_end);
		vm_area = vm_area->vm_next;
	}

	return 0;
}

static void my_proc_exit(void)
{
	struct task_struct *p;

	p = current;
	pr_info("process : %s (pid=%d)\n", p->comm, p->pid);
	pr_info("module removed\n");

}

module_init(my_proc_init);
module_exit(my_proc_exit);

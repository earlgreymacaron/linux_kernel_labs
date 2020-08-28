/*
 * PSO - Memory Mapping Lab(#11)
 *
 * Exercise #2: memory mapping using vmalloc'd kernel areas
 */

#include <linux/version.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/slab.h>
#include <linux/vmalloc.h>
#include <linux/sched.h>
#include <linux/sched/mm.h>
#include <linux/mm.h>
#include <asm/io.h>
#include <linux/uaccess.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>

#include "../test/mmap-test.h"


MODULE_DESCRIPTION("simple mmap driver");
MODULE_AUTHOR("PSO");
MODULE_LICENSE("Dual BSD/GPL");

#define MY_MAJOR	42

/* how many pages do we actually vmalloc */
#define NPAGES		16

/* character device basic structure */
static struct cdev mmap_cdev;

/* pointer to the vmalloc'd area, rounded up to a page boundary */
static char *vmalloc_area;

static int my_open(struct inode *inode, struct file *filp)
{
	return 0;
}

static int my_release(struct inode *inode, struct file *filp)
{
	return 0;
}

static ssize_t my_read(struct file *file, char __user *user_buffer,
		size_t size, loff_t *offset)
{
	/* TODO 2: check size doesn't exceed our mapped area size */

	/* TODO 2: copy from mapped area to user buffer */

	return size;
}

static ssize_t my_write(struct file *file, const char __user *user_buffer,
		size_t size, loff_t *offset)
{
	/* TODO 2: check size doesn't exceed our mapped area size */

	/* TODO 2: copy from user buffer to mapped area */

	return size;
}

static int my_mmap(struct file *filp, struct vm_area_struct *vma)
{
	int ret, i;
	long length = vma->vm_end - vma->vm_start;
	unsigned long start = vma->vm_start;
	unsigned long vmalloc_area_ptr = (unsigned long) vmalloc_area;
	unsigned long pfn;
	unsigned long vaddr = start;

	if (length > NPAGES * PAGE_SIZE)
		return -EIO;

	/* Map pages individually */
	while(vmalloc_area_ptr < (unsigned long) vmalloc_area + length) {
		pfn = vmalloc_to_pfn((void *) vmalloc_area_ptr);
		ret = remap_pfn_range(vma, vaddr, pfn, PAGE_SIZE, vma->vm_page_prot);
		if (ret < 0) {
			pr_err("could not map the address area\n");
			return -EIO;
		}
		vmalloc_area_ptr += PAGE_SIZE;
		vaddr += PAGE_SIZE;
	}

	return 0;
}

static const struct file_operations mmap_fops = {
	.owner = THIS_MODULE,
	.open = my_open,
	.release = my_release,
	.mmap = my_mmap,
	.read = my_read,
	.write = my_write
};

static int my_seq_show(struct seq_file *seq, void *v)
{
	struct mm_struct *mm;
	struct vm_area_struct *vma_iterator;
	unsigned long total = 0;

	/* TODO 3: Get current process' mm_struct */

	/* TODO 3: Iterate through all memory mappings and print ranges */
	/* TODO 3: Release mm_struct */
	mmput(mm);

	/* TODO 3: write the total count to file  */
	return 0;
}

static int my_seq_open(struct inode *inode, struct file *file)
{
	return single_open(file, my_seq_show, NULL);
}

static const struct file_operations my_proc_file_ops = {
	.owner   = THIS_MODULE,
	.open    = my_seq_open,
	.read    = seq_read,
	.llseek  = seq_lseek,
	.release = single_release,
};

static int __init my_init(void)
{
	int ret = 0;
	int i;
	/* TODO 3: create a new entry in procfs */

	ret = register_chrdev_region(MKDEV(MY_MAJOR, 0), 1, "mymap");
	if (ret < 0) {
		pr_err("could not register region\n");
		goto out_no_chrdev;
	}

	/* Allocate NPAGES using vmalloc */
	vmalloc_area = vmalloc(NPAGES * PAGE_SIZE);
	if(!vmalloc_area) {
		pr_err("could not allocate memory\n");
		ret = -ENOMEM;
		goto out_unreg;
	}

	/* Mark pages as reserved */
	pr_info("vmalloc_area=0x%x\n", vmalloc_area);

	/* Mark pages as reserved */
	for(i = 0; i < NPAGES * PAGE_SIZE; i += PAGE_SIZE) {
		unsigned long ptr = ((unsigned long) vmalloc_area) + i;
		SetPageReserved(vmalloc_to_page((void *) ptr));
		/* Write data in each page */
		memset((void *)ptr,'\xaa', 1);
		memset((void *)ptr+1,'\xbb', 1);
		memset((void *)ptr+2,'\xcc', 1);
		memset((void *)ptr+3,'\xdd', 1);
	}

	cdev_init(&mmap_cdev, &mmap_fops);
	ret = cdev_add(&mmap_cdev, MKDEV(MY_MAJOR, 0), 1);
	if (ret < 0) {
		pr_err("could not add device\n");
		goto out_vfree;
	}

	return 0;

out_vfree:
	vfree(vmalloc_area);
out_unreg:
	unregister_chrdev_region(MKDEV(MY_MAJOR, 0), 1);
out_no_chrdev:
	remove_proc_entry(PROC_ENTRY_NAME, NULL);
out:
	return ret;
}

static void __exit my_exit(void)
{
	int i;

	cdev_del(&mmap_cdev);

	/* Clear reservation on pages and free mem.*/
	for(i = 0; i < NPAGES * PAGE_SIZE; i += PAGE_SIZE) {
		unsigned long ptr = ((unsigned long) vmalloc_area) + i;
		ClearPageReserved(vmalloc_to_page((void *) ptr));
	}
	vfree(vmalloc_area);

	unregister_chrdev_region(MKDEV(MY_MAJOR, 0), 1);
	/* TODO 3: remove proc entry */
}

module_init(my_init);
module_exit(my_exit);

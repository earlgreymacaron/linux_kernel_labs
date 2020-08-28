/*
 * PSO - Memory Mapping Lab(#11)
 *
 * Exercise #1: memory mapping using kmalloc'd kernel areas
 */

#include <linux/version.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/slab.h>
#include <linux/vmalloc.h>
#include <asm/pgtable.h>
#include <linux/sched/mm.h>
#include <linux/sched.h>
#include <asm/io.h>
#include <asm/highmem.h>
#include <linux/rmap.h>
#include <asm/uaccess.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>

#include "../test/mmap-test.h"

MODULE_DESCRIPTION("simple mmap driver");
MODULE_AUTHOR("PSO");
MODULE_LICENSE("Dual BSD/GPL");

#define MY_MAJOR	42
/* how many pages do we actually kmalloc */
#define NPAGES		16

/* character device basic structure */
static struct cdev mmap_cdev;

/* pointer to kmalloc'd area */
static void *kmalloc_ptr;

/* pointer to the kmalloc'd area, rounded up to a page boundary */
static char *kmalloc_area;
unsigned long area_size = NPAGES*PAGE_SIZE;

static int my_open(struct inode *inode, struct file *filp)
{
	return 0;
}

static int my_release(struct inode *inode, struct file *filp)
{
	return 0;
}

static int my_read(struct file *file, char __user *user_buffer,
		size_t size, loff_t *offset)
{
	/* Check size doesn't exceed our mapped area size */
	unsigned long len = (*offset + size < area_size) ? size : area_size - *offset; 

	/* Copy from mapped area to user buffer */
	copy_to_user(user_buffer, (unsigned long) kmalloc_area + *offset, len); 

	return len;
}

static int my_write(struct file *file, const char __user *user_buffer,
		size_t size, loff_t *offset)
{
	/* Check size doesn't exceed our mapped area size */
	unsigned long len = (*offset + size < area_size) ? size : area_size - *offset;

	/* Copy from user buffer to mapped area */
	copy_from_user((unsigned long) kmalloc_area + *offset, user_buffer, len);

	return len;
}

static int my_mmap(struct file *filp, struct vm_area_struct *vma)
{
	int ret;
	long length = vma->vm_end - vma->vm_start;
	unsigned long pfn = virt_to_phys((void *)kmalloc_area) >> PAGE_SHIFT;

	/* Do not map more than we can */
	if (length > NPAGES * PAGE_SIZE)
		return -EIO;

	/* Map the whole physically contiguous area in one piece */
	ret = remap_pfn_range(vma, vma->vm_start, pfn, length, vma->vm_page_prot);
	if (ret < 0) {
		pr_err("could not map the address area\n");
		return -EIO;
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

	/* Get current process' mm_struct */
	mm = get_task_mm(current);
	
	/* Iterate through all memory mappings */
	vma_iterator = mm->mmap;
	while (vma_iterator) {
		total += (vma_iterator->vm_end - vma_iterator->vm_start);
		pr_info("%lx %lx\n", vma_iterator->vm_start, vma_iterator->vm_end);
		vma_iterator = vma_iterator->vm_next;
	}

	/* Release mm_struct */
		mmput(mm);

	/* Write the total count to file  */
	seq_printf(seq, "%d", total);
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
	
	/* Create a new entry in procfs */
	struct proc_dir_entry *proc = proc_create(PROC_ENTRY_NAME, 0, NULL, &my_proc_file_ops);
	if (proc < 0) {
		pr_err("could not create proc entry\n");
		goto out;
	}

	ret = register_chrdev_region(MKDEV(MY_MAJOR, 0), 1, "mymap");
	if (ret < 0) {
		pr_err("could not register region\n");
		goto out_no_chrdev;
	}

	/* Allocate NPAGES+2 pages using kmalloc */
	kmalloc_ptr = kmalloc(PAGE_SIZE * (NPAGES+2), GFP_KERNEL);
	if (!kmalloc_ptr) {
		pr_err("could not allocate memory\n");
		ret = -ENOMEM;
		goto out_unreg;
	}
	
	/* Round kmalloc_ptr to nearest page start address */
	memset(kmalloc_ptr, 0, PAGE_SIZE * (NPAGES+2));
	kmalloc_area = (char *) PAGE_ALIGN((unsigned long) kmalloc_ptr);
	pr_info("kmalloc_ptr=0x%x, kmalloc_area=0x%x\n", kmalloc_ptr, kmalloc_area);

	/* Mark pages as reserved */
	for(i = 0; i < NPAGES * PAGE_SIZE; i += PAGE_SIZE) {
		unsigned long ptr = ((unsigned long) kmalloc_area) + i;
		SetPageReserved(virt_to_page(ptr));
		/* Write data in each page */
		memset((void *)ptr,'\xaa', 1);
		memset((void *)ptr+1,'\xbb', 1);
		memset((void *)ptr+2,'\xcc', 1);
		memset((void *)ptr+3,'\xdd', 1);
	}

	/* Init device. */
	cdev_init(&mmap_cdev, &mmap_fops);
	ret = cdev_add(&mmap_cdev, MKDEV(MY_MAJOR, 0), 1);
	if (ret < 0) {
		pr_err("could not add device\n");
		goto out_kfree;
	}

	return 0;

out_kfree:
	kfree(kmalloc_ptr);
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

	/* Clear reservation on pages and free mem. */
	for(i = 0; i < NPAGES * PAGE_SIZE; i += PAGE_SIZE)
		ClearPageReserved(virt_to_page(((unsigned long)kmalloc_area) + i));
	kfree(kmalloc_ptr);

	unregister_chrdev_region(MKDEV(MY_MAJOR, 0), 1);
	
	/* Remove proc entry */
	remove_proc_entry(PROC_ENTRY_NAME, NULL);
}

module_init(my_init);
module_exit(my_exit);

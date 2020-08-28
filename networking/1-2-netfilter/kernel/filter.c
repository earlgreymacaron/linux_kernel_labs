/*
 * SO2 - Networking Lab (#10)
 *
 * Exercise #1, #2: simple netfilter module
 *
 * Code skeleton.
 */

#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/init.h>
#include <linux/module.h>
#include <asm/uaccess.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <asm/atomic.h>
#include <linux/netfilter.h>
#include <linux/netfilter_ipv4.h>
#include <linux/net.h>
#include <linux/in.h>
#include <linux/skbuff.h>
#include <linux/ip.h>
#include <linux/tcp.h>

#include "filter.h"

MODULE_DESCRIPTION("Simple netfilter module");
MODULE_AUTHOR("SO2");
MODULE_LICENSE("GPL");

#define LOG_LEVEL		KERN_ALERT
#define MY_DEVICE		"filter"

static struct cdev my_cdev;
static atomic_t ioctl_set;
static unsigned int ioctl_set_addr;


/* Test ioctl_set_addr if it has been set.
 */
static int test_daddr(unsigned int dst_addr)
{
	int ret = 0;
	/* Return 1 if address has been set *and* matches dst_addr */
	if (!atomic_read(&ioctl_set))
		ret = 1;
	else {
		if (dst_addr == ioctl_set_addr)
			ret = 1;
	}
	return ret;
}

/* Netfilter hook function */
static unsigned int my_hookfn(void *priv, struct sk_buff *skb, const struct nf_hook_state *state) 
{
	struct tcphdr *tcphdr;
	struct iphdr *iphdr;
	
	iphdr = ip_hdr(skb);
	
	if (iphdr->protocol == IPPROTO_TCP) {
		tcphdr = tcp_hdr(skb);

		/* If it is not the address we are looking for, then pass */
		if (!test_daddr(iphdr->daddr))
			return NF_ACCEPT;
			
		/* Find TCP Initiation Packet */
		if (tcphdr->syn && !(tcphdr->ack)) 
			printk("TCP connection initiated from %pI4:%d\n", &iphdr->saddr, ntohs(tcphdr->source));
	}
	return NF_ACCEPT;
}

static int my_open(struct inode *inode, struct file *file)
{
	return 0;
}

static int my_close(struct inode *inode, struct file *file)
{
	return 0;
}

static long my_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	switch (cmd) {
	case MY_IOCTL_FILTER_ADDRESS:
		copy_from_user((void *) &ioctl_set_addr, (void *) arg, sizeof(unsigned int));
		atomic_set(&ioctl_set, 1);
		break;

	default:
		return -ENOTTY;
	}

	return 0;
}

static const struct file_operations my_fops = {
	.owner = THIS_MODULE,
	.open = my_open,
	.release = my_close,
	.unlocked_ioctl = my_ioctl
};

/* Define netfilter hook operations structure */
static struct nf_hook_ops my_hook = {
	.hook = my_hookfn, 
	.hooknum = NF_INET_LOCAL_OUT,
	.pf = PF_INET,
	.priority = NF_IP_PRI_FIRST, 	
};

int __init my_hook_init(void)
{
	int err;

	/* register filter device */
	err = register_chrdev_region(MKDEV(MY_MAJOR, 0), 1, MY_DEVICE);
	if (err != 0)
		return err;

	atomic_set(&ioctl_set, 0);
	ioctl_set_addr = 0;

	/* init & add device */
	cdev_init(&my_cdev, &my_fops);
	cdev_add(&my_cdev, MKDEV(MY_MAJOR, 0), 1);

	/* Register netfilter hook */
	err = nf_register_net_hook(&init_net, &my_hook);
	if (err)
		goto out;

	return 0;

out:
	/* cleanup */
	cdev_del(&my_cdev);
	unregister_chrdev_region(MKDEV(MY_MAJOR, 0), 1);

	return err;
}

void __exit my_hook_exit(void)
{
	/* Unregister hook */
	nf_unregister_net_hook(&init_net, &my_hook);

	/* cleanup device */
	cdev_del(&my_cdev);
	unregister_chrdev_region(MKDEV(MY_MAJOR, 0), 1);
}

module_init(my_hook_init);
module_exit(my_hook_exit);

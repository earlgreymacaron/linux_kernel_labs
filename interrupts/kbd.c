#include <linux/module.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <asm/io.h>
#include <linux/uaccess.h>
#include <linux/ioport.h>
#include <linux/interrupt.h>
#include <linux/spinlock.h>

MODULE_DESCRIPTION("KBD");
MODULE_AUTHOR("Kernel Hacker");
MODULE_LICENSE("GPL");

#define MODULE_NAME		"kbd"

#define KBD_MAJOR		42
#define KBD_MINOR		0
#define KBD_NR_MINORS	1

#define I8042_KBD_IRQ		1
#define I8042_STATUS_REG	0x65
#define I8042_DATA_REG		0x61

#define BUFFER_SIZE		1024
#define SCANCODE_RELEASED_MASK	0x80

struct kbd {
	struct cdev cdev;
	spinlock_t buf_lock;
	char buf[BUFFER_SIZE];
	size_t put_idx, get_idx, count;
} devs[1];

/*
 * Checks if scancode corresponds to key press or release.
 */
static int is_key_press(unsigned int scancode)
{
	return !(scancode & SCANCODE_RELEASED_MASK);
}

/*
 * Return the character of the given scancode.
 * Only works for alphanumeric/space/enter; returns '?' for other
 * characters.
 */
static int get_ascii(unsigned int scancode)
{
	static char *row1 = "1234567890";
	static char *row2 = "qwertyuiop";
	static char *row3 = "asdfghjkl";
	static char *row4 = "zxcvbnm";

	scancode &= ~SCANCODE_RELEASED_MASK;
	if (scancode >= 0x02 && scancode <= 0x0b)
		return *(row1 + scancode - 0x02);
	if (scancode >= 0x10 && scancode <= 0x19)
		return *(row2 + scancode - 0x10);
	if (scancode >= 0x1e && scancode <= 0x26)
		return *(row3 + scancode - 0x1e);
	if (scancode >= 0x2c && scancode <= 0x32)
		return *(row4 + scancode - 0x2c);
	if (scancode == 0x39)
		return ' ';
	if (scancode == 0x1c)
		return '\n';
	return '?';
}

static void put_char(struct kbd *data, char c)
{
	if (data->count >= BUFFER_SIZE)
		return;
	
	spin_lock(&data->buf_lock);
	data->buf[data->put_idx] = c;
	data->put_idx = (data->put_idx + 1) % BUFFER_SIZE;
	data->count++;
	spin_unlock(&data->buf_lock);
}

static bool get_char(char *c, struct kbd *data)
{
	unsigned long flags;

	/* Get char from buffer; update count and get_idx */
	if (data->count <= 0)
			return false;

	spin_lock_irqsave(&data->buf_lock, flags);
	*c = data->buf[data->get_idx];
	data->get_idx = (data->get_idx + 1) % BUFFER_SIZE;
	data->count--;
	spin_unlock_irqrestore(&data->buf_lock, flags);

	return true;
}

static void reset_buffer(struct kbd *data)
{
	unsigned long flags;

	/* Reset count, put_idx, get_idx */
	spin_lock_irqsave(&data->buf_lock, flags);
	memset(&data->buf, 0, BUFFER_SIZE);
	data->get_idx = 0;
	data->put_idx = 0;
	data->count = 0;
	spin_unlock_irqrestore(&data->buf_lock, flags);	
}

/*
 * Return the value of the DATA register.
 */
static inline u8 i8042_read_data(void)
{
	u8 val;
	/* Read DATA register (8 bits). */
	val = inb(I8042_DATA_REG);
	return val;
}

/* Implement interrupt handler */
irqreturn_t kbd_handler(int irq_no, void *dev_id) 
{
	u8 val;
	int ascii, pressed;
	struct kbd *data = (struct kbd *) dev_id;
	
	pr_info("kbd_handler is called\n");
	
	/* Read the scancode */
	val = i8042_read_data();
	
	/* Interpret the scancode */
	pressed = is_key_press(val);

	ascii = get_ascii(val);		

	/* Display information about the keystrokes */
	pr_info("IRQ %d: scancode=0x%x (%u) pressed=%d ch=%c\n", irq_no, val, val, pressed, ascii);
	
	/* Store ASCII key to buffer */
	if (pressed)
		put_char(data, ascii);
	
	return IRQ_NONE;
}

static int kbd_open(struct inode *inode, struct file *file)
{
	struct kbd *data = container_of(inode->i_cdev, struct kbd, cdev);

	file->private_data = data;
	pr_info("%s opened\n", MODULE_NAME);
	return 0;
}

static int kbd_release(struct inode *inode, struct file *file)
{
	pr_info("%s closed\n", MODULE_NAME);
	return 0;
}

/* Add write operation and reset the buffer */
static ssize_t kbd_write(struct file *file, const char __user *user_buffer, size_t size, loff_t *offset)
{
	struct kbd *data = (struct kbd *) file->private_data;
	reset_buffer(data);
	return size;
}

static ssize_t kbd_read(struct file *file,  char __user *user_buffer, size_t size, loff_t *offset)
{
	struct kbd *data = (struct kbd *) file->private_data;
	size_t read = 0;
	char c;

	/* Read data from buffer */
	while (size > 0) {
		if (get_char(&c, data)) {
			put_user(c, user_buffer);
			user_buffer++;
			size--;
			read++;
		} else  // no more data to read
				break;
	}

	return read;
}

static const struct file_operations kbd_fops = {
	.owner = THIS_MODULE,
	.open = kbd_open,
	.release = kbd_release,
	.write = kbd_write,
	.read = kbd_read,
};

static int kbd_init(void)
{
	int err;

	err = register_chrdev_region(MKDEV(KBD_MAJOR, KBD_MINOR),
				     KBD_NR_MINORS, MODULE_NAME);
	if (err != 0) {
		pr_err("register_region failed: %d\n", err);
		goto out;
	}

	/* 1: request the keyboard I/O ports */
	if (!request_region(I8042_STATUS_REG, 1, MODULE_NAME)){
		err = -EBUSY;
		pr_err("request_region failed: %d\n", err);
		goto out_unregister;
	}
	if (!request_region(I8042_DATA_REG, 1, MODULE_NAME)){
		err = -EBUSY;
		pr_err("request_region failed: %d\n", err);
		release_region(I8042_STATUS_REG, 1);
		goto out_unregister;
	}

	/* Initialize spinlock */
	spin_lock_init(&devs[0].buf_lock);

	/* Register IRQ handler for keyboard IRQ (IRQ 1). */
	err = request_irq(I8042_KBD_IRQ, kbd_handler, IRQF_SHARED, MODULE_NAME, &devs[0]);
	if (err < 0) {
		pr_err("request_irq failed: %d\n", err);
		release_region(I8042_STATUS_REG, 1);
		release_region(I8042_DATA_REG, 1);
		goto out_unregister;
	}

	cdev_init(&devs[0].cdev, &kbd_fops);
	cdev_add(&devs[0].cdev, MKDEV(KBD_MAJOR, KBD_MINOR), 1);

	pr_notice("Driver %s loaded\n", MODULE_NAME);
	return 0;

out_unregister:
	unregister_chrdev_region(MKDEV(KBD_MAJOR, KBD_MINOR),
				 KBD_NR_MINORS);
out:
	return err;
}

static void kbd_exit(void)
{
	cdev_del(&devs[0].cdev);

	/* Free IRQ. */
	free_irq(I8042_KBD_IRQ, &devs[0]);

	/* Release keyboard I/O ports */
	release_region(I8042_STATUS_REG, 1);
	release_region(I8042_DATA_REG, 1);


	unregister_chrdev_region(MKDEV(KBD_MAJOR, KBD_MINOR),
				 KBD_NR_MINORS);
	pr_notice("Driver %s unloaded\n", MODULE_NAME);
}

module_init(kbd_init);
module_exit(kbd_exit);

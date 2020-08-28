/*
 * SO2 Lab - Filesystem drivers
 * Exercise #1 (no-dev filesystem)
 */

#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/pagemap.h>

MODULE_DESCRIPTION("Simple no-dev filesystem");
MODULE_AUTHOR("SO2");
MODULE_LICENSE("GPL");

#define MYFS_BLOCKSIZE		4096
#define MYFS_BLOCKSIZE_BITS	12
#define MYFS_MAGIC		0xbeefcafe
#define LOG_LEVEL		KERN_ALERT

/* declarations of functions that are part of operation structures */

static int myfs_mknod(struct inode *dir,
		struct dentry *dentry, umode_t mode, dev_t dev);
static int myfs_create(struct inode *dir, struct dentry *dentry,
		umode_t mode, bool excl);
static int myfs_mkdir(struct inode *dir, struct dentry *dentry, umode_t mode);

/* Define super_operations structure */
static const struct super_operations myfs_ops = {
	.statfs = simple_statfs,
  .drop_inode = generic_delete_inode,
};

static const struct inode_operations myfs_dir_inode_operations = {
	.create = myfs_create,
  .lookup = simple_lookup,
  .link = simple_link,
  .unlink = simple_unlink,
  .mkdir = myfs_mkdir,
  .rmdir = simple_rmdir,
  .mknod = myfs_mknod,
  .rename = simple_rename,
};

static const struct file_operations myfs_file_operations = {
	.read_iter  = generic_file_read_iter,
	.write_iter = generic_file_write_iter,
	.mmap   = generic_file_mmap,
	.fsync    = noop_fsync,
	.splice_read  = generic_file_splice_read,
	.splice_write = iter_file_splice_write,
	.llseek   = generic_file_llseek,
};

static const struct inode_operations myfs_file_inode_operations = {
	.setattr  = simple_setattr,
	.getattr  = simple_getattr,
};

static const struct address_space_operations myfs_aops = {
	.readpage = simple_readpage,
	.write_begin  = simple_write_begin,
	.write_end  = simple_write_end,
};

struct inode *myfs_get_inode(struct super_block *sb, const struct inode *dir,
		int mode)
{
	struct inode *inode = new_inode(sb);

	if (!inode)
		return NULL;

	/* Fill inode structure */
	inode_init_owner(inode, NULL, mode);
	inode->i_atime = current_time(inode); 
	inode->i_ctime = current_time(inode); 
	inode->i_mtime = current_time(inode); 
	inode->i_ino = get_next_ino();


	/* Initialize address space operations. */
	inode->i_mapping->a_ops = &myfs_aops;
	
	/* Set file inode and file operations for regular files */
	if (S_ISREG(mode)) {
		inode->i_op = &myfs_file_inode_operations;
		inode->i_fop = &myfs_file_operations;	
	}

	if (S_ISDIR(mode)) {
		/* Set inode operations for dir inodes. */
		//inode->i_op = &simple_dir_inode_operations;
		inode->i_op = &myfs_dir_inode_operations;
		inode->i_fop = &simple_dir_operations;

		/* Directory inodes start off with i_nlink == 2 (for "." entry).
		 * Directory link count should be incremented (use inc_nlink). */
		inc_nlink(inode);
	}

	return inode;
}

/* Implement myfs_mknod, myfs_create, myfs_mkdir. */

static int myfs_mknod(struct inode *dir, struct dentry *dentry, umode_t mode, dev_t dev)
{	
	struct inode *inode = myfs_get_inode(dir->i_sb, dir, mode);
	if (inode) {
		d_instantiate(dentry, inode); 
		dget(dentry); 
		dir->i_mtime = current_time(dir);
		dir->i_ctime = current_time(dir);
		return 0;
	}	
	return -ENOSPC;
}

static int myfs_create(struct inode *dir, struct dentry *dentry, umode_t mode, bool excl) 
{
	return myfs_mknod(dir, dentry, mode | S_IFREG, 0);
}

static int myfs_mkdir(struct inode *dir, struct dentry *dentry, umode_t mode)
{
	int res = myfs_mknod(dir, dentry, mode | S_IFDIR, 0); 
	if (!res)
		inc_nlink(dir);
	return res;
} 


static int myfs_fill_super(struct super_block *sb, void *data, int silent)
{
	struct inode *root_inode;
	struct dentry *root_dentry;

	/* Fill super_block */
   sb->s_maxbytes          = MAX_LFS_FILESIZE;
   sb->s_blocksize         = MYFS_BLOCKSIZE;
   sb->s_blocksize_bits    = MYFS_BLOCKSIZE_BITS;
   sb->s_magic             = MYFS_MAGIC;
   sb->s_op                = &myfs_ops;

	/* mode = directory & access rights (755) */
	root_inode = myfs_get_inode(sb, NULL,
			S_IFDIR | S_IRWXU | S_IRGRP |
			S_IXGRP | S_IROTH | S_IXOTH);

	printk(LOG_LEVEL "root inode has %d link(s)\n", root_inode->i_nlink);

	if (!root_inode)
		return -ENOMEM;

	root_dentry = d_make_root(root_inode);
	if (!root_dentry)
		goto out_no_root;
	sb->s_root = root_dentry;

	return 0;

out_no_root:
	iput(root_inode);
	return -ENOMEM;
}

static struct dentry *myfs_mount(struct file_system_type *fs_type,
		int flags, const char *dev_name, void *data)
{
	/* Call superblock mount function */
	struct dentry *d = mount_nodev(fs_type, flags, data, myfs_fill_super);
	if (IS_ERR(d))
		return ERR_CAST(d);
	return d; 
}

/* Define file_system_type structure */
static struct file_system_type myfs_fs_type = {
        .name           = "myfs",
        .mount          = myfs_mount,
        .kill_sb        = kill_litter_super,
        .fs_flags       = FS_USERNS_MOUNT,
};


static int __init myfs_init(void)
{
	int err;

	/* Register */
	err = register_filesystem(&myfs_fs_type);
	if (err) {
		printk(LOG_LEVEL "register_filesystem failed\n");
		return err;
	}

	return 0;
}

static void __exit myfs_exit(void)
{
	/* Unregister */
	unregister_filesystem(&myfs_fs_type);
}

module_init(myfs_init);
module_exit(myfs_exit);

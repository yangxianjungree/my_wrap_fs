/********************************************************************************
File			: loopfs_main.c
Description		: my loop file sytem main routine

********************************************************************************/
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/namei.h>
#include <linux/slab.h>

#include "loopfs.h"
#include "loopfs_util.h"


#define	LOOPFS_VERSION_STRING		"loopfs_0.01"

#define	LOOPFS_PROC_MODULE_NAME		"loopfs"

#define	LOOPFS_MODULE_DESC			"My loop file system (loopfs)"
#define	LOOPFS_MODULE_AUTHOR		"Stephen.Yang <71696209@qq.com>"


static struct dentry* loopfs_mount(struct file_system_type *fs_type,
									int flags,
									const char *dev_name,
									void *data);


static struct file_system_type loopfs_fstype =
{
	.owner		= THIS_MODULE,
	.name		= LOOPFS_PROC_MODULE_NAME,
	.mount		= loopfs_mount,
	.kill_sb	= generic_shutdown_super,
	.fs_flags	= 0,
};

static void loopfs_read_super_block(struct super_block *sb)
{
	LDBG("Print information about loopfs super block.");

	LDBG("dev number = %u", sb->s_dev);
	LDBG("block size in bits = %u", sb->s_blocksize_bits);
	LDBG("block size = %lu", sb->s_blocksize);
	LDBG("max file size = %llu", sb->s_maxbytes);
	LDBG("file system name = %s", sb->s_type->name);
	LDBG("file system flags = %u", sb->s_type->fs_flags);
	LDBG("file system magic = %lu", sb->s_magic);
	LDBG("file system root name = %s", sb->s_root->d_name.name);
	LDBG("file system be refered count = %u", sb->s_count);
	LDBG("file system active = %u", sb->s_active.counter);
}

static int loopfs_fill_super_block(struct super_block *sb, void *raw_data, int silent)
{
	int err = 0;
	struct super_block *lower_sb;
	struct path lower_path;
	char *dev_name = (char *)raw_data;
	struct inode *inode;

	if (!dev_name) {
		LERR("read_super: missing dev_name argument.");
		err = -EINVAL;
		goto out;
	}

	/* parse lower path */
	err = kern_path(dev_name, LOOKUP_FOLLOW | LOOKUP_DIRECTORY, &lower_path);
	if (err) {
		LERR("error accessing ower directory '%s'.", dev_name);
		goto out;
	}

	/* allocate superblock private data */
	sb->s_fs_info = kzalloc(sizeof(struct loopfs_sb_info), GFP_KERNEL);
	if (!LOOPFS_SB(sb)) {
		LERR("loopfs_fill_super_block: out of memory");
		err = -ENOMEM;
		goto out_free;
	}

	/* initialize internal hash list */
	hash_init(LOOPFS_SB(sb)->hlist);
	spin_lock_init(&LOOPFS_SB(sb)->hlock);

	/* set the lower superblock field of upper superblock */
	lower_sb = lower_path.dentry->d_sb;
	atomic_inc(&lower_sb->s_active);
	
	loopfs_set_lower_super(sb, lower_sb);

	/* inherit maxbytes from lower file system */
	sb->s_maxbytes = lower_sb->s_maxbytes;

	/*
	 * Our c/m/atime granularity is 1 ns because we may stack on file
	 * systems whose granularity is as good.
	 */
	sb->s_time_gran = 1;

	sb->s_op = &loopfs_sops;

	/* get a new inode and allocate our root dentry */
	inode = loopfs_iget(sb, d_inode(lower_path.dentry));
	if (IS_ERR(inode)) {
		err = PTR_ERR(inode);
		goto out_sput;
	}
	sb->s_root = d_make_root(inode);
	if (!sb->s_root) {
		err = -ENOMEM;
		goto out_iput;
	}

	LDBG("loopfs is mounted !");

	/* print information about super block in loopfs */
	loopfs_read_super_block(sb);

	/*
	 * No need to call interpose because we already have a positive
	 * dentry, which was instantiated by d_make_root.  Just need to
	 * d_rehash it.
	 */
	d_rehash(sb->s_root);
	if (!silent) {
		LDBG("Mounted on top of %s type %s", dev_name,
			lower_sb->s_type->name);
	}

	goto out;


out_iput:
	iput(inode);
out_sput:
	/* drop refs we took earlier */
	atomic_dec(&lower_sb->s_active);
	kfree(LOOPFS_SB(sb));
	sb->s_fs_info = NULL;
out_free:
	path_put(&lower_path);

out:
	return err;
}

static struct dentry* loopfs_mount(struct file_system_type *fs_type,
									int flags,
									const char *dev_name,
									void *data)
{
	void *low_path = (void *)dev_name;

	LDBG("Mount entry, dev name: %s.", dev_name);
	
	return mount_nodev(fs_type, flags, low_path, loopfs_fill_super_block);
}


static int __init init_loopfs(void)
{
	LDBG("Module init, register file system.");

	return (register_filesystem(&loopfs_fstype));
}

/**
 * Description	:clean up loopfs module
 * 
 * Input		:void
 * Output		:void
 * Return		:void
 */
static void __exit exit_loopfs(void)
{
	LDBG("Module exit! Unregister file system.");

	unregister_filesystem(&loopfs_fstype);
}

/**
 * General Module Information
 * 
 * Kernel Registrations
 */
module_init(init_loopfs);
module_exit(exit_loopfs);

MODULE_AUTHOR(LOOPFS_MODULE_AUTHOR);
MODULE_DESCRIPTION(LOOPFS_MODULE_DESC);
MODULE_LICENSE("GPL");

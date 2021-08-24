/********************************************************************************
File			: loopfs_main.c
Description		: my loop file sytem main routine

********************************************************************************/
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/namei.h>

#include "loopfs.h"


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
	.fs_flags	= 0,
};

static void loopfs_read_low_super_block(struct super_block *lsb)
{
	LDBG("Print information about lower super block.");

	LDBG("dev number = %u", lsb->s_dev);
	LDBG("block size in bits = %u", lsb->s_blocksize_bits);
	LDBG("block size = %lu", lsb->s_blocksize);
	LDBG("max file size = %llu", lsb->s_maxbytes);
	LDBG("file system name = %s", lsb->s_type->name);
	LDBG("file system flags = %u", lsb->s_type->fs_flags);
	LDBG("file system magic = %lu", lsb->s_magic);
	LDBG("file system root name = %s", lsb->s_root->d_name.name);
	LDBG("file system be refered count = %u", lsb->s_count);
	LDBG("file system active = %u", lsb->s_active.counter);
}

static int loopfs_fill_super_block(struct super_block *sb, void *raw_data, int silent)
{
	int err = 0;
	struct super_block *lower_sb;
	struct path lower_path;
	char *dev_name = (char *)raw_data;

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

	/* set the lower superblock field of upper superblock */
	lower_sb = lower_path.dentry->d_sb;

	/* print information about lower super block. */
	loopfs_read_low_super_block(lower_sb);

	return 0;

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

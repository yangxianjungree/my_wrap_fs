/********************************************************************************
File			: loopfs_main.c
Description		: my loop file sytem main routine

********************************************************************************/
#include <linux/module.h>
#include <linux/fs.h>

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
	.fs_flags	= 0,
};


static struct dentry* loopfs_mount(struct file_system_type *fs_type,
									int flags,
									const char *dev_name,
									void *data)
{
	DBG_PRINT("i am not yet implemented.\n");
	return (NULL);
}


static int __init init_loopfs(void)
{
	DBG_PRINT("Hello, world\n");

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
	DBG_PRINT("Good-bye!\n");

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

/********************************************************************************
File			: super.c
Description		: Defines for my loop filesystem super block

********************************************************************************/
#include <linux/slab.h>
#include <linux/fs.h>
#include <linux/mm.h>

#include "loopfs.h"
#include "loopfs_util.h"


static void loopfs_destroy_inode(struct inode *inode)
{
	LDBG("loopfs_destroy_inode");
}

/* final actions when unmounting a file system */
static void loopfs_put_super(struct super_block *sb)
{

	struct loopfs_sb_info *spd;
	struct super_block *s;
	
	LDBG("loopfs_put_super");

	spd = LOOPFS_SB(sb);
	if (!spd) {
		return;
	}

	/* decrement lower super references */
	s = loopfs_lower_super(sb);
	loopfs_set_lower_super(sb, NULL);
	atomic_dec(&s->s_active);

	kfree(spd);
	sb->s_fs_info = NULL;
}

static int loopfs_statfs(struct dentry *dentry, struct kstatfs *buf)
{
	LDBG("loopfs_statfs");
	
	return 0;
}

/*
 * @flags: numeric mount options
 * @options: mount options string
 */
static int loopfs_remount_fs(struct super_block *sb, int *flags, char *options)
{
	LDBG("loopfs_remount_fs: remount flags 0x%x.", *flags);

	return 0;
}

/*
 * Used only in nfs, to kill any pending RPC tasks, so that subsequent
 * code can actually succeed and won't leave tasks that need handling.
 */
static void loopfs_umount_begin(struct super_block *sb)
{
	LDBG("loopfs_umount_begin");
}


struct super_operations loopfs_sops = {
	.destroy_inode	= loopfs_destroy_inode,
	.drop_inode		= generic_delete_inode,
	.put_super		= loopfs_put_super,
	.statfs			= loopfs_statfs,
	.remount_fs		= loopfs_remount_fs,
	.umount_begin	= loopfs_umount_begin,
};

/********************************************************************************
File			: super.c
Description		: Defines for my loop filesystem super block

********************************************************************************/
#include <linux/slab.h>
#include <linux/fs.h>
#include <linux/mm.h>
#include <linux/mount.h>
#include <linux/statfs.h>

#include "loopfs.h"
#include "loopfs_util.h"



/*
 * The inode cache is used with alloc_inode for both our inode info and the
 * vfs inode.
 */
static struct kmem_cache *loopfs_inode_cachep;


/* loopfs inode cache constructor */
static void init_once(void *obj)
{
	struct loopfs_inode_info *i = obj;

	inode_init_once(&i->vfs_inode);
}

int loopfs_init_inode_cache(void)
{
	int err = 0;

	loopfs_inode_cachep =
		kmem_cache_create("loopfs_inode_cache",
					sizeof(struct loopfs_inode_info), 0,
					SLAB_RECLAIM_ACCOUNT, init_once);
	if (!loopfs_inode_cachep) {
		err = -ENOMEM;
	}
	return err;
}

/* loopfs inode cache destructor */
void loopfs_destroy_inode_cache(void)
{
	if (loopfs_inode_cachep) {
		kmem_cache_destroy(loopfs_inode_cachep);
	}
}


static struct inode *loopfs_alloc_inode(struct super_block *sb)
{
	struct loopfs_inode_info *i;
	LDBG("loopfs_alloc_inode\n");

	i = kmem_cache_alloc(loopfs_inode_cachep, GFP_KERNEL);
	if (!i) {
		return NULL;
	}

	/* memset everything up to the inode to 0 */
	memset(i, 0, offsetof(struct loopfs_inode_info, vfs_inode));

	atomic64_set(&i->vfs_inode.i_version, 1);
	return &i->vfs_inode;
}

static void loopfs_destroy_inode(struct inode *inode)
{
	LDBG("loopfs_destroy_inode\n");
	
	kmem_cache_free(loopfs_inode_cachep, LOOPFS_I(inode));
}

/*
 * Called by iput() when the inode reference count reached zero
 * and the inode is not hashed anywhere.  Used to clear anything
 * that needs to be, before the inode is completely destroyed and put
 * on the inode free list.
 */
static void loopfs_evict_inode(struct inode *inode)
{
	struct inode *lower_inode;

	truncate_inode_pages(&inode->i_data, 0);
	clear_inode(inode);
	/*
	 * Decrement a reference to a lower_inode, which was incremented
	 * by our read_inode when it was created initially.
	 */
	lower_inode = loopfs_lower_inode(inode);
	loopfs_set_lower_inode(inode, NULL);
	iput(lower_inode);
}

/* final actions when unmounting a file system */
static void loopfs_put_super(struct super_block *sb)
{
	struct loopfs_sb_info *spd;
	struct super_block *s;

	LDBG("loopfs_put_super\n");

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
	int err = 0;	
	struct path lower_path;

	LDBG("loopfs_statfs\n");

	loopfs_get_lower_path(dentry, &lower_path);
	err = vfs_statfs(&lower_path, buf);
	loopfs_put_lower_path(dentry, &lower_path);

	/* set return buf to our f/s to avoid confusing user-level utils */
	buf->f_type = LOOPFS_SUPER_MAGIC;

	return err;
}

/*
 * @flags: numeric mount options
 * @options: mount options string
 */
static int loopfs_remount_fs(struct super_block *sb, int *flags, char *options)
{
	int err = 0;

	LDBG("loopfs_remount_fs: remount flags 0x%x.\n", *flags);
	/*
	 * The VFS will take care of "ro" and "rw" flags among others.  We
	 * can safely accept a few flags (RDONLY, MANDLOCK), and honor
	 * SILENT, but anything else left over is an error.
	 */
	if ((*flags & ~(SB_RDONLY | SB_MANDLOCK | SB_SILENT)) != 0) {
		LDBG("loopfs_remount_fs: remount flags 0x%x unsupported\n", *flags);
		err = -EINVAL;
	}

	return err;
}

/*
 * Used only in nfs, to kill any pending RPC tasks, so that subsequent
 * code can actually succeed and won't leave tasks that need handling.
 */
static void loopfs_umount_begin(struct super_block *sb)
{
	LDBG("loopfs_umount_begin\n");
}


const struct super_operations loopfs_sops = {
	.alloc_inode	= loopfs_alloc_inode,
	.destroy_inode	= loopfs_destroy_inode, /* first called when umount */
	.drop_inode		= generic_delete_inode,
	.evict_inode	= loopfs_evict_inode,
	.put_super		= loopfs_put_super, /* secondly called when umount */
	.statfs			= loopfs_statfs,
	.remount_fs		= loopfs_remount_fs,
	.umount_begin	= loopfs_umount_begin,
};

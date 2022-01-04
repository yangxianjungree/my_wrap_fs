/********************************************************************************
File			: loopfs.h
Description		: Defines for my loop file system

********************************************************************************/
#ifndef	__LOOP_FS_H__
#define	__LOOP_FS_H__

#include <linux/fs.h>
#include <linux/hashtable.h>
#include <linux/spinlock.h>

#define LOOPFS_SUPER_MAGIC		0xb550ca10

/* loopfs super-block data in memory */
struct loopfs_sb_info {
	struct super_block *lower_sb;
	DECLARE_HASHTABLE(hlist, 4);
	spinlock_t hlock;
};

/* loopfs inode data in memory */
struct loopfs_inode_info {
	struct inode *lower_inode;
	struct inode vfs_inode;
};

/* loopfs dentry data in memory */
struct loopfs_dentry_info {
	spinlock_t lock;	/* protects lower_path */
	struct path lower_path;
};

/*
 * inode to private data
 *
 * Since we use containers and the struct inode is _inside_ the
 * loopfs_inode_info structure, LOOPFS_I will always (given a non-NULL
 * inode pointer), return a valid non-NULL pointer.
 */
static inline struct loopfs_inode_info *LOOPFS_I(const struct inode *inode)
{
	return container_of(inode, struct loopfs_inode_info, vfs_inode);
}

/* superblock to private data */
#define LOOPFS_SB(super) ((struct loopfs_sb_info *)(super)->s_fs_info)

/* dentry to private data */
#define LOOPFS_D(dent) ((struct loopfs_dentry_info *)(dent)->d_fsdata)



/* superblock to lower superblock */
static inline struct super_block *loopfs_lower_super(
	const struct super_block *sb)
{
	return LOOPFS_SB(sb)->lower_sb;
}

static inline void loopfs_set_lower_super(struct super_block *sb,
				struct super_block *val)
{
	LOOPFS_SB(sb)->lower_sb = val;
}

/* inode to lower inode. */
static inline struct inode *loopfs_lower_inode(const struct inode *i)
{
	return LOOPFS_I(i)->lower_inode;
}

/* path based (dentry/mnt) macros */
static inline void pathcpy(struct path *dst, const struct path *src)
{
	dst->dentry = src->dentry;
	dst->mnt = src->mnt;
}

/* Returns struct path.  Caller must path_put it. */
static inline void loopfs_get_lower_path(const struct dentry *dent,
					 struct path *lower_path)
{
	spin_lock(&LOOPFS_D(dent)->lock);
	pathcpy(lower_path, &LOOPFS_D(dent)->lower_path);
	path_get(lower_path);
	spin_unlock(&LOOPFS_D(dent)->lock);
	return;
}

extern struct inode *loopfs_iget(struct super_block *sb, struct inode *lower_inode);



extern struct super_operations loopfs_sops;

#endif	// __LOOP_FS_H__

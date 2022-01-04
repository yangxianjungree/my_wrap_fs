/********************************************************************************
File			: loopfs.h
Description		: Defines for my loop file system

********************************************************************************/
#ifndef	__LOOP_FS_H__
#define	__LOOP_FS_H__

#include <linux/fs.h>
#include <linux/hashtable.h>
#include <linux/spinlock.h>
#include <linux/mm.h>

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

/* file private data */
struct loopfs_file_info {
	struct file *lower_file;
	const struct vm_operations_struct *lower_vm_ops;
};

// struct loopfs_hnode {
// 	struct hlist_node hnode;
// 	char *path;
// 	unsigned long inode;
// 	unsigned int flags;
// };


// /* flags */
// #define LOOPFS_HIDE	(1 << 0)
// #define LOOPFS_BLOCK	(1 << 1)


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

/* file to private Data */
#define LOOPFS_F(file) ((struct loopfs_file_info *)((file)->private_data))



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

static inline void loopfs_set_lower_inode(struct inode *i, struct inode *val)
{
	LOOPFS_I(i)->lower_inode = val;
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

static inline void loopfs_put_lower_path(const struct dentry *dent,
					struct path *lower_path)
{
	path_put(lower_path);
	return;
}

static inline void loopfs_set_lower_path(const struct dentry *dent,
					struct path *lower_path)
{
	spin_lock(&LOOPFS_D(dent)->lock);
	pathcpy(&LOOPFS_D(dent)->lower_path, lower_path);
	spin_unlock(&LOOPFS_D(dent)->lock);
	return;
}

static inline void loopfs_reset_lower_path(const struct dentry *dent)
{
	spin_lock(&LOOPFS_D(dent)->lock);
	LOOPFS_D(dent)->lower_path.dentry = NULL;
	LOOPFS_D(dent)->lower_path.mnt = NULL;
	spin_unlock(&LOOPFS_D(dent)->lock);
	return;
}

static inline void loopfs_put_reset_lower_path(const struct dentry *dent)
{
	struct path lower_path;
	spin_lock(&LOOPFS_D(dent)->lock);
	pathcpy(&lower_path, &LOOPFS_D(dent)->lower_path);
	LOOPFS_D(dent)->lower_path.dentry = NULL;
	LOOPFS_D(dent)->lower_path.mnt = NULL;
	spin_unlock(&LOOPFS_D(dent)->lock);
	path_put(&lower_path);
	return;
}

/* file to lower file */
static inline struct file *loopfs_lower_file(const struct file *f)
{
	return LOOPFS_F(f)->lower_file;
}

static inline void loopfs_set_lower_file(struct file *f, struct file *val)
{
	LOOPFS_F(f)->lower_file = val;
}




extern struct inode *loopfs_iget(struct super_block *sb, struct inode *lower_inode);
extern struct dentry *loopfs_lookup(struct inode *dir, struct dentry *dentry,
				unsigned int flags);
extern int loopfs_is_blocked(struct loopfs_sb_info *sbinfo, const char *path, unsigned long inode);



extern const struct super_operations loopfs_sops;
extern const struct dentry_operations loopfs_dops;
extern const struct inode_operations loopfs_symlink_iops;
extern const struct inode_operations loopfs_dir_iops;
extern const struct inode_operations loopfs_main_iops;
extern const struct file_operations loopfs_main_fops;
extern const struct file_operations loopfs_dir_fops;
extern const struct address_space_operations loopfs_aops;
extern const struct vm_operations_struct loopfs_vm_ops;


extern int loopfs_init_inode_cache(void);
extern void loopfs_destroy_inode_cache(void);
extern int loopfs_init_dentry_cache(void);
extern void loopfs_destroy_dentry_cache(void);
extern int new_dentry_private_data(struct dentry *dentry);
extern void free_dentry_private_data(struct dentry *dentry);


/* Export symbol from kernel */
extern int vfs_path_lookup(struct dentry *dentry, struct vfsmount *mnt,
		    const char *name, unsigned int flags,
		    struct path *path);

#endif	// __LOOP_FS_H__

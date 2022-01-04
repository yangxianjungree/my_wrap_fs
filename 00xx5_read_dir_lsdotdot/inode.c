#include "loopfs.h"
#include "loopfs_util.h"


static int loopfs_create(struct inode *dir, struct dentry *dentry,
				umode_t mode, bool want_excl)
{
	LDBG("loopfs_create\n");

	return 0;
}

static int loopfs_link(struct dentry *old_dentry, struct inode *dir,
				struct dentry *new_dentry)
{
	LDBG("loopfs_link\n");

	return 0;
}

static int loopfs_unlink(struct inode *dir, struct dentry *dentry)
{
	LDBG("loopfs_unlink\n");

	return 0;
}

static int loopfs_symlink(struct inode *dir, struct dentry *dentry,
				const char *symname)
{
	LDBG("loopfs_symlink\n");

	return 0;
}

static int loopfs_mkdir(struct inode *dir, struct dentry *dentry, umode_t mode)
{
	LDBG("loopfs_mkdir\n");

	return 0;
}

static int loopfs_rmdir(struct inode *dir, struct dentry *dentry)
{
	LDBG("loopfs_rmdir\n");

	return 0;
}

static int loopfs_mknod(struct inode *dir, struct dentry *dentry, umode_t mode,
				dev_t dev)
{
	LDBG("loopfs_mknod\n");

	return 0;
}

/*
 * The locking rules in loopfs_rename are complex.  We could use a simpler
 * superblock-level name-space lock for renames and copy-ups.
 */
static int loopfs_rename(struct inode *old_dir, struct dentry *old_dentry,
				struct inode *new_dir, struct dentry *new_dentry, unsigned int flags)
{
	LDBG("loopfs_rename\n");

	return 0;
}

static int loopfs_readlink(struct dentry *dentry, char __user *buf, int bufsiz)
{
	LDBG("loopfs_readlink\n");

	return 0;
}

static int loopfs_permission(struct inode *inode, int mask)
{
	LDBG("loopfs_permission\n");

	return 0;
}

static int loopfs_setattr(struct dentry *dentry, struct iattr *ia)
{
	LDBG("loopfs_setattr\n");

	return 0;
}

static int loopfs_getattr(const struct path *k_path, struct kstat *stat,
				u32 tt, unsigned int flags)
{
	LDBG("loopfs_getattr\n");

	return 0;
}

static int loopfs_setxattr(struct dentry *dentry, const char *name,
				const void *value, size_t size, int flags)
{
	LDBG("loopfs_setxattr\n");

	return 0;
}

static ssize_t loopfs_getxattr(struct dentry *dentry, const char *name,
				void *buffer, size_t size)
{
	LDBG("loopfs_getxattr\n");

	return 0;
}

static ssize_t loopfs_listxattr(struct dentry *dentry, char *buffer, size_t buffer_size)
{
	LDBG("loopfs_listxattr\n");

	return 0;
}

static int loopfs_removexattr(struct dentry *dentry, const char *name)
{
	LDBG("loopfs_removexattr\n");

	return 0;
}

const struct inode_operations loopfs_dir_iops = {
	.create		= loopfs_create,
	.lookup		= loopfs_lookup,
	.link		= loopfs_link,
	.unlink		= loopfs_unlink,
	.symlink	= loopfs_symlink,
	.mkdir		= loopfs_mkdir,
	.rmdir		= loopfs_rmdir,
	.mknod		= loopfs_mknod,
	.rename		= loopfs_rename,
	// .permission	= loopfs_permission,
	.setattr	= loopfs_setattr,
	// .getattr	= loopfs_getattr,
	// .listxattr	= loopfs_listxattr,
};
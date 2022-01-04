#include <linux/fs_stack.h>
#include <linux/namei.h>

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
	int err;
	struct dentry *lower_dentry;
	struct inode *lower_dir_inode = loopfs_lower_inode(dir);
	struct dentry *lower_dir_dentry;
	struct path lower_path;

	LDBG("loopfs_unlink\n");

	loopfs_get_lower_path(dentry, &lower_path);
	lower_dentry = lower_path.dentry;
	dget(lower_dentry);
	lower_dir_dentry = lock_parent(lower_dentry);
	if (lower_dentry->d_parent != lower_dir_dentry || d_unhashed(lower_dentry)) {
		err = -EINVAL;
		goto out;
	}

	err = vfs_unlink(lower_dir_inode, lower_dentry, NULL);

	/*
	 * Note: unlinking on top of NFS can cause silly-renamed files.
	 * Trying to delete such files results in EBUSY from NFS
	 * below.  Silly-renamed files will get deleted by NFS later on, so
	 * we just need to detect them here and treat such EBUSY errors as
	 * if the upper file was successfully deleted.
	 */
	if (err == -EBUSY && lower_dentry->d_flags & DCACHE_NFSFS_RENAMED) {
		err = 0;
	}
	if (err) {
		goto out;
	}
	fsstack_copy_attr_times(dir, lower_dir_inode);
	fsstack_copy_inode_size(dir, lower_dir_inode);
	set_nlink(d_inode(dentry), loopfs_lower_inode(d_inode(dentry))->i_nlink);
	d_inode(dentry)->i_ctime = dir->i_ctime;
	d_drop(dentry); /* this is needed, else LTP fails (VFS won't do it) */
out:
	unlock_dir(lower_dir_dentry);
	dput(lower_dentry);
	loopfs_put_lower_path(dentry, &lower_path);
	return err;
}

static int loopfs_symlink(struct inode *dir, struct dentry *dentry,
				const char *symname)
{
	LDBG("loopfs_symlink\n");

	return 0;
}

static int loopfs_mkdir(struct inode *dir, struct dentry *dentry, umode_t mode)
{
	int err;
	struct dentry *lower_dentry;
	struct dentry *lower_parent_dentry = NULL;
	struct path lower_path;

	LDBG("loopfs_mkdir\n");

	loopfs_get_lower_path(dentry, &lower_path);
	lower_dentry = lower_path.dentry;
	lower_parent_dentry = lock_parent(lower_dentry);

	err = vfs_mkdir(d_inode(lower_parent_dentry), lower_dentry, mode);
	if (err) {
		goto out;
	}

	err = loopfs_interpose(dentry, dir->i_sb, &lower_path);
	if (err) {
		goto out;
	}

	fsstack_copy_attr_times(dir, loopfs_lower_inode(dir));
	fsstack_copy_inode_size(dir, d_inode(lower_parent_dentry));
	/* update number of links on parent directory */
	set_nlink(dir, loopfs_lower_inode(dir)->i_nlink);

out:
	unlock_dir(lower_parent_dentry);
	loopfs_put_lower_path(dentry, &lower_path);
	return err;
}

static int loopfs_rmdir(struct inode *dir, struct dentry *dentry)
{
	struct dentry *lower_dentry;
	struct dentry *lower_dir_dentry;
	int err;
	struct path lower_path;

	LDBG("loopfs_rmdir\n");

	loopfs_get_lower_path(dentry, &lower_path);
	lower_dentry = lower_path.dentry;
	lower_dir_dentry = lock_parent(lower_dentry);
	if (lower_dentry->d_parent != lower_dir_dentry ||
		d_unhashed(lower_dentry)) {
		err = -EINVAL;
		goto out;
	}

	err = vfs_rmdir(d_inode(lower_dir_dentry), lower_dentry);
	if (err) {
		goto out;
	}

	d_drop(dentry);	/* drop our dentry on success (why not VFS's job?) */
	if (d_inode(dentry)) {
		clear_nlink(d_inode(dentry));
	}
	fsstack_copy_attr_times(dir, d_inode(lower_dir_dentry));
	fsstack_copy_inode_size(dir, d_inode(lower_dir_dentry));
	set_nlink(dir, d_inode(lower_dir_dentry)->i_nlink);

out:
	unlock_dir(lower_dir_dentry);
	loopfs_put_lower_path(dentry, &lower_path);
	return err;
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
	int err = 0;
	struct dentry *lower_old_dentry = NULL;
	struct dentry *lower_new_dentry = NULL;
	struct dentry *lower_old_dir_dentry = NULL;
	struct dentry *lower_new_dir_dentry = NULL;
	struct dentry *trap = NULL;
	struct path lower_old_path, lower_new_path;

	LDBG("loopfs_rename\n");

	if (flags) {
		return -EINVAL;
	}

	loopfs_get_lower_path(old_dentry, &lower_old_path);
	loopfs_get_lower_path(new_dentry, &lower_new_path);
	lower_old_dentry = lower_old_path.dentry;
	lower_new_dentry = lower_new_path.dentry;
	lower_old_dir_dentry = dget_parent(lower_old_dentry);
	lower_new_dir_dentry = dget_parent(lower_new_dentry);

	trap = lock_rename(lower_old_dir_dentry, lower_new_dir_dentry);
	err = -EINVAL;
	/* check for unexpected namespace changes */
	if (lower_old_dentry->d_parent != lower_old_dir_dentry) {
		goto out;
	}
	if (lower_new_dentry->d_parent != lower_new_dir_dentry) {
		goto out;
	}
	/* check if either dentry got unlinked */
	if (d_unhashed(lower_old_dentry) || d_unhashed(lower_new_dentry)) {
		goto out;
	}
	/* source should not be ancestor of target */
	if (trap == lower_old_dentry) {
		goto out;
	}
	/* target should not be ancestor of source */
	if (trap == lower_new_dentry) {
		err = -ENOTEMPTY;
		goto out;
	}

	err = vfs_rename(d_inode(lower_old_dir_dentry), lower_old_dentry,
				d_inode(lower_new_dir_dentry), lower_new_dentry, NULL, 0);
	if (err) {
		goto out;
	}

	fsstack_copy_attr_all(new_dir, d_inode(lower_new_dir_dentry));
	fsstack_copy_inode_size(new_dir, d_inode(lower_new_dir_dentry));
	if (new_dir != old_dir) {
		fsstack_copy_attr_all(old_dir, d_inode(lower_old_dir_dentry));
		fsstack_copy_inode_size(old_dir, d_inode(lower_old_dir_dentry));
	}

out:
	unlock_rename(lower_old_dir_dentry, lower_new_dir_dentry);
	dput(lower_old_dir_dentry);
	dput(lower_new_dir_dentry);
	loopfs_put_lower_path(old_dentry, &lower_old_path);
	loopfs_put_lower_path(new_dentry, &lower_new_path);
	return err;
}

// static int loopfs_readlink(struct dentry *dentry, char __user *buf, int bufsiz)
// {
// 	LDBG("loopfs_readlink\n");

// 	return 0;
// }

// static int loopfs_permission(struct inode *inode, int mask)
// {
// 	LDBG("loopfs_permission\n");

// 	return 0;
// }

static int loopfs_setattr(struct dentry *dentry, struct iattr *ia)
{
	LDBG("loopfs_setattr\n");

	return 0;
}

// static int loopfs_getattr(const struct path *k_path, struct kstat *stat,
// 				u32 tt, unsigned int flags)
// {
// 	LDBG("loopfs_getattr\n");

// 	return 0;
// }

// static int loopfs_setxattr(struct dentry *dentry, const char *name,
// 				const void *value, size_t size, int flags)
// {
// 	LDBG("loopfs_setxattr\n");

// 	return 0;
// }

// static ssize_t loopfs_getxattr(struct dentry *dentry, const char *name,
// 				void *buffer, size_t size)
// {
// 	LDBG("loopfs_getxattr\n");

// 	return 0;
// }

// static ssize_t loopfs_listxattr(struct dentry *dentry, char *buffer, size_t buffer_size)
// {
// 	LDBG("loopfs_listxattr\n");

// 	return 0;
// }

// static int loopfs_removexattr(struct dentry *dentry, const char *name)
// {
// 	LDBG("loopfs_removexattr\n");

// 	return 0;
// }

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
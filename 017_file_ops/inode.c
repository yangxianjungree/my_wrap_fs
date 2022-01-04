#include <linux/fs_stack.h>
#include <linux/namei.h>

#include "loopfs.h"
#include "loopfs_util.h"


static int loopfs_create(struct inode *dir, struct dentry *dentry,
				umode_t mode, bool want_excl)
{
	int err;
	struct dentry *lower_dentry;
	struct dentry *lower_parent_dentry = NULL;
	struct path lower_path;

	LDBG("loopfs_create\n");

	loopfs_get_lower_path(dentry, &lower_path);
	lower_dentry = lower_path.dentry;
	lower_parent_dentry = lock_parent(lower_dentry);

	err = vfs_create(d_inode(lower_parent_dentry), lower_dentry, mode, want_excl);
	if (err) {
		goto out;
	}
	err = loopfs_interpose(dentry, dir->i_sb, &lower_path);
	if (err) {
		goto out;
	}
	fsstack_copy_attr_times(dir, loopfs_lower_inode(dir));
	fsstack_copy_inode_size(dir, d_inode(lower_parent_dentry));

out:
	unlock_dir(lower_parent_dentry);
	loopfs_put_lower_path(dentry, &lower_path);
	return err;
}

static int loopfs_link(struct dentry *old_dentry, struct inode *dir,
				struct dentry *new_dentry)
{
	struct dentry *lower_old_dentry;
	struct dentry *lower_new_dentry;
	struct dentry *lower_dir_dentry;
	u64 file_size_save;
	int err;
	struct path lower_old_path, lower_new_path;

	LDBG("loopfs_link\n");

	file_size_save = i_size_read(d_inode(old_dentry));
	loopfs_get_lower_path(old_dentry, &lower_old_path);
	loopfs_get_lower_path(new_dentry, &lower_new_path);
	lower_old_dentry = lower_old_path.dentry;
	lower_new_dentry = lower_new_path.dentry;
	lower_dir_dentry = lock_parent(lower_new_dentry);

	err = vfs_link(lower_old_dentry, d_inode(lower_dir_dentry), lower_new_dentry, NULL);
	if (err || d_really_is_negative(lower_new_dentry)) {
		goto out;
	}

	err = loopfs_interpose(new_dentry, dir->i_sb, &lower_new_path);
	if (err) {
		goto out;
	}
	fsstack_copy_attr_times(dir, d_inode(lower_new_dentry));
	fsstack_copy_inode_size(dir, d_inode(lower_new_dentry));
	set_nlink(d_inode(old_dentry), loopfs_lower_inode(d_inode(old_dentry))->i_nlink);
	i_size_write(d_inode(new_dentry), file_size_save);
out:
	unlock_dir(lower_dir_dentry);
	loopfs_put_lower_path(old_dentry, &lower_old_path);
	loopfs_put_lower_path(new_dentry, &lower_new_path);
	return err;
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
	int err;
	struct dentry *lower_dentry;
	struct dentry *lower_parent_dentry = NULL;
	struct path lower_path;

	LDBG("loopfs_symlink\n");

	loopfs_get_lower_path(dentry, &lower_path);
	lower_dentry = lower_path.dentry;
	lower_parent_dentry = lock_parent(lower_dentry);

	err = vfs_symlink(d_inode(lower_parent_dentry), lower_dentry, symname);
	if (err) {
		goto out;
	}
	err = loopfs_interpose(dentry, dir->i_sb, &lower_path);
	if (err) {
		goto out;
	}
	fsstack_copy_attr_times(dir, loopfs_lower_inode(dir));
	fsstack_copy_inode_size(dir, d_inode(lower_parent_dentry));

out:
	unlock_dir(lower_parent_dentry);
	loopfs_put_lower_path(dentry, &lower_path);
	return err;
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
	int err;
	struct dentry *lower_dentry;
	struct dentry *lower_parent_dentry = NULL;
	struct path lower_path;

	LDBG("loopfs_mknod\n");

	loopfs_get_lower_path(dentry, &lower_path);
	lower_dentry = lower_path.dentry;
	lower_parent_dentry = lock_parent(lower_dentry);

	err = vfs_mknod(d_inode(lower_parent_dentry), lower_dentry, mode, dev);
	if (err) {
		goto out;
	}

	err = loopfs_interpose(dentry, dir->i_sb, &lower_path);
	if (err) {
		goto out;
	}
	fsstack_copy_attr_times(dir, loopfs_lower_inode(dir));
	fsstack_copy_inode_size(dir, d_inode(lower_parent_dentry));

out:
	unlock_dir(lower_parent_dentry);
	loopfs_put_lower_path(dentry, &lower_path);
	return err;
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

static const char *loopfs_get_link(struct dentry *dentry, struct inode *inode,
				struct delayed_call *done)
{
	DEFINE_DELAYED_CALL(lower_done);
	struct dentry *lower_dentry;
	struct path lower_path;
	char *buf;
	const char *lower_link;

	LDBG("loopfs_get_link\n");

	if (!dentry) {
		return ERR_PTR(-ECHILD);
	}

	loopfs_get_lower_path(dentry, &lower_path);
	lower_dentry = lower_path.dentry;

	/*
	 * get link from lower file system, but use a separate
	 * delayed_call callback.
	 */
	lower_link = vfs_get_link(lower_dentry, &lower_done);
	if (IS_ERR(lower_link)) {
		buf = ERR_CAST(lower_link);
		goto out;
	}

	/*
	 * we can't pass lower link up: have to make private copy and
	 * pass that.
	 */
	buf = kstrdup(lower_link, GFP_KERNEL);
	do_delayed_call(&lower_done);
	if (!buf) {
		buf = ERR_PTR(-ENOMEM);
		goto out;
	}

	fsstack_copy_attr_atime(d_inode(dentry), d_inode(lower_dentry));

	set_delayed_call(done, kfree_link, buf);
out:
	loopfs_put_lower_path(dentry, &lower_path);
	return buf;
}

static int loopfs_permission(struct inode *inode, int mask)
{
	struct inode *lower_inode;
	int err;

	LDBG("loopfs_permission\n");
	
	lower_inode = loopfs_lower_inode(inode);
	err = inode_permission(lower_inode, mask);
	return err;
}

static int loopfs_setattr(struct dentry *dentry, struct iattr *ia)
{
	int err;
	struct dentry *lower_dentry;
	struct inode *inode;
	struct inode *lower_inode;
	struct path lower_path;
	struct iattr lower_ia;

	LDBG("loopfs_setattr\n");

	inode = d_inode(dentry);

	/*
	 * Check if user has permission to change inode.  We don't check if
	 * this user can change the lower inode: that should happen when
	 * calling notify_change on the lower inode.
	 */
	err = setattr_prepare(dentry, ia);
	if (err) {
		goto out_err;
	}

	loopfs_get_lower_path(dentry, &lower_path);
	lower_dentry = lower_path.dentry;
	lower_inode = loopfs_lower_inode(inode);

	/* prepare our own lower struct iattr (with the lower file) */
	memcpy(&lower_ia, ia, sizeof(lower_ia));
	if (ia->ia_valid & ATTR_FILE) {
		lower_ia.ia_file = loopfs_lower_file(ia->ia_file);
	}

	/*
	 * If shrinking, first truncate upper level to cancel writing dirty
	 * pages beyond the new eof; and also if its' maxbytes is more
	 * limiting (fail with -EFBIG before making any change to the lower
	 * level).  There is no need to vmtruncate the upper level
	 * afterwards in the other cases: we fsstack_copy_inode_size from
	 * the lower level.
	 */
	if (ia->ia_valid & ATTR_SIZE) {
		err = inode_newsize_ok(inode, ia->ia_size);
		if (err) {
			goto out;
		}
		truncate_setsize(inode, ia->ia_size);
	}

	/*
	 * mode change is for clearing setuid/setgid bits. Allow lower fs
	 * to interpret this in its own way.
	 */
	if (lower_ia.ia_valid & (ATTR_KILL_SUID | ATTR_KILL_SGID)) {
		lower_ia.ia_valid &= ~ATTR_MODE;
	}

	/* notify the (possibly copied-up) lower inode */
	/*
	 * Note: we use d_inode(lower_dentry), because lower_inode may be
	 * unlinked (no inode->i_sb and i_ino==0.  This happens if someone
	 * tries to open(), unlink(), then ftruncate() a file.
	 */
	inode_lock(d_inode(lower_dentry));
	/* note: lower_ia */
	err = notify_change(lower_dentry, &lower_ia, NULL);
	inode_unlock(d_inode(lower_dentry));
	if (err) {
		goto out;
	}

	/* get attributes from the lower inode */
	fsstack_copy_attr_all(inode, lower_inode);
	/*
	 * Not running fsstack_copy_inode_size(inode, lower_inode), because
	 * VFS should update our inode size, and notify_change on
	 * lower_inode should update its size.
	 */

out:
	loopfs_put_lower_path(dentry, &lower_path);
out_err:
	return err;
}

static int loopfs_getattr(const struct path *path, struct kstat *stat,
				u32 request_mask, unsigned int flags)
{
	int err;
	struct dentry *dentry = path->dentry;
	struct kstat lower_stat;
	struct path lower_path;

	LDBG("loopfs_getattr\n");

	loopfs_get_lower_path(dentry, &lower_path);
	err = vfs_getattr(&lower_path, &lower_stat, request_mask, flags);
	if (err) {
		goto out;
	}
	fsstack_copy_attr_all(d_inode(dentry), d_inode(lower_path.dentry));
	generic_fillattr(d_inode(dentry), stat);
	stat->blocks = lower_stat.blocks;
out:
	loopfs_put_lower_path(dentry, &lower_path);
	return err;
}

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


const struct inode_operations loopfs_symlink_iops = {
	.permission	= loopfs_permission,
	.setattr	= loopfs_setattr,
	.getattr	= loopfs_getattr,
	.get_link	= loopfs_get_link,
	// .listxattr	= loopfs_listxattr,
};

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
	.permission	= loopfs_permission,
	.setattr	= loopfs_setattr,
	.getattr	= loopfs_getattr,
	// .listxattr	= loopfs_listxattr,
};

const struct inode_operations loopfs_main_iops = {
	.permission	= loopfs_permission,
	.setattr	= loopfs_setattr,
	.getattr	= loopfs_getattr,
	// .listxattr	= loopfs_listxattr,
};
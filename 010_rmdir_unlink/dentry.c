#include <linux/slab.h>
#include <linux/namei.h>
#include <linux/mount.h>
#include <linux/fs_stack.h>

#include "loopfs.h"
#include "loopfs_util.h"



/* The dentry cache is just so we have properly sized dentries */
static struct kmem_cache *loopfs_dentry_cachep;

int loopfs_init_dentry_cache(void)
{
	loopfs_dentry_cachep =
		kmem_cache_create("loopfs_dentry",
				  sizeof(struct loopfs_dentry_info),
				  0, SLAB_RECLAIM_ACCOUNT, NULL);

	return loopfs_dentry_cachep ? 0 : -ENOMEM;
}

void loopfs_destroy_dentry_cache(void)
{
	if (loopfs_dentry_cachep) {
		kmem_cache_destroy(loopfs_dentry_cachep);
	}
}

/* allocate new dentry private data */
int new_dentry_private_data(struct dentry *dentry)
{
	struct loopfs_dentry_info *info = LOOPFS_D(dentry);

	LDBG("new_dentry_private_data!\n");

	/* use zalloc to init dentry_info.lower_path */
	info = kmem_cache_zalloc(loopfs_dentry_cachep, GFP_ATOMIC);
	if (!info) {
		return -ENOMEM;
	}

	spin_lock_init(&info->lock);
	dentry->d_fsdata = info;

	return 0;
}

void free_dentry_private_data(struct dentry *dentry)
{
	LDBG("__loopfs_interpose!\n");

	if (!dentry || !dentry->d_fsdata) {
		return;
	}

	kmem_cache_free(loopfs_dentry_cachep, dentry->d_fsdata);
	dentry->d_fsdata = NULL;
}


/*
 * Helper interpose routine, called directly by ->lookup to handle
 * spliced dentries.
 */
static struct dentry *__loopfs_interpose(struct dentry *dentry,
					 struct super_block *sb,
					 struct path *lower_path)
{
	struct inode *inode;
	struct inode *lower_inode;
	struct super_block *lower_sb;
	struct dentry *ret_dentry;

	LDBG("__loopfs_interpose!\n");

	lower_inode = d_inode(lower_path->dentry);
	lower_sb = loopfs_lower_super(sb);

	/* check that the lower file system didn't cross a mount point */
	if (lower_inode->i_sb != lower_sb) {
		ret_dentry = ERR_PTR(-EXDEV);
		goto out;
	}

	/*
	 * We allocate our new inode below by calling loopfs_iget,
	 * which will initialize some of the new inode's fields
	 */

	/* inherit lower inode number for loopfs's inode */
	inode = loopfs_iget(sb, lower_inode);
	if (IS_ERR(inode)) {
		ret_dentry = ERR_PTR(PTR_ERR(inode));
		goto out;
	}

	ret_dentry = d_splice_alias(inode, dentry);

out:
	return ret_dentry;
}


/*
 * Connect a loopfs inode dentry/inode with several lower ones.  This is
 * the classic stackable file system "vnode interposition" action.
 *
 * @dentry: loopfs's dentry which interposes on lower one
 * @sb: loopfs's super_block
 * @lower_path: the lower path (caller does path_get/put)
 */
int loopfs_interpose(struct dentry *dentry, struct super_block *sb,
		     struct path *lower_path)
{
	struct dentry *ret_dentry;

	LDBG("loopfs_interpose!\n");

	ret_dentry = __loopfs_interpose(dentry, sb, lower_path);
	return PTR_ERR(ret_dentry);
}

/*
 * Main driver function for loopfs's lookup.
 *
 * Returns: NULL (ok), ERR_PTR if an error occurred.
 * Fills in lower_parent_path with <dentry,mnt> on success.
 */
static struct dentry *__loopfs_lookup(struct dentry *dentry,
				      unsigned int flags,
				      struct path *lower_parent_path)
{
	int err = 0;
	struct vfsmount *lower_dir_mnt;
	struct dentry *lower_dir_dentry = NULL;
	struct dentry *lower_dentry;
	const char *name;
	struct path lower_path;
	struct qstr this;
	struct dentry *ret_dentry = NULL;

	LDBG("__loopfs_lookup!\n");

	/* must initialize dentry operations */
	d_set_d_op(dentry, &loopfs_dops);

	if (IS_ROOT(dentry)) {
		goto out;
	}

	name = dentry->d_name.name;

	/* now start the actual lookup procedure */
	lower_dir_dentry = lower_parent_path->dentry;
	lower_dir_mnt = lower_parent_path->mnt;

	/* Use vfs_path_lookup to check if the dentry exists or not */
	err = vfs_path_lookup(lower_dir_dentry, lower_dir_mnt, name, 0,
					&lower_path);

	/* no error: handle positive dentries */
	if (!err) {
		loopfs_set_lower_path(dentry, &lower_path);
		ret_dentry = __loopfs_interpose(dentry, dentry->d_sb, &lower_path);
		if (IS_ERR(ret_dentry)) {
			err = PTR_ERR(ret_dentry);
			 /* path_put underlying path on error */
			loopfs_put_reset_lower_path(dentry);
		}
		goto out;
	}

	/*
	 * We don't consider ENOENT an error, and we want to return a
	 * negative dentry.
	 */
	if (err && err != -ENOENT) {
		goto out;
	}

	this.name = name;
	this.len = strlen(name);
	this.hash = full_name_hash(lower_dir_dentry, this.name, this.len);
	/* instatiate a new negative dentry */
	lower_dentry = d_lookup(lower_dir_dentry, &this);
	if (lower_dentry) {
		goto setup_lower;
	}

	lower_dentry = d_alloc(lower_dir_dentry, &this);
	if (!lower_dentry) {
		err = -ENOMEM;
		goto out;
	}
	d_add(lower_dentry, NULL); /* instantiate and hash */

setup_lower:
	lower_path.dentry = lower_dentry;
	lower_path.mnt = mntget(lower_dir_mnt);
	loopfs_set_lower_path(dentry, &lower_path);

	/*
	 * If the intent is to create a file, then don't return an error, so
	 * the VFS will continue the process of making this negative dentry
	 * into a positive one.
	 */
	if (err == -ENOENT || (flags & (LOOKUP_CREATE|LOOKUP_RENAME_TARGET))) {
		err = 0;
	}

out:
	if (err) {
		return ERR_PTR(err);
	}

	return ret_dentry;
}

struct dentry *loopfs_lookup(struct inode *dir, struct dentry *dentry,
			     unsigned int flags)
{
	int err;
	struct dentry *ret, *parent;
	struct path lower_parent_path;

	LDBG("loopfs_lookup!\n");
	
	parent = dget_parent(dentry);

	loopfs_get_lower_path(parent, &lower_parent_path);

	/* allocate dentry private data.  We free it in ->d_release */
	err = new_dentry_private_data(dentry);
	if (err) {
		ret = ERR_PTR(err);
		goto out;
	}
	ret = __loopfs_lookup(dentry, flags, &lower_parent_path);
	if (IS_ERR(ret)) {
		goto out;
	}
	if (ret) {
		dentry = ret;
	}
	if (d_inode(dentry)) {
		fsstack_copy_attr_times(d_inode(dentry), loopfs_lower_inode(d_inode(dentry)));
	}
	/* update parent directory's atime */
	fsstack_copy_attr_atime(d_inode(parent), loopfs_lower_inode(d_inode(parent)));

out:
	loopfs_put_lower_path(parent, &lower_parent_path);
	dput(parent);
	return ret;
}


/*
 * returns: -ERRNO if error (returned to user)
 *          0: tell VFS to invalidate dentry
 *          1: dentry is valid
 */
static int loopfs_d_revalidate(struct dentry *dentry, unsigned int flags)
{
	LDBG("loopfs_d_revalidate\n");

	return 0;
}

static void loopfs_d_release(struct dentry *dentry)
{
	LDBG("loopfs_d_release\n");

	// /* release and reset the lower paths */
	loopfs_put_reset_lower_path(dentry);
	free_dentry_private_data(dentry);

	return;
}

const struct dentry_operations loopfs_dops = {
	.d_revalidate	= loopfs_d_revalidate,
	.d_release	= loopfs_d_release,
};

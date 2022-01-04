#include "loopfs.h"
#include "loopfs_util.h"

/*
 * returns: -ERRNO if error (returned to user)
 *          0: tell VFS to invalidate dentry
 *          1: dentry is valid
 */
static int loopfs_d_revalidate(struct dentry *dentry, unsigned int flags)
{
	LDBG("loopfs_d_revalidate\n");

	return 0;

// 	struct path lower_path;
// 	struct dentry *lower_dentry;
// 	int err = 1;

// 	if (flags & LOOKUP_RCU) {
// 		return -ECHILD;
// 	}

// 	loopfs_get_lower_path(dentry, &lower_path);
// 	lower_dentry = lower_path.dentry;
// 	if (!(lower_dentry->d_flags & DCACHE_OP_REVALIDATE)) {
// 		goto out;
// 	}

// 	err = lower_dentry->d_op->d_revalidate(lower_dentry, flags);

// out:
// 	loopfs_put_lower_path(dentry, &lower_path);
	// return err;
}

static void loopfs_d_release(struct dentry *dentry)
{
	LDBG("loopfs_d_release\n");

	// /* release and reset the lower paths */
	// loopfs_put_reset_lower_path(dentry);
	// free_dentry_private_data(dentry);

	return;
}

const struct dentry_operations loopfs_dops = {
	.d_revalidate	= loopfs_d_revalidate,
	.d_release	= loopfs_d_release,
};

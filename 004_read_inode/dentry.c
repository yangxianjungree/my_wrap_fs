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
}

static void loopfs_d_release(struct dentry *dentry)
{
	LDBG("loopfs_d_release\n");

	return;
}

const struct dentry_operations loopfs_dops = {
	.d_revalidate	= loopfs_d_revalidate,
	.d_release	= loopfs_d_release,
};

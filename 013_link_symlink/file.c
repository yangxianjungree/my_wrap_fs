#include <linux/fs_stack.h>
#include <linux/slab.h>
#include <linux/file.h>

#include "loopfs.h"
#include "loopfs_util.h"


static int loopfs_readdir(struct file *file, struct dir_context *ctx)
{
	int err;
	struct file *lower_file = NULL;
	struct dentry *dentry = file->f_path.dentry;

	LDBG("loopfs_readdir\n");

	lower_file = loopfs_lower_file(file);
	err = iterate_dir(lower_file, ctx);
	file->f_pos = lower_file->f_pos;
	/* copy the atime */
	if (err >= 0) {
		fsstack_copy_attr_atime(d_inode(dentry), file_inode(lower_file));
	}

	return err;
}

static int loopfs_open(struct inode *inode, struct file *file)
{
	int err = 0;
	struct file *lower_file = NULL;
	struct path lower_path;

	LDBG("loopfs_open\n");

	/* don't open unhashed/deleted files */
	if (d_unhashed(file->f_path.dentry)) {
		err = -ENOENT;
		goto out_err;
	}

	file->private_data = kzalloc(sizeof(struct loopfs_file_info), GFP_KERNEL);
	if (!LOOPFS_F(file)) {
		err = -ENOMEM;
		goto out_err;
	}

	/* open lower object and link loopfs's file struct to lower's */
	loopfs_get_lower_path(file->f_path.dentry, &lower_path);
	lower_file = dentry_open(&lower_path, file->f_flags, current_cred());
	path_put(&lower_path);
	if (IS_ERR(lower_file)) {
		err = PTR_ERR(lower_file);
		lower_file = loopfs_lower_file(file);
		if (lower_file) {
			loopfs_set_lower_file(file, NULL);
			fput(lower_file); /* fput calls dput for lower_dentry */
		}
	} else {
		loopfs_set_lower_file(file, lower_file);
	}

	if (err) {
		kfree(LOOPFS_F(file));
	} else {
		fsstack_copy_attr_all(inode, loopfs_lower_inode(inode));
	}

out_err:
	return err;
}


/* trimmed directory options */
const struct file_operations loopfs_dir_fops = {
	.iterate	= loopfs_readdir,
	.open		= loopfs_open,
};

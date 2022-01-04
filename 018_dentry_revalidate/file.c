#include <linux/fs_stack.h>
#include <linux/slab.h>
#include <linux/file.h>

#include "loopfs.h"
#include "loopfs_util.h"


static ssize_t loopfs_read(struct file *file, char __user *buf,
				size_t count, loff_t *ppos)
{
	int err;
	struct file *lower_file;
	struct dentry *dentry = file->f_path.dentry;
	
	LDBG("loopfs_read\n");

	lower_file = loopfs_lower_file(file);
	err = vfs_read(lower_file, buf, count, ppos);
	/* update our inode atime upon a successful lower read */
	if (err >= 0) {
		fsstack_copy_attr_atime(d_inode(dentry), file_inode(lower_file));
	}

	return err;
}

static ssize_t loopfs_write(struct file *file, const char __user *buf,
				size_t count, loff_t *ppos)
{
	int err;

	struct file *lower_file;
	struct dentry *dentry = file->f_path.dentry;
	
	LDBG("loopfs_write\n");

	lower_file = loopfs_lower_file(file);
	err = vfs_write(lower_file, buf, count, ppos);
	/* update our inode times+sizes upon a successful lower write */
	if (err >= 0) {
		fsstack_copy_inode_size(d_inode(dentry), file_inode(lower_file));
		fsstack_copy_attr_times(d_inode(dentry), file_inode(lower_file));
	}

	return err;
}

static int loopfs_readdir(struct file *file, struct dir_context *ctx)
{
	int err;
	struct file *lower_file = NULL;
	struct dentry *dentry = file->f_path.dentry;
	
	LDBG("loopfs_readdir\n");

	lower_file = loopfs_lower_file(file);
	err = iterate_dir(lower_file, ctx);
	file->f_pos = lower_file->f_pos;
	if (err >= 0) {
		/* copy the atime */
		fsstack_copy_attr_atime(d_inode(dentry), file_inode(lower_file));
	}
	return err;
}

static long loopfs_unlocked_ioctl(struct file *file, unsigned int cmd,
					unsigned long arg)
{
	long err = -ENOTTY;
	struct file *lower_file;
	
	LDBG("loopfs_unlocked_ioctl\n");

	lower_file = loopfs_lower_file(file);

	/* XXX: use vfs_ioctl if/when VFS exports it */
	if (!lower_file || !lower_file->f_op) {
		goto out;
	}
	if (lower_file->f_op->unlocked_ioctl) {
		err = lower_file->f_op->unlocked_ioctl(lower_file, cmd, arg);
	}

	/* some ioctls can change inode attributes (EXT2_IOC_SETFLAGS) */
	if (!err) {
		fsstack_copy_attr_all(file_inode(file), file_inode(lower_file));
	}
out:
	return err;
}

#ifdef CONFIG_COMPAT
static long loopfs_compat_ioctl(struct file *file, unsigned int cmd,
				unsigned long arg)
{
	long err = -ENOTTY;
	struct file *lower_file;
	
	LDBG("loopfs_compat_ioctl\n");

	lower_file = loopfs_lower_file(file);

	/* XXX: use vfs_ioctl if/when VFS exports it */
	if (!lower_file || !lower_file->f_op) {
		goto out;
	}
	if (lower_file->f_op->compat_ioctl) {
		err = lower_file->f_op->compat_ioctl(lower_file, cmd, arg);
	}

out:
	return err;
}
#endif

static int loopfs_mmap(struct file *file, struct vm_area_struct *vma)
{
	int err = 0;
	bool willwrite;
	struct file *lower_file;
	const struct vm_operations_struct *saved_vm_ops = NULL;
	
	LDBG("loopfs_mmap\n");

	/* this might be deferred to mmap's writepage */
	willwrite = ((vma->vm_flags | VM_SHARED | VM_WRITE) == vma->vm_flags);

	/*
	 * File systems which do not implement ->writepage may use
	 * generic_file_readonly_mmap as their ->mmap op.  If you call
	 * generic_file_readonly_mmap with VM_WRITE, you'd get an -EINVAL.
	 * But we cannot call the lower ->mmap op, so we can't tell that
	 * writeable mappings won't work.  Therefore, our only choice is to
	 * check if the lower file system supports the ->writepage, and if
	 * not, return EINVAL (the same error that
	 * generic_file_readonly_mmap returns in that case).
	 */
	lower_file = loopfs_lower_file(file);
	if (willwrite && !lower_file->f_mapping->a_ops->writepage) {
		err = -EINVAL;
		printk(KERN_ERR "loopfs: lower file system does not "
				"support writeable mmap\n");
		goto out;
	}

	/*
	 * find and save lower vm_ops.
	 *
	 * XXX: the VFS should have a cleaner way of finding the lower vm_ops
	 */
	if (!LOOPFS_F(file)->lower_vm_ops) {
		err = lower_file->f_op->mmap(lower_file, vma);
		if (err) {
			printk(KERN_ERR "loopfs: lower mmap failed %d\n", err);
			goto out;
		}
		saved_vm_ops = vma->vm_ops; /* save: came from lower ->mmap */
	}

	/*
	 * Next 3 lines are all I need from generic_file_mmap.  I definitely
	 * don't want its test for ->readpage which returns -ENOEXEC.
	 */
	file_accessed(file);
	vma->vm_ops = &loopfs_vm_ops;

	file->f_mapping->a_ops = &loopfs_aops; /* set our aops */
	if (!LOOPFS_F(file)->lower_vm_ops) {
		/* save for our ->fault */
		LOOPFS_F(file)->lower_vm_ops = saved_vm_ops;
	}

out:
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

static int loopfs_flush(struct file *file, fl_owner_t id)
{
	int err = 0;
	struct file *lower_file = NULL;
	
	LDBG("loopfs_flush\n");

	lower_file = loopfs_lower_file(file);
	if (lower_file && lower_file->f_op && lower_file->f_op->flush) {
		filemap_write_and_wait(file->f_mapping);
		err = lower_file->f_op->flush(lower_file, id);
	}

	return err;
}

/* release all lower object references & free the file info structure */
static int loopfs_file_release(struct inode *inode, struct file *file)
{
	struct file *lower_file;
	
	LDBG("loopfs_file_release\n");

	lower_file = loopfs_lower_file(file);
	if (lower_file) {
		loopfs_set_lower_file(file, NULL);
		fput(lower_file);
	}

	kfree(LOOPFS_F(file));
	return 0;
}

static int loopfs_fsync(struct file *file, loff_t start, loff_t end,
				int datasync)
{
	int err;
	struct file *lower_file;
	struct path lower_path;
	struct dentry *dentry = file->f_path.dentry;
	
	LDBG("loopfs_fsync\n");

	err = __generic_file_fsync(file, start, end, datasync);
	if (err) {
		goto out;
	}
	lower_file = loopfs_lower_file(file);
	loopfs_get_lower_path(dentry, &lower_path);
	err = vfs_fsync_range(lower_file, start, end, datasync);
	loopfs_put_lower_path(dentry, &lower_path);
out:
	return err;
}

static int loopfs_fasync(int fd, struct file *file, int flag)
{
	int err = 0;
	struct file *lower_file = NULL;
	
	LDBG("loopfs_fasync\n");

	lower_file = loopfs_lower_file(file);
	if (lower_file->f_op && lower_file->f_op->fasync) {
		err = lower_file->f_op->fasync(fd, lower_file, flag);
	}

	return err;
}

/*
 * loopfs cannot use generic_file_llseek as ->llseek, because it would
 * only set the offset of the upper file.  So we have to implement our
 * own method to set both the upper and lower file offsets
 * consistently.
 */
static loff_t loopfs_file_llseek(struct file *file, loff_t offset, int whence)
{
	int err;
	struct file *lower_file;
	
	LDBG("loopfs_file_llseek\n");

	err = generic_file_llseek(file, offset, whence);
	if (err < 0) {
		goto out;
	}

	lower_file = loopfs_lower_file(file);
	err = generic_file_llseek(lower_file, offset, whence);

out:
	return err;
}

/*
 * loopfs read_iter, redirect modified iocb to lower read_iter
 */
ssize_t loopfs_read_iter(struct kiocb *iocb, struct iov_iter *iter)
{
	int err;
	struct file *file = iocb->ki_filp, *lower_file;
	
	LDBG("loopfs_read_iter\n");

	lower_file = loopfs_lower_file(file);
	if (!lower_file->f_op->read_iter) {
		err = -EINVAL;
		goto out;
	}

	get_file(lower_file); /* prevent lower_file from being released */
	iocb->ki_filp = lower_file;
	err = lower_file->f_op->read_iter(iocb, iter);
	iocb->ki_filp = file;
	fput(lower_file);
	/* update upper inode atime as needed */
	if (err >= 0 || err == -EIOCBQUEUED) {
		fsstack_copy_attr_atime(d_inode(file->f_path.dentry), file_inode(lower_file));
	}
out:
	return err;
}

/*
 * loopfs write_iter, redirect modified iocb to lower write_iter
 */
ssize_t loopfs_write_iter(struct kiocb *iocb, struct iov_iter *iter)
{
	int err;
	struct file *file = iocb->ki_filp, *lower_file;
	
	LDBG("loopfs_write_iter\n");

	lower_file = loopfs_lower_file(file);
	if (!lower_file->f_op->write_iter) {
		err = -EINVAL;
		goto out;
	}

	get_file(lower_file); /* prevent lower_file from being released */
	iocb->ki_filp = lower_file;
	err = lower_file->f_op->write_iter(iocb, iter);
	iocb->ki_filp = file;
	fput(lower_file);
	/* update upper inode times/sizes as needed */
	if (err >= 0 || err == -EIOCBQUEUED) {
		fsstack_copy_inode_size(d_inode(file->f_path.dentry), file_inode(lower_file));
		fsstack_copy_attr_times(d_inode(file->f_path.dentry), file_inode(lower_file));
	}
out:
	return err;
}

const struct file_operations loopfs_main_fops = {
	.llseek		= generic_file_llseek,
	.read		= loopfs_read,
	.write		= loopfs_write,
	.unlocked_ioctl	= loopfs_unlocked_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl	= loopfs_compat_ioctl,
#endif
	.mmap		= loopfs_mmap,
	.open		= loopfs_open,
	.flush		= loopfs_flush,
	.release	= loopfs_file_release,
	.fsync		= loopfs_fsync,
	.fasync		= loopfs_fasync,
	.read_iter	= loopfs_read_iter,
	.write_iter	= loopfs_write_iter,
};

/* trimmed directory options */
const struct file_operations loopfs_dir_fops = {
	.llseek		= loopfs_file_llseek,
	.read		= generic_read_dir,
	.iterate	= loopfs_readdir,
	.unlocked_ioctl	= loopfs_unlocked_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl	= loopfs_compat_ioctl,
#endif
	.open		= loopfs_open,
	.release	= loopfs_file_release,
	.flush		= loopfs_flush,
	.fsync		= loopfs_fsync,
	.fasync		= loopfs_fasync,
};

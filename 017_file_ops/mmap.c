#include "loopfs.h"
#include "loopfs_util.h"



static vm_fault_t loopfs_fault(struct vm_fault *vmf)
{
	vm_fault_t err;
	struct vm_area_struct *vma = vmf->vma;
	struct file *file, *lower_file;
	const struct vm_operations_struct *lower_vm_ops;
	struct vm_area_struct lower_vma;

	LDBG("loopfs_fault\n");

	memcpy(&lower_vma, vma, sizeof(struct vm_area_struct));
	file = lower_vma.vm_file;
	lower_vm_ops = LOOPFS_F(file)->lower_vm_ops;
	BUG_ON(!lower_vm_ops);

	lower_file = loopfs_lower_file(file);
	/*
	 * XXX: vm_ops->fault may be called in parallel.  Because we have to
	 * resort to temporarily changing the vma->vm_file to point to the
	 * lower file, a concurrent invocation of loopfs_fault could see a
	 * different value.  In this workaround, we keep a different copy of
	 * the vma structure in our stack, so we never expose a different
	 * value of the vma->vm_file called to us, even temporarily.  A
	 * better fix would be to change the calling semantics of ->fault to
	 * take an explicit file pointer.
	 */
	lower_vma.vm_file = lower_file;
	vmf->vma = &lower_vma; /* override vma temporarily */
	err = lower_vm_ops->fault(vmf);
	vmf->vma = vma; /* restore vma*/
	return err;
}

static vm_fault_t loopfs_page_mkwrite(struct vm_fault *vmf)
{
	vm_fault_t err = 0;
	struct vm_area_struct *vma = vmf->vma;
	struct file *file, *lower_file;
	const struct vm_operations_struct *lower_vm_ops;
	struct vm_area_struct lower_vma;

	LDBG("loopfs_page_mkwrite\n");

	memcpy(&lower_vma, vma, sizeof(struct vm_area_struct));
	file = lower_vma.vm_file;
	lower_vm_ops = LOOPFS_F(file)->lower_vm_ops;
	BUG_ON(!lower_vm_ops);
	if (!lower_vm_ops->page_mkwrite) {
		goto out;
	}

	lower_file = loopfs_lower_file(file);
	/*
	 * XXX: vm_ops->page_mkwrite may be called in parallel.
	 * Because we have to resort to temporarily changing the
	 * vma->vm_file to point to the lower file, a concurrent
	 * invocation of loopfs_page_mkwrite could see a different
	 * value.  In this workaround, we keep a different copy of the
	 * vma structure in our stack, so we never expose a different
	 * value of the vma->vm_file called to us, even temporarily.
	 * A better fix would be to change the calling semantics of
	 * ->page_mkwrite to take an explicit file pointer.
	 */
	lower_vma.vm_file = lower_file;
	vmf->vma = &lower_vma; /* override vma temporarily */
	err = lower_vm_ops->page_mkwrite(vmf);
	vmf->vma = vma; /* restore vma */
out:
	return err;
}

static ssize_t loopfs_direct_IO(struct kiocb *iocb, struct iov_iter *iter)
{
	LDBG("loopfs_page_mkwrite\n");
	/*
	 * This function should never be called directly.  We need it
	 * to exist, to get past a check in open_check_o_direct(),
	 * which is called from do_last().
	 */
	return -EINVAL;
}

const struct address_space_operations loopfs_aops = {
	.direct_IO = loopfs_direct_IO,
};

const struct vm_operations_struct loopfs_vm_ops = {
	.fault			= loopfs_fault,
	.page_mkwrite	= loopfs_page_mkwrite,
};

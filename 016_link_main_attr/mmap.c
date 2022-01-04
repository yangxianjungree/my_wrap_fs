#include "loopfs.h"
#include "loopfs_util.h"



static vm_fault_t loopfs_fault(struct vm_fault *vmf)
{
	LDBG("loopfs_fault\n");

	return 0;
}

static vm_fault_t loopfs_page_mkwrite(struct vm_fault *vmf)
{
	LDBG("loopfs_page_mkwrite\n");

	return 0;
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

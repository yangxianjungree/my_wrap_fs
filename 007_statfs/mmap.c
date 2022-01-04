#include "loopfs.h"
#include "loopfs_util.h"



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

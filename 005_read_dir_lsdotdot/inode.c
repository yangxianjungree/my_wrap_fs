#include "loopfs.h"
#include "loopfs_util.h"



const struct inode_operations loopfs_dir_iops = {
	.lookup		= loopfs_lookup,
};
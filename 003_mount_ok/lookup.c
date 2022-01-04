/********************************************************************************
File			: lookup.c
Description		: Defines for my loop filesystem look up

********************************************************************************/
#include <linux/fs_stack.h>
#include <linux/fs.h>

#include "loopfs.h"
#include "loopfs_util.h"


static int loopfs_inode_test(struct inode *inode, void *candidate_lower_inode)
{
	struct inode *current_lower_inode = loopfs_lower_inode(inode);
	if (current_lower_inode == (struct inode *)candidate_lower_inode) {
		return 1; /* found a match */
	} else {
		return 0; /* no match */
	}
}

static int loopfs_inode_set(struct inode *inode, void *lower_inode)
{
	/* we do actual inode initialization in loopfs_iget */
	return 0;
}

struct inode *loopfs_iget(struct super_block *sb, struct inode *lower_inode)
{
	struct inode *inode; /* the new inode to return */

	if (!igrab(lower_inode)) {
		return ERR_PTR(-ESTALE);
	}

	inode = iget5_locked(sb, /* our superblock */
			     /*
			      * hashval: we use inode number, but we can
			      * also use "(unsigned long)lower_inode"
			      * instead.
			      */
			     lower_inode->i_ino, /* hashval */
			     loopfs_inode_test,	/* inode comparison function */
			     loopfs_inode_set, /* inode init function */
			     lower_inode); /* data passed to test+set fxns */
	if (!inode) {
		iput(lower_inode);
		return ERR_PTR(-ENOMEM);
	}
	/* if found a cached inode, then just return it (after iput) */
	if (!(inode->i_state & I_NEW)) {
		iput(lower_inode);
		return inode;
	}

	atomic64_inc(&inode->i_version);

	inode->i_atime.tv_sec = 0;
	inode->i_atime.tv_nsec = 0;
	inode->i_mtime.tv_sec = 0;
	inode->i_mtime.tv_nsec = 0;
	inode->i_ctime.tv_sec = 0;
	inode->i_ctime.tv_nsec = 0;

	inode->i_mode = S_IFDIR;

	unlock_new_inode(inode);
	return inode;
}

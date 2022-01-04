set(SRC_011
	loopfs_main.c
	loopfs_util.h
	loopfs.h
	super.c
	dentry.c
	inode.c
	file.c
	mmap.c)

add_executable(exec_011 ${SRC_011})
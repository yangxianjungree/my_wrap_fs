set(SRC_006
	loopfs_main.c
	loopfs_util.h
	loopfs.h
	super.c
	dentry.c
	inode.c
	file.c
	mmap.c)

add_executable(exec_006 ${SRC_006})
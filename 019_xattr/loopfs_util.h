/********************************************************************************
File			: loopfs_util.h
Description		: Defines for my loop filesystem utilities

********************************************************************************/
#ifndef	__LOOP_FS_UTIL_H__
#define	__LOOP_FS_UTIL_H__


#define	LOOPFS_DEBUG

#ifdef	LOOPFS_DEBUG
	#define	LDBG(msg, args...) do {									\
		printk(KERN_INFO "<LOOPFS> " msg, ##args);										\
	} while(0)
#else
	#define	LDBG(msg, args...) do { } while(0)
#endif

#define	LERR(msg, args...) do {										\
	printk(KERN_ERR "<LOOPFS> " msg, ##args);											\
} while(0)


#endif	// __LOOP_FS_UTIL_H__

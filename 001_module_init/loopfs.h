/********************************************************************************
File			: loopfs.h
Description		: Defines for my loop file system

********************************************************************************/
#ifndef	__LOOP_FS_H__
#define	__LOOP_FS_H__


#define	LOOPFS_DEBUG

#ifdef	LOOPFS_DEBUG
	#define	LDBG(msg, args...) do {																		\
		printk(KERN_INFO "<LOOPFS DBG> " msg "	[%s:%s:%d]\n", ##args, __FILE__, __func__, __LINE__);	\
	} while(0)
#else
	#define	LDBG(msg, args...) do { } while(0)
#endif


#define	LERR(msg, args...) do {																		\
	printk(KERN_ERR "<LOOPFS ERR> " msg "	[%s:%s:%d]\n", ##args, __FILE__, __func__, __LINE__);	\
} while(0)


#endif	// __LOOP_FS_H__

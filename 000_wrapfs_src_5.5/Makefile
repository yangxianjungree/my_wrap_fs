LOOPFS_SOURCE = main.c super.c lookup.c dentry.c inode.c file.c mmap.c

obj-m += wrapfs.o
wrapfs-objs := $(LOOPFS_SOURCE:.c=.o)

all:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) modules

clean:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) clean

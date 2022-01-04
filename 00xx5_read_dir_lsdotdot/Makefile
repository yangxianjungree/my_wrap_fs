LOOPFS_SOURCE = loopfs_main.c super.c lookup.c dentry.c inode.c file.c

obj-m += loopfs.o
loopfs-objs := $(LOOPFS_SOURCE:.c=.o)

all:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) modules

clean:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) clean

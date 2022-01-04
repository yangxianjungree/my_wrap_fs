#include <sys/mount.h>
#include <stdio.h>

int main()
{
	if (mount("/xxx", "/mnt/xxx", "xfs", 0, NULL)) {
		perror("mount failed.");
	}

	return 0;
}
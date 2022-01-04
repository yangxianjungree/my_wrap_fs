#include "loopfs.h"
#include "loopfs_util.h"


static int loopfs_readdir(struct file *file, struct dir_context *ctx)
{
	LDBG("loopfs_readdir\n");

	if (ctx->pos == 0) {
		LDBG("Read Dir dot !\n");
		if (!dir_emit_dot(file, ctx)) {
			LERR("Cannot emit \".\" directory entry\n");
			return (-1);
		}
		ctx->pos = 1;
	}

	if (ctx->pos == 1) {
		LDBG("Read Dir dot dot !\n");
		if (!dir_emit_dotdot(file, ctx)) {
			LERR("Cannot emit \"..\" directory entry\n");
			return (-1);
		}

		ctx->pos = 2;
	}

	return (0);
}


/* trimmed directory options */
const struct file_operations loopfs_dir_fops = {
	.iterate	= loopfs_readdir,
};

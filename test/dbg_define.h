#ifndef __DBG_DEFINE_H__
#define __DBG_DEFINE_H__

#define xxprint(msg, arg...) do { \
	printf(msg, ##arg);             \
} while(0)

#define xx_print(msg) { \
	xxprint("xxx"msg);                        \
}

#endif//__DBG_DEFINE_H__
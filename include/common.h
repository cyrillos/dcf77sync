#ifndef DCF77_COMMON_H__
#define DCF77_COMMON_H__

#include <stdlib.h>
#include <stdio.h>

#define pr_err(fmt, ...)	fprintf(stderr, fmt, ## __VA_ARGS__)
#define pr_perror(fmt, ...)	fprintf(stderr, fmt ": %m\n", ## __VA_ARGS__)
#define pr_info(fmt, ...)	fprintf(stdout, fmt, ## __VA_ARGS__)

#define ARRAY_SIZE(__a)		(sizeof(__a) / sizeof((__a)[0]))

#define BUILD_BUG_ON(condition)	((void)sizeof(char[1 - 2*!!(condition)]))

#define __stringify_1(x...)	#x
#define __stringify(x...)	__stringify_1(x)

#define BUG_ON(cond)								\
	do {									\
		if (cond) {							\
			pr_err("BUG_ON: %s:%d\n", __FILE__, __LINE__);		\
			abort();						\
		}								\
	} while (0)

#endif /* DCF77_COMMON_H__ */

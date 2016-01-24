/*
 * Common helper definitions
 *
 */

#ifndef _HELPER_H
#define _HELPER_H

/*
 * Declare logfile in your tool
 */
extern FILE *logfile;

/*
 * Overwrite the component in your tool
 */
#ifndef COMP
#define COMP "nlmon"
#endif

#define DEBUG_LOGFILE "/tmp/nlmon-" COMP "-debug.log"

#define DEBUG_ENABLED
#ifdef DEBUG_ENABLED
#define DEBUG(...)							\
	do {								\
		fprintf(logfile, __VA_ARGS__);				\
		fflush(logfile);					\
	} while (0)
#else
#define DEBUG(...)
#endif

#define DIE(...)							\
	do {								\
		fprintf(stderr, COMP ": " __VA_ARGS__);			\
		exit(1);						\
	} while (0)

#define DIE_PERROR(...)							\
	do {								\
		perror(COMP ": " __VA_ARGS__);				\
		exit(1);						\
	} while (0)

#define BUG(x)								\
	if (x) {							\
		fprintf(stderr, COMP ": assert failed at "		\
			__FILE__ ":%d in %s()\n", __LINE__, __func__);	\
		exit(1);						\
	}

#define WARN(...)							\
	do {								\
		fprintf(stderr, COMP ": Warning, " __VA_ARGS__);	\
	} while (0)

#define min(x, y) ((x) < (y) ? (x) : (y))
#define max(x, y) ((x) > (y) ? (x) : (y))

#define ARRAY_SIZE(arr) (sizeof(arr) / sizeof((arr)[0]))

/*
 * Short data types to avoid the uintXX_t mess
 */
typedef unsigned long long      u64;
typedef signed long long        s64;
typedef unsigned int            u32;
typedef signed int              s32;
typedef unsigned short int      u16;
typedef signed short int        s16;
typedef unsigned char           u8;
typedef signed char             s8;

#endif

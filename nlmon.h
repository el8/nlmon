#ifndef _NLMON_H
#define _NLMON_H

#define _GNU_SOURCE             /* See feature_test_macros(7) */
#include <time.h>
#include <pthread.h>
#include <linux/taskstats.h>

#include "atomic.h"
#include "rbtree.h"

#define NSECS_PER_MSEC	(1000000UL)

struct taskstat_delta {
	unsigned long long utime;
	unsigned long long stime;
	unsigned long long cpu_delay;
	unsigned long long rss;
	unsigned long long io_rd_bytes;
	unsigned long long io_wr_bytes;
	unsigned long long blkio_delay;
	char comm[TS_COMM_LEN];
	int pid;
	int tid;
	struct rb_node node;
};

/* sum over all processes utime or stime in the current measurement interval */
int current_sum_utime;
int current_sum_stime;
int current_sum_cpu_utime;
int current_sum_cpu_stime;

struct cpu_usage {
	unsigned int user;	/* gives us 497 days which should be enough. We're not servers. :*/
	unsigned int nice;
	unsigned int system;
	unsigned int idle;
	unsigned int iowait;
	unsigned int irq;
	unsigned int softirq;
	unsigned int steal;
	unsigned int guest;
	unsigned int guest_nice;
	/* current cpu freq */
	unsigned long freq;
};

/* absolute values of last interval */
struct cpu_usage *cpu_hist;

/* calculated delta for current interval */
struct cpu_usage *cpu_delta;

struct output_operations {
	void (*init_output)	(void);
	void (*exit_output)	(void);
	void (*print_sync)	(void);
	void (*print_banner)	(struct taskstats *t);
	void (*print_data)	(struct taskstat_delta *delta);
	void (*print_cpu_info)	(int cpu, struct cpu_usage *delta);
	void (*print_mem_info)	(int total, int free);
	void (*print_cycle_start) (void);
	void (*print_cycle_end)	(struct timespec *ts);
};

enum sort_options {
	OPT_SORT_TID,
	OPT_SORT_NAME,
	OPT_SORT_TIME,
	OPT_SORT_DELAY,
	OPT_SORT_MEM,
	OPT_SORT_IO,
	OPT_SORT_IODELAY,
};

#define PID_MAX 32768	/* /proc/sys/kernel/pid_max */

/* detected once, no CPU hotplug support */
int nr_cpus;

short int ts_version;
int ts_size;
int nr_cycles;

/* for csv headers */
int new_cycle;

struct output_operations *output;

/* default intervall is one second */
extern struct timespec target;

extern pthread_t procfs_thread;
extern atomic_t nr_threads;

/* prototypes */
void query_cpus(void);
void print_cpus(void);
int get_nr_cpus(void);
void data_init_cpu(void);
void query_memory(void);
void print_memory(void);
void *proc_events_main(void *unused);
void cache_init(void);
int cache_add(struct taskstat_delta *delta);
struct taskstat_delta *cache_walk(struct taskstat_delta *last);
void cache_flush(void);

#endif

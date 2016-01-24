/*
 * Copyright Penguin Boost, 2016
 * Author(s): Jan Glauber <jan.glauber@gmail.com>
 *
 * CSV output.
 */
#include <stdio.h>

#define COMP "nlmon"
#include "helper.h"
#include "nlmon.h"

static void print_banner_csv(struct taskstats *t)
{
	printf("HEADER;TSVersion;TSSize\n");
	printf("BANNER;%d;%zu\n", t->version, sizeof(*t));
}

static void print_cycle_start_csv(void)
{
	printf("HEADER;Cycle;Threads\n");
	printf("MEASUREMENT;%d;%u\n", nr_cycles, atomic_read(&nr_threads));
}

static void print_cycle_end_csv(struct timespec *ts)
{
	printf("HEADER;Cycle_used_sec;Cycle_used_ms\n");
	printf("MEASUREMENT;%u;%lu\n",	(int) ts->tv_sec, ts->tv_nsec / NSECS_PER_MSEC);
	fflush(stdout);
}

static void print_data_csv(struct taskstat_delta *delta)
{
	float total = (delta->utime + delta->stime) / NSECS_PER_MSEC;

	/* header */
	if (new_cycle) {
		printf("HEADER;PID;TID;Name;UserT[ms];SysT[ms];TotalT[sec];Rss[MB];IORead[Bytes];IOWrite[Bytes];IODelay[ms];Iteration\n");
		new_cycle = 0;
	}
	printf("THREAD;%d;%d;%s;%llu;%llu;%f;%llu;%llu;%llu;%llu;%d\n",
		delta->pid,
		delta->tid,
		delta->comm,
		delta->utime / 1000,
		delta->stime / 1000,
		total,
		(delta->utime + delta->stime) ? delta->rss / (delta->utime + delta->stime) : 0,
		delta->io_rd_bytes,
		delta->io_wr_bytes,
		delta->blkio_delay / NSECS_PER_MSEC,
		nr_cycles
		);
}

static void print_cpu_info_csv(int i, struct cpu_usage *delta)
{
	// TODO: only print once for all cpus
	printf("HEADER;CPU;USER;SYSTEM;IRQ;SOFTIRQ;IOWAIT;IDLE\n");

	printf("CPU%d;%u;%u;%u;%u;%u;%u\n",
		i,
		delta->user,
		delta->system,
		delta->irq,
		delta->softirq,
		delta->iowait,
		delta->idle
		);
}

static void print_mem_info_csv(int total, int free)
{
	printf("HEADER;MEM_TOTAL;MEM_USED;MEM_FREE\n");
	printf("%9u;%9u;%9u\n", total, total - free, free);
}

static void print_sync(void) { }
static void init_output(void) { }
static void exit_output(void) { }

struct output_operations oops_csv = {
	.init_output =		init_output,
	.exit_output =		exit_output,
	.print_sync =		print_sync,
	.print_banner =		print_banner_csv,
	.print_data =		print_data_csv,
	.print_cpu_info =	print_cpu_info_csv,
	.print_mem_info =	print_mem_info_csv,
	.print_cycle_start =	print_cycle_start_csv,
	.print_cycle_end =	print_cycle_end_csv,
};

/*
 * Copyright Penguin Boost, 2016
 * Author(s): Jan Glauber <jan.glauber@gmail.com>
 *
 * Console output.
 */
#include <stdio.h>

#define COMP "nlmon"
#include "helper.h"
#include "nlmon.h"

#define average_ms(t, c) (t / 1000000ULL / (c ? c : 1))

static void print_banner_stdout(struct taskstats *t)
{
	printf("\nTaskstats version: %d  Taskstat size: %zu\n", t->version, sizeof(*t));
	printf("\n");
}

static void print_sync_stdout(void)
{
	printf("... Syncing ...\n");
}

static void print_cycle_start_stdout(void)
{
	printf("measurement cycle: %d  threads: %u\n", nr_cycles, atomic_read(&nr_threads));
}

static void print_cycle_end_stdout(struct timespec *ts)
{
	printf("ERROR [ms]: user: %4u  system: %4u  total: %4u\n",
		current_sum_utime, current_sum_stime, current_sum_utime + current_sum_stime);
	printf("... took: %us %lums\n\n",(int) ts->tv_sec, ts->tv_nsec / NSECS_PER_MSEC);
	fflush(stdout);
}

static void print_data_stdout(struct taskstat_delta *delta)
{
	/*
	printf("\n\nCPU   %15s%15s%15s%15s%15s\n"
	       "      %15llu%15llu%15llu%15llu%15.3fms\n"
	       "IO    %15s%15s%15s\n"
	       "      %15llu%15llu%15llums\n"
	       "SWAP  %15s%15s%15s\n"
	       "      %15llu%15llu%15llums\n"
	       "RECLAIM  %12s%15s%15s\n"
	       "      %15llu%15llu%15llums\n",
	       "count", "real total", "virtual total",
	       "delay total", "delay average",
	       (unsigned long long)t->cpu_count,
	       (unsigned long long)t->cpu_run_real_total,
	       (unsigned long long)t->cpu_run_virtual_total,
	       (unsigned long long)t->cpu_delay_total,
	       average_ms((double)t->cpu_delay_total, t->cpu_count),
	       "count", "delay total", "delay average",
	       (unsigned long long)t->blkio_count,
	       (unsigned long long)t->blkio_delay_total,
	       average_ms(t->blkio_delay_total, t->blkio_count),
	       "count", "delay total", "delay average",
	       (unsigned long long)t->swapin_count,
	       (unsigned long long)t->swapin_delay_total,
	       average_ms(t->swapin_delay_total, t->swapin_count),
	       "count", "delay total", "delay average",
	       (unsigned long long)t->freepages_count,
	       (unsigned long long)t->freepages_delay_total,
	       average_ms(t->freepages_delay_total, t->freepages_count));
	*/

	printf("PID: %5d [%16s]  user: %6llu  system: %6llu  rss: %6llu  io_rd: %8llu  io_wr: %8llu  blkio_delay: %9llu\n",
		delta->tid,
		delta->comm,
		delta->utime / 1000,
		delta->stime / 1000,
		(delta->utime + delta->stime) ? delta->rss / (delta->utime + delta->stime) : 0,
		delta->io_rd_bytes,
		delta->io_wr_bytes,
		delta->blkio_delay / NSECS_PER_MSEC
	);
}

static void print_cpu_info_stdout(int i, struct cpu_usage *delta)
{
	printf("CPU%d  [ms]  user: %4u  system: %4u  irq: %4u  softirq: %4u  iowait: %4u  idle: %4u  freq: %8lu\n",
		i,
		delta->user,
		delta->system,
		delta->irq,
		delta->softirq,
		delta->iowait,
		delta->idle,
		delta->freq // XXX broken?
		);
}

static void print_mem_info_stdout(total, free)
{
	printf("MEM   [kB]  ");
	printf("total: %9u  used: %9u  free: %9u\n", total, total - free, free);
}

static void init_output(void) { }
static void exit_output(void) { }

struct output_operations oops_stdout = {
	.init_output =		init_output,
	.exit_output =		exit_output,
	.print_sync =		print_sync_stdout,
	.print_banner =		print_banner_stdout,
	.print_data =		print_data_stdout,
	.print_cpu_info =	print_cpu_info_stdout,
	.print_mem_info =	print_mem_info_stdout,
	.print_cycle_start =	print_cycle_start_stdout,
	.print_cycle_end =	print_cycle_end_stdout,
};

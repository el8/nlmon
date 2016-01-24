/*
 * Copyright Penguin Boost, 2016
 * Author(s): Jan Glauber <jan.glauber@gmail.com>
 *
 * No output. Maybe useful for performance measurements.
 */
#include <stdio.h>

#define COMP "nlmon"
#include "helper.h"
#include "nlmon.h"

static void print_banner_nop(struct taskstats *t)
{
	printf("\nTaskstats version: %d  Taskstat size: %zu\n", t->version, sizeof(*t));
	printf("\n");
}

static void print_sync_nop(void) { }

static void print_cycle_start_nop(void)
{
	printf("measurement cycle: %d  threads: %u\n", nr_cycles, atomic_read(&nr_threads));
}

static void print_cycle_end_nop(struct timespec *ts)
{
	printf("... took: %us %lums\n\n",(int) ts->tv_sec, ts->tv_nsec / NSECS_PER_MSEC);
	fflush(stdout);
}

static void print_data_nop(struct taskstat_delta *delta) { }
static void print_cpu_info_nop(int i, struct cpu_usage *delta) { }
static void print_mem_info_nop(total, free) { }
static void init_output(void) { }
static void exit_output(void) { }

struct output_operations oops_nop = {
	.init_output =		init_output,
	.exit_output =		exit_output,
	.print_sync =		print_sync_nop,
	.print_banner =		print_banner_nop,
	.print_data =		print_data_nop,
	.print_cpu_info =	print_cpu_info_nop,
	.print_mem_info =	print_mem_info_nop,
	.print_cycle_start =	print_cycle_start_nop,
	.print_cycle_end =	print_cycle_end_nop,
};

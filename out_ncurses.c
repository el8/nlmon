/*
 * Copyright Penguin Boost, 2016
 * Author(s): Jan Glauber <jan.glauber@gmail.com>
 *
 * ncurses output.
 */
#include <ncurses.h>

#define COMP "nlmon"
#include "helper.h"
#include "nlmon.h"
#include "math.h"

static WINDOW *bthreads;
static WINDOW *bcpus;
static WINDOW *threads;
static WINDOW *cpus;

static int max_output_lines;
static int used_output_lines;

static void print_sync_ncurses(void)
{
	wclear(threads);
	wclear(cpus);
	mvwprintw(threads, 20, 20, "... Synching ...\n");
}

static void print_cycle_start_ncurses(void)
{
	used_output_lines = max_output_lines;

	wclear(threads);
	wclear(cpus);
	wprintw(threads, "Taskstats version: %d  Taskstat size: %lu  ", ts_version, ts_size);
	wprintw(threads, "Measurement cycle: %d  Interval: %us.%ums  ", nr_cycles, target.tv_sec, target.tv_nsec / NSECS_PER_MSEC);
	wprintw(threads, "Threads: %u\n", atomic_read(&nr_threads));
	wprintw(threads, "\n");
	wprintw(threads,
		"%5s  %16s   %6s    %6s  %9s    %6s    %8s    %8s    %9s\n",
		"TID", "Name", "User[ms]", "System[ms]", "CpuDelay[ms]", "Rss[MB]", "IORead[Bytes]", "IOWrite[Bytes]", "IODelay[ms]");
}

static void print_cycle_end_ncurses(struct timespec *ts)
{
	unsigned int err_utime, err_stime;
	float total_100p;

	// TODO: full error statistic only in ncurses variant...
	wprintw(cpus, "\n");
	wprintw(cpus, "SUM NETLINK [ms]: user: %4u  system: %4u  total: %4u\n",
		current_sum_utime,
		current_sum_stime,
		current_sum_utime + current_sum_stime);
	wprintw(cpus, "SUM CPUS    [ms]: user: %4u  system: %4u  total: %4u\n",
		current_sum_cpu_utime,
		current_sum_cpu_stime,
		current_sum_cpu_utime + current_sum_cpu_stime);

	err_utime = (current_sum_cpu_utime > current_sum_utime) ? current_sum_cpu_utime - current_sum_utime
		: current_sum_utime - current_sum_cpu_utime;
	err_stime = (current_sum_cpu_stime > current_sum_stime) ? current_sum_cpu_stime - current_sum_stime
		: current_sum_stime - current_sum_cpu_stime;
	total_100p = max(current_sum_utime + current_sum_stime,
			current_sum_cpu_utime + current_sum_cpu_stime);

	wprintw(cpus, "ERROR       [ms]: user: %4u  system: %4u  total: %4u  (%3.1f%%)\n",
			err_utime,
			err_stime,
			err_utime + err_stime,
			(100 * (err_utime + err_stime)) / total_100p
			);
	wprintw(cpus, "\t\t\t\t... took: %us %lums\n\n",(int) ts->tv_sec, ts->tv_nsec / NSECS_PER_MSEC);

	/* now refresh all windows */
	wrefresh(threads);
	wrefresh(cpus);
}

static void print_data_ncurses(struct taskstat_delta *delta)
{
	if (used_output_lines <= 0)
		return;

	wprintw(threads, "%5d  %16s  %6llu        %6llu     %9llu      %6llu      %8llu          %8llu         %9llu\n",
		delta->tid,
		delta->comm,
		delta->utime / 1000,
		delta->stime / 1000,
		delta->cpu_delay / NSECS_PER_MSEC,
		(delta->utime + delta->stime) ? delta->rss / (delta->utime + delta->stime) : 0,
		delta->io_rd_bytes,
		delta->io_wr_bytes,
		delta->blkio_delay / NSECS_PER_MSEC
	);

	used_output_lines--;
}

static void print_cpu_info_ncurses(int i, struct cpu_usage *delta)
{
	wprintw(cpus, "CPU");
	if (opt_all_cpus)
		wprintw(cpus, "%d", i);
	else
		wprintw(cpus, " ");
	wprintw(cpus, "  [ms]  user: %4u  system: %4u  irq: %4u  softirq: %4u  iowait: %4u  idle: %4u  freq: %8lu\n",
		delta->user,
		delta->system,
		delta->irq,
		delta->softirq,
		delta->iowait,
		delta->idle,
		delta->freq // XXX broken?
		);

	// XXX make generic
	current_sum_cpu_utime += delta->user;
	current_sum_cpu_stime += delta->system;
}

static void print_mem_info_ncurses(total, free)
{
	wprintw(cpus, "MEM   [kB]  ");
	wprintw(cpus, "total: %9u  used: %9u  free: %9u\n", total, total - free, free);
}

static void init_ncurses(void)
{
	int split_size = nr_cpus + 7;
	int total_x, total_y;

	initscr();
	noecho();
	curs_set(FALSE);

	getmaxyx(stdscr, total_y, total_x);
	max_output_lines = total_y - split_size - 5;

	/* set up border windows */
	bthreads = newwin(total_y - split_size, total_x, 0, 0);
	bcpus = newwin(split_size, total_x, total_y - split_size, 0);
	box(bthreads, 0, 0);
	box(bcpus, 0, 0);
	wrefresh(bthreads);
	wrefresh(bcpus);

	/* set up view windows */
	threads = newwin(total_y - split_size - 2, total_x - 2, 1, 1);
	cpus = newwin(split_size - 2, total_x - 2, total_y - split_size + 1, 1);
	wrefresh(threads);
	wrefresh(cpus);
}

static void exit_ncurses(void)
{
	delwin(threads);
	delwin(cpus);
	endwin();
}

static void print_banner(struct taskstats *t) { }

struct output_operations oops_ncurses = {
	.init_output =		init_ncurses,
	.exit_output =		exit_ncurses,
	.print_sync =		print_sync_ncurses,
	.print_banner =		print_banner,
	.print_data =		print_data_ncurses,
	.print_cpu_info =	print_cpu_info_ncurses,
	.print_mem_info =	print_mem_info_ncurses,
	.print_cycle_start =	print_cycle_start_ncurses,
	.print_cycle_end =	print_cycle_end_ncurses,
};

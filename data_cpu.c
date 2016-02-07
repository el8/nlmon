/*
 * Copyright Penguin Boost, 2016
 * Author(s): Jan Glauber <jan.glauber@gmail.com>
 *
 * CPU data gathering.
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#define COMP "nlmon"
#include "nlmon.h"
#include "helper.h"

static FILE *cpu_fp;

// TODO: make size a function of nr cpus, enough for 4 CPUs now
#define CPU_LINE_SIZE 512
static size_t cpu_line_size = CPU_LINE_SIZE;
static char cpu_buf[CPU_LINE_SIZE];
static char *cpu_buf_ptr = cpu_buf;
static int cpu_line_entries;

/* current interval delta per cpu */
struct cpu_usage *cpu_delta;

static void parse_cpu_info(char *line, int cpu, struct cpu_usage *now)
{
	int i;

	/* cpu0  2255 34 2290 22625563 6290 127 456 0 0 */
	sscanf(line, "cpu%d %u %u %u %u %u %u %u %u %u %u\n",
		&i,
		&now->user,
		&now->nice,
		&now->system,
		&now->idle,
		&now->iowait,
		&now->irq,
		&now->softirq,
		&now->steal,
		&now->guest,
		&now->guest_nice
		);
}

static void parse_cpu_sum_info(char *line, int cpu, struct cpu_usage *now)
{
	/* cpu  2255 34 2290 22625563 6290 127 456 0 0 */
	sscanf(line, "cpu %u %u %u %u %u %u %u %u %u %u\n",
		&now->user,
		&now->nice,
		&now->system,
		&now->idle,
		&now->iowait,
		&now->irq,
		&now->softirq,
		&now->steal,
		&now->guest,
		&now->guest_nice
		);
}

/* get CPU summary values out of proc */
void query_cpu_summary(void)
{
	struct cpu_usage now;
	ssize_t len;
	int cpu = 0;

	cpu_fp = fopen("/proc/stat", "r");
	if (!cpu_fp)
		DIE_PERROR("open procfs failed");

	len = getline(&cpu_buf_ptr, &cpu_line_size, cpu_fp);
	if (len < 0)
		DIE_PERROR("getline failed");

	parse_cpu_sum_info(cpu_buf_ptr, cpu, &now);

	// XXX calc deltas and store in cpu_delta
	cpu_delta[cpu].user = (now.user - cpu_hist[cpu].user) * 10;
	cpu_delta[cpu].system = (now.system - cpu_hist[cpu].system) * 10;
	cpu_delta[cpu].irq = (now.irq - cpu_hist[cpu].irq) * 10;
	cpu_delta[cpu].softirq = (now.softirq - cpu_hist[cpu].softirq) * 10;
	cpu_delta[cpu].iowait = (now.iowait - cpu_hist[cpu].iowait) * 10;
	cpu_delta[cpu].idle = (now.idle - cpu_hist[cpu].idle) * 10;

	/* update cpu values */
	memcpy(&cpu_hist[cpu], &now, sizeof(now));

	fclose(cpu_fp);
}

/* get per CPU values out of proc */
void query_all_cpus(void)
{
	struct cpu_usage now;
	ssize_t len;
	int cpu, line;

	cpu_fp = fopen("/proc/stat", "r");
	if (!cpu_fp)
		DIE_PERROR("open procfs failed");

	/* this is a bit ugly (nr_cpus + 1) because of the summary line... */
	for (line = 0; line < nr_cpus + 1; line++) {
		len = getline(&cpu_buf_ptr, &cpu_line_size, cpu_fp);
		if (len < 0)
			DIE_PERROR("getline failed");

		/* skip summary line */
		if (line == 0)
			continue;

		cpu = line - 1;

		parse_cpu_info(cpu_buf_ptr, cpu, &now);
		//if (nr_cycles)
		//	output->print_cpu_info(i - 1, &now);

		// XXX calc deltas and store in cpu_delta
		cpu_delta[cpu].user = (now.user - cpu_hist[cpu].user) * 10;
		cpu_delta[cpu].system = (now.system - cpu_hist[cpu].system) * 10;
		cpu_delta[cpu].irq = (now.irq - cpu_hist[cpu].irq) * 10;
		cpu_delta[cpu].softirq = (now.softirq - cpu_hist[cpu].softirq) * 10;
		cpu_delta[cpu].iowait = (now.iowait - cpu_hist[cpu].iowait) * 10;
		cpu_delta[cpu].idle = (now.idle - cpu_hist[cpu].idle) * 10;

		/* update cpu values */
		memcpy(&cpu_hist[cpu], &now, sizeof(now));
	}
	fclose(cpu_fp);
}

void query_cpus(int all)
{
	if (all)
		query_all_cpus();
	else
		query_cpu_summary();
}

void print_cpus(int all)
{
	int i;

	if (!nr_cycles)
		return;

	if (all)
		for (i = 0; i < nr_cpus; i++)
			output->print_cpu_info(i, &cpu_delta[i]);
	else
		output->print_cpu_info(0, &cpu_delta[0]);
}

/* get the number of configured CPUs, some may be offline */
int get_nr_cpus(void)
{
	unsigned long cpus = sysconf(_SC_NPROCESSORS_CONF);

	if (cpus < 0)
		DIE_PERROR("sysconf failed");
	return cpus;
}

void data_init_cpu(void)
{
	int i, values = 0;
	char *ptr;

	cpu_fp = fopen("/proc/stat", "r");
	if (!cpu_fp)
		DIE_PERROR("open procfs failed");

	/* detect number of columns in cpu line */
	i = getline(&cpu_buf_ptr, &cpu_line_size, cpu_fp);
	if (i < 0)
		DIE_PERROR("getline failed");
	/* skiped cpu summary line */
	i = getline(&cpu_buf_ptr, &cpu_line_size, cpu_fp);
	if (i < 0)
		DIE_PERROR("getline failed");
	fclose(cpu_fp);

	ptr = cpu_buf_ptr;
	while (*ptr != '\n') {
		if (*ptr == ' ')
			values++;
		ptr++;
	}
	cpu_line_entries = values;
	DEBUG("%d numerical values in cpu lines\n", cpu_line_entries);

	cpu_hist = calloc(nr_cpus, sizeof(struct cpu_usage));
	if (!cpu_hist)
		DIE_PERROR("malloc failed");
	cpu_delta = calloc(nr_cpus, sizeof(struct cpu_usage));
	if (!cpu_delta)
		DIE_PERROR("malloc failed");
}

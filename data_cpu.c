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
static int cpufreq_available;

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

	if (i != cpu)
		DIE("wrong cpu");
}

static unsigned long query_cpu_freq(int cpu)
{
	char name[80];
	char buf[16];
	int fd, rc;

	memset(name, 0, 80);
	snprintf(name, 80, "/sys/devices/system/cpu/cpu%d/cpufreq/cpuinfo_cur_freq", cpu);
	fd = open(name, O_RDONLY);
	if (fd < 0)
		DIE_PERROR("open cpuinfo_cur_freq failed");
	rc = read(fd, buf, 16);
	if (rc < 0)
		DIE_PERROR("read cpuinfo_cur_freq failed");
	close(fd);
	return strtoul(buf, NULL, 10);
}

/* get per CPU values out of proc */
void query_cpus(void)
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
		if (cpufreq_available)
			now.freq = query_cpu_freq(cpu);
		else
			now.freq = 0;

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

void print_cpus(void)
{
	int i;

	if (!nr_cycles)
		return;

	for (i = 0; i < nr_cpus; i++)
		output->print_cpu_info(i, &cpu_delta[i]);
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
	int i, rc, values = 0;
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

	/* check availability of cpu frequency info */
	rc = access("/sys/devices/system/cpu/cpu0/cpufreq/cpuinfo_cur_freq", R_OK);
	if (!rc)
		cpufreq_available = 1;
}

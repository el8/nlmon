/*
 * Copyright Penguin Boost, 2016
 * Author(s): Jan Glauber <jan.glauber@gmail.com>
 *
 * Memory data gathering.
 */
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#define COMP "nlmon"
#include "helper.h"
#include "nlmon.h"

static int mem_total;
static int mem_free;

/* get system memory usage out of proc */
void query_memory(void)
{
	size_t mem_line_size = 256;
	char mem_buf[256];
	char *mem_buf_ptr = mem_buf;
	FILE *mem_fp;
	ssize_t len;
	int i = 0;

	mem_fp = fopen("/proc/meminfo", "r");
	if (!mem_fp)
		DIE_PERROR("open procfs failed");

	do {
		/* only intrested in first two lines:
		 * MemTotal:        4980832 kB
		 * MemFree:         1376304 kB
		 */
		if (i++ > 1)
			break;

		len = getline(&mem_buf_ptr, &mem_line_size, mem_fp);
		if (len < 0)
			DIE_PERROR("getline failed");

		if (i == 1)
			sscanf(mem_buf_ptr, "MemTotal: %u kB\n", &mem_total);
		if (i == 2)
			sscanf(mem_buf_ptr, "MemFree: %u kB\n", &mem_free);

	} while (!feof(mem_fp));

	fclose(mem_fp);
}

void print_memory(void)
{
	if (!nr_cycles)
		return;
	output->print_mem_info(mem_total, mem_free);
}

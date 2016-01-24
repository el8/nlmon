/*
 * Copyright Penguin Boost, 2016
 * Author(s): Jan Glauber <jan.glauber@gmail.com>
 *
 * Simple linear bitmap relying on atomic bit set|clear|test
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>

#include "helper.h"

static char *map;
static int nr_bits;

extern FILE *logfile;

/*
 * ARM has not atomic bitops so we need a lock...
 */
static pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;

/*
 * TODO:
 * - map ptr for multiple maps
 */

/*
 * TODO: check alignment
 */
void bm_alloc(int bits)
{
	int bytes = bits / 8;

	if (!bits || bits < 0 || bits % 8)
		DIE("invalid bit count: %d", bits);

	map = malloc(bytes);
	if (!map)
		DIE_PERROR("allocating bitmap failed");
	memset(map, 0, bytes);

	nr_bits = bits;
	DEBUG("alloc map @ %p  for %d bits\n", map, nr_bits);
}

void bm_destroy(void)
{
	free(map);
}

static void access_ok(int bit)
{
	if (bit > nr_bits)
		DIE("bitmap overflow");
}

void bm_set(int bit_nr)
{
	int byte = bit_nr / 8;
	int bit = bit_nr % 8;

	access_ok(bit_nr);

	pthread_mutex_lock(&mutex);
	*(map + byte) |= 1 << bit;
	pthread_mutex_unlock(&mutex);

	//printf("set bit %d of byte %d\n", 1 << bit, byte);
}

void bm_clear(int bit_nr)
{
	int byte = bit_nr / 8;
	int bit = bit_nr % 8;

	access_ok(bit_nr);

	pthread_mutex_lock(&mutex);
	*(map + byte) &= ~(1 << bit);
	pthread_mutex_unlock(&mutex);

	//printf("clear bit %d of byte %d\n", 1 << bit, byte);
}

int bm_test(int bit_nr)
{
	int byte = bit_nr / 8;
	int bit = bit_nr % 8;
	int set;

	access_ok(bit_nr);

	pthread_mutex_lock(&mutex);
	set = *(map + byte) & (1 << bit);
	pthread_mutex_unlock(&mutex);

	if (set)
		return 1;
	else
		return 0;
}

// TODO: add word-wise test for performance
//       also add max used counter to limit scan to currently highest pid!

void bm_dump(void)
{
	int i;

	printf("bitmap:\n");
	for (i = 0; i < nr_bits; i++) {
		if (bm_test(i))
			printf("+");
		else
			printf("-");
	}
	printf("\n");
}

/*
int main(void)
{
	bm_alloc(4096); // proc 0 - 63

	bm_set(0);
	bm_set(1);
	bm_set(8);
	bm_set(4095);

	bm_dump();

	bm_clear(0);
	bm_clear(1);
	bm_clear(8);
	bm_clear(4095);

	bm_dump();

	bm_destroy();
	exit(EXIT_SUCCESS);
}
*/

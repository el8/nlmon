/*
 * Copyright Penguin Boost, 2016
 * Author(s): Jan Glauber <jan.glauber@gmail.com>
 *
 * Caching layer based on pre-sorted lists.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stddef.h>

#include "nlmon.h"
#include "helper.h"
#include "rbtree.h"

struct rb_root cache_tree = RB_ROOT;

extern enum sort_options opt_sort;

int (*compare_fn) (struct taskstat_delta *t1, struct taskstat_delta *t2);

/* sost alphabetically by task name ignoring case */
static int compare_name(struct taskstat_delta *t1, struct taskstat_delta *t2)
{
	return strncasecmp(t1->comm, t2->comm, 16);
}

/* sort by increasing tid */
static int compare_tid(struct taskstat_delta *t1, struct taskstat_delta *t2)
{
	int a = t2->tid;
	int b = t1->tid;

	return (a < b) ? 1 : (a > b) ? -1 : 0;
}

/* sort be decreasing cpu time */
static int compare_time(struct taskstat_delta *t1, struct taskstat_delta *t2)
{
	unsigned long long a = t1->utime + t1->stime;
	unsigned long long b = t2->utime + t2->stime;

	return (a < b) ? 1 : (a > b) ? -1 : 0;
}

/* sort by decreasing I/O */
static int compare_io(struct taskstat_delta *t1, struct taskstat_delta *t2)
{
	unsigned long long a = t1->io_rd_bytes + t1->io_wr_bytes;
	unsigned long long b = t2->io_rd_bytes + t2->io_wr_bytes;

	return (a < b) ? 1 : (a > b) ? -1 : 0;
}

/* sort by decreasing RSS */
static int compare_mem(struct taskstat_delta *t1, struct taskstat_delta *t2)
{
	int a = (t1->utime + t1->stime) ? t1->rss / (t1->utime + t1->stime) : 0;
	int b = (t2->utime + t2->stime) ? t2->rss / (t2->utime + t2->stime) : 0;

	return (a < b) ? 1 : (a > b) ? -1 : 0;
}

/*
 * This implementation allows to have items with identical keys in tree,
 * hopefully this does not break rbtrees.
 */
int cache_add(struct taskstat_delta *data)
{
	struct rb_node **new = &(cache_tree.rb_node), *parent = NULL;

	/* Figure out where to put new node */
	while (*new) {
		struct taskstat_delta *tmp = container_of(*new, struct taskstat_delta, node);
		int result = compare_fn(data, tmp);

		parent = *new;
		if (result < 0)
			new = &((*new)->rb_left);
		else if (result >= 0)
			new = &((*new)->rb_right);
	}

	/* Add new node and rebalance tree. */
	rb_link_node(&data->node, parent, new);
	rb_insert_color(&data->node, &cache_tree);

	return 1;
}

void cache_init(void)
{
	switch (opt_sort) {
		case OPT_SORT_TID:
			compare_fn = &compare_tid;
			break;
		case OPT_SORT_NAME:
			compare_fn = &compare_name;
			break;
		case OPT_SORT_TIME:
			compare_fn = &compare_time;
			break;
		case OPT_SORT_IO:
			compare_fn = &compare_io;
			break;
		case OPT_SORT_MEM:
			compare_fn = &compare_mem;
			break;
		default:
			compare_fn = &compare_time;
	}
}

struct taskstat_delta *cache_walk(struct taskstat_delta *last)
{
	struct rb_node *node = (last) ? rb_next(&last->node) : rb_first(&cache_tree);

	if (!node)
		return NULL;
	else
		return rb_entry(node, struct taskstat_delta, node);
}

/* remove all elements after cycle is done */
void cache_flush(void)
{
	struct taskstat_delta *data;
	struct rb_node *node;

	do {
		node = rb_first(&cache_tree);
		if (!node)
			continue;
		data = rb_entry(node, struct taskstat_delta, node);
		rb_erase(&data->node, &cache_tree);
		free(data);
	} while (rb_first(&cache_tree) != NULL);
}

/*
 * Copyright Penguin Boost, 2016
 * Author(s): Jan Glauber <jan.glauber@gmail.com>
 *
 * Simple hash implementation for an integer key
 * (thats why glibc's hsearch was not used (beside that it sucks)).
 */
#include <stdlib.h>
#include <stdio.h>
#include <pthread.h>
#include <string.h>

#include "hash.h"
#include "helper.h"

static pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
static struct hash_entry *htab[HASH_ENTRIES];

/*
void alloc_hash(void)
{
	htab = calloc(HASH_ENTRIES, sizeof(struct hash_entry));
	if (!htab)
		DIE_PERROR("calloc failed");
}

void free_hash(void)
{
	free(htab);
}
*/

static void hash_entry(struct hash_entry *h)
{
	struct hash_entry **hptr = &htab[hashfn(h->tid)];

	if ((h->next = *hptr) != NULL)
		(*hptr)->pprev = &h->next;
	*hptr = h;
	h->pprev = hptr;
}

static void unhash_entry(struct hash_entry *h)
{
	if (h->next)
		h->next->pprev = h->pprev;
	*h->pprev = h->next;
}

/* returns NULL if not found */
static struct hash_entry *search_entry(int tid)
{
	struct hash_entry *h, **hptr = &htab[hashfn(tid)];

	for (h = *hptr; h && h->tid != tid; h = h->next);

        return h;
}

/* ignores double-adds */
void create_hash_entry(int tid, int tgid)
{
	struct hash_entry *new;

	pthread_mutex_lock(&mutex);
	new = search_entry(tid);
	if (new) {
		pthread_mutex_unlock(&mutex);
		DEBUG("duplicated add for tid %d\n", tid);
		return;
	}
	new = malloc(sizeof(struct hash_entry));
	if (!new)
		DIE_PERROR("malloc failed");
	memset(new, 0, sizeof(struct hash_entry));
	new->tid = tid;
	new->tgid = tgid;
	hash_entry(new);
	//fprintf(stderr, "hashed tid %d\n", tid);
	pthread_mutex_unlock(&mutex);
}

/* ignores already-gone */
void remove_hash_entry(int tid)
{
	struct hash_entry *old;

	pthread_mutex_lock(&mutex);
	old = search_entry(tid);
	if (!old) {
		pthread_mutex_unlock(&mutex);
		DEBUG("duplicated del for tid %d\n", tid);
		return;
	}
	unhash_entry(old);
	//fprintf(stderr, "unhashed tid %d\n", tid);
	pthread_mutex_unlock(&mutex);
	free(old);
}

/* accquires the hash table lock */
struct hash_entry *get_hash_entry(int tid)
{
	struct hash_entry *h;

	pthread_mutex_lock(&mutex);
	h = search_entry(tid);
	if (!h) {
		pthread_mutex_unlock(&mutex);
		return NULL;
	}
	return h;
}

/* releases the hash table lock */
void put_hash_entry(int tid)
{
	pthread_mutex_unlock(&mutex);
}

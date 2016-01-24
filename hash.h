#ifndef _HASH_H
#define _HASH_H

/*
 * Simple hash implementation for an integer key
 * (thats why glibc's hsearch was not used (beside that it sucks))
 */

/* this fits roughly the number of expected threads */
#define HASH_ENTRIES	1024

/* good enough for PIDs */
#define hashfn(x)	((x) % HASH_ENTRIES)

struct hash_entry {
	int tid;			/* the key */
	int tgid;
	struct hash_entry *next;
	struct hash_entry **pprev;	// WTF
	/* data */
	unsigned long long utime;	// carefull here with 64 bits... or need a lock
	unsigned long long stime;
	unsigned long long cpu_delay;
	unsigned long long rss;
	unsigned long long io_rd_bytes;
	unsigned long long io_wr_bytes;
	unsigned long long blkio_delay;
};

/* hash interface prototypes */
struct hash_entry *get_hash_entry(int tid);
void put_hash_entry(int tid);
void create_hash_entry(int tid, int tgid);
void remove_hash_entry(int tid);

#endif

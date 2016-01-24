#ifndef _BITMAP_H
#define _BITMAP_H

/*
 * bitmap interface prototypes
 */

void bm_alloc(int bits);
void bm_destroy(void);
void bm_set(int bit_nr);
void bm_clear(int bit_nr);
int bm_test(int bit_nr);

#endif

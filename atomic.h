#ifndef _ATOMIC_H
#define _ATOMIC_H

typedef struct {
        int counter;
} atomic_t;


/*
 * Basic atomic ops implementation for ARMv6(+)
 */
#ifdef ARM

static inline void atomic_add(int i, atomic_t *v)
{
	unsigned long tmp;
	int result;

	__asm__ __volatile__("\n"
"1:	ldrex   %0, [%3]\n"
"	add     %0, %0, %4\n"
"	strex   %1, %0, [%3]\n"
"	teq     %1, #0\n"
"	bne     1b"
	: "=&r" (result), "=&r" (tmp), "+Qo" (v->counter)
	: "r" (&v->counter), "Ir" (i)
	: "cc");
}

static inline void atomic_sub(int i, atomic_t *v)
{
	unsigned long tmp;
	int result;

	__asm__ __volatile__("\n"
"1:	ldrex   %0, [%3]\n"
"	sub     %0, %0, %4\n"
"	strex   %1, %0, [%3]\n"
"	teq     %1, #0\n"
"	bne     1b"
	: "=&r" (result), "=&r" (tmp), "+Qo" (v->counter)
	: "r" (&v->counter), "Ir" (i)
	: "cc");
}

#else /* !ARM */

/* for now these are just non-atomic fakes ... */

static inline void atomic_add(int i, atomic_t *v)
{
	v->counter += i;
}

static inline void atomic_sub(int i, atomic_t *v)
{
	v->counter -= i;
}

#endif /* ARM */

#define atomic_inc(v)           atomic_add(1, v)
#define atomic_dec(v)           atomic_sub(1, v)

static inline int atomic_read(const atomic_t *v)
{
        return (*(volatile int *)&(v)->counter);
}

static inline void atomic_set(atomic_t *v, int i)
{
        v->counter = i;
}

#endif /* _ATOMIC_H */

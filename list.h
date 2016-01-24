/*
 * Simple doubly linked list implementation.
 * Derived from Linux kernel include/linux/list.h.
 */

#ifndef _LIST_H
#define _LIST_H

struct list_head {
	struct list_head *prev, *next;
};

static inline void list_init(struct list_head *head)
{
	head->prev = head;
	head->next = head;
}

static inline void __list_add(struct list_head *new,
			      struct list_head *prev,
			      struct list_head *next)
{
	next->prev = new;
	new->next = next;
	new->prev = prev;
	prev->next = new;
}

static inline void list_add_tail(struct list_head *new, struct list_head *head)
{
	__list_add(new, head->prev, head);
}

static inline int list_is_empty(struct list_head *head)
{
	return head->next == head;
}

#endif /* _LIST_H */

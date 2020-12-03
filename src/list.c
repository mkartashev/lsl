#include "list.h"
#include <assert.h>
#include <memory.h>
#include <stdlib.h>

/**
 * Prepends 'item' to 'list'; returns the new list head.
 */
extern list_t	list_add(list_t list, list_t item)
{
	// list may be NULL
	assert(item);

	item->next = list;
	return item;
}

/**
 * Inserts 'item' after the 'list' element of a list.
 * Returns 'item'.
 */
extern list_t	list_insert(list_t list, list_t item)
{
	assert(list);
	assert(item);

	item->next = list->next;
	list->next = item;
	return item;
}

/**
 * Creates (on the heap) a new list element, initializes it with 0 and returns the new element.
 */
extern list_t	list_create()
{
	list_t	l = calloc(1, sizeof(struct list_s));
	assert(l);

	return l;
}

/**
 * Deletes given list element, freeing its memory.
 */
extern list_t	list_delete(list_t list)
{
	assert(list);

	list_t	n = list->next;
	free(list);
	return n;
}

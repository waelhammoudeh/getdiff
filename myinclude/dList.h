/*
 * dList.h
 *
 *	This is Double Linked List header file.
 *
 *  Created on: Jan 5, 2019
 *      Author: wael
 *
 */

#ifndef DLIST_H_
#define DLIST_H_

#include <stdlib.h>

typedef struct DL_ELEM_ {

	void			*data;
	struct DL_ELEM_	*next;
	struct DL_ELEM_ *prev;

} DL_ELEM;

typedef struct DL_LIST_ {

	int			size;
	void		(*destroy) (void **data); // data is pointer to pointer
	int     	(*compare ) (const char *str1, const char *str2);
	DL_ELEM		*head;
	DL_ELEM		*tail;

} DL_LIST;

void initialDL (DL_LIST *list,
				 void (*destroy) (void **data),
				 int (*compare) (const char *str1, const char *str2));

int insertNextDL (DL_LIST *list, DL_ELEM *nextTo, const void *data);

int insertPrevDL (DL_LIST *list, DL_ELEM *before, const void *data);

int removeDL (DL_LIST *list, DL_ELEM *element, void **data);

void destroyDL (DL_LIST *list);

int ListInsertInOrder (DL_LIST *list, char *str);

#define DL_SIZE(list)  ((list)->size)

#define DL_HEAD(list)  ((list)->head)

#define DL_TAIL(list)  ((list)->tail)

#define DL_IS_HEAD(element)  ((element)->prev == NULL ? 1 : 0)

#define DL_IS_TAIL(element)  ((element)->next == NULL ? 1 : 0)

#define DL_NEXT(element)  ((element)->next)

#define DL_PREV(element)  ((element)->prev)

#define DL_DATA(element)  ((element)->data)

#endif /* DLIST_H_ */

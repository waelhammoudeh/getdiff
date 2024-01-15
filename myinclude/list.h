/*
 * dList.h
 *
 *	This is Double Linked List header file.
 *
 *  Created on: Jan 5, 2019
 *
 * renamed DL_ELEM to ELEM and DL_LIST to DLIST, keep D in DLIST,
 * we have a lot of functions with "DL". 9/15/2022
 *
 */

#ifndef DLIST_H_
#define DLIST_H_

/* types definitions */

typedef unsigned char LIST_TYPE;

typedef struct ELEM_ {

  void			*data;
  struct ELEM_	*next;
  struct ELEM_ *prev;

} ELEM;

typedef struct DLIST_ {

  LIST_TYPE    listType;
  int			size;
  void		(*destroy) (void **data);
  int     	(*compare) (const char *str1, const char *str2);
  ELEM		*head;
  ELEM		*tail;

} DLIST;

void initialDL (DLIST *list,
		void (*destroy) (void **data),
		int (*compare) (const char *str1, const char *str2));

int insertNextDL (DLIST *list, ELEM *nextTo, const void *data);

int insertPrevDL (DLIST *list, ELEM *before, const void *data);

int removeDL (DLIST *list, ELEM *element, void **data);

void destroyDL (DLIST *list);

int ListInsertInOrder (DLIST *list, char *str);

#define listInsertInOrder(l, str) ListInsertInOrder(l, str)

#define DL_SIZE(list)  ((list)->size)

#define DL_HEAD(list)  ((list)->head)

#define DL_TAIL(list)  ((list)->tail)

#define DL_IS_HEAD(element)  ((element)->prev == NULL ? 1 : 0)

#define DL_IS_TAIL(element)  ((element)->next == NULL ? 1 : 0)

#define DL_NEXT(element)  ((element)->next)

#define DL_PREV(element)  ((element)->prev)

#define DL_DATA(element)  ((element)->data)

/* include STRING_LIST from "primitives.h" here **/

void zapString(void **string);

#define STRING_LT   1

typedef DLIST STRING_LIST;

#define TYPE_STRING_LIST(list) ((list)->listType == STRING_LT)

STRING_LIST *initialStringList();

void zapStringList(void **strList);


#endif /* DLIST_H_ */

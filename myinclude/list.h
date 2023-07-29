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

/* List Type constant */
#define  STRING_LT     1
#define	GPS_LT     2
#define	SEGMENT_LT     3
#define	GEOMETRY_LT     4
#define  POLYGON_LT     5
#define FRQNCY_LT    6

typedef DLIST STRING_LIST;
typedef DLIST GPS_LIST;
typedef DLIST SEGMENT;
typedef DLIST GEOMETRY;
typedef DLIST POLYGON;
typedef DLIST FRQNCY_LIST;

#define TYPE_STRING_LIST(list) ((list)->listType == STRING_LT)
#define TYPE_GPS_LIST(list) ((list)->listType == GPS_LT)
#define TYPE_SEGMENT(list) ((list)->listType == SEGMENT_LT)
#define TYPE_GEOMETRY(geometry) ((geometry)->listType == GEOMETRY_LT)
#define TYPE_POLYGON(list) ((list)->listType == POLYGON_LT)
#define TYPE_FRQNCY_LIST(list) ((list)->listType == FRQNCY_LT)

STRING_LIST *initialStringList();

GPS_LIST *initialGpsList();

void zapGpsList(void **gpsList);

SEGMENT *initialSegment();

GEOMETRY *initialGeometry();

POLYGON *initialPolygon();

FRQNCY_LIST *initialFrqncyList();


#endif /* DLIST_H_ */

/*
 * dList.c
 *
 *  Created on: Jan 5, 2019
 *
 *		Source: Mastering Algorithms with C by Kyle Loudon. O'Reilly
 *
 *		D: is for double-linked list; element has pointers to next & previous element.
 *
 ****************************************************************************/

#include <stdlib.h>
#include <string.h>
#include "ztError.h"
#include "list.h"
#include "util.h"

/* initialDL(): initials double linked list, caller allocates memory for list */
void initialDL (DLIST *list,
		void (*destroy) (void **data),
		int (*compare) (const char *str1, const char *str2)){

  ASSERTARGS(list); //function pointers (destroy & compare) can be NULL

  list->size = 0;
  list->destroy = destroy;
  list->compare = compare;
  list->head = NULL;
  list->tail = NULL;

  return;

}  /* END initialDL() */

/* insertNextToDL(): Creates (allocates memory) for new ELEM sets data
 * member pointer in the new element to data. The new element is inserted
 * next to element pointed to by nextTo, if nextTo is NULL new element is
 * inserted as head of list. Caller manages storage for data.
 * Function calls abort() if either list or data parameter is NULL.
 * Function returns ztSuccess or ztMemoryAllocate if allocation fails.
 *
 *****************************************************************************/

int insertNextDL (DLIST *list, ELEM *nextTo, const void *data) {

  ELEM *newElem;

  ASSERTARGS(list && data); // abort() if NULL

  /* NULL is not allowed when list is not empty. Will be confusing because
   * we have insertPrevDL() function; both functions add new element as
   * head when nextTo is NULL only if list is EMPTY. */
  if (nextTo == NULL && DL_SIZE(list) != 0){
    printf ("insertNextDL(): Error null nextTo AND empty list not allowed.\n");
    return ztListNotEmpty;
  }

  /* allocate memory for newElem */
  newElem = (ELEM *) malloc (sizeof(ELEM));
  if (newElem == NULL )

    return ztMemoryAllocate;

  memset(newElem, 0, sizeof(ELEM));

  newElem->data = (void *) data; // set data member

  if (DL_SIZE(list) == 0) {   /* empty list, insert as head */

    list->head = newElem;
    list->tail = newElem;
    list->head->prev = NULL;
    list->head->next = NULL;
  }
  else {                      /* list is NOT empty */

    newElem->next = nextTo->next;
    newElem->prev = nextTo;

    if (nextTo->next == NULL)  /* adding newElem next to tail  */
      list->tail = newElem;
    else
      nextTo->next->prev = newElem;

    nextTo->next = newElem;

  }   /* end else */

  list->size++;

  return ztSuccess;

}  // END insertNextDL()

int insertPrevDL (DLIST *list, ELEM *before, const void *data) {

  ELEM *newElem;

  ASSERTARGS (list && data);

  /* NULL is not allowed when list is not empty. Will be confusing because
   * we have insertNextDL() function; both functions add new element as
   * head when before is NULL only if list is EMPTY. */
  if (before == NULL && DL_SIZE(list) != 0){
    printf ("insertPrevDL(): Error null before AND empty list not allowed.\n");
    return ztListNotEmpty;
  }

  /* allocate memory for newElem */
  newElem = (ELEM *) malloc (sizeof(ELEM));
  if (newElem == NULL )

    return ztMemoryAllocate;

  memset(newElem, 0, sizeof(ELEM));

  newElem->data = (void *) data; // set data member

  // insert new element
  if (DL_SIZE(list) == 0) {   /* empty list */

    list->head = newElem;
    list->tail = newElem;
    list->head->prev = NULL;
    list->head->next = NULL;
  }

  else {

    newElem->next = before;
    newElem->prev = before->prev;

    if (before->prev == NULL)
      list->head = newElem;
    else
      before->prev->next = newElem;

    before->prev = newElem;

  }   /* end else */

  list->size++;

  return ztSuccess;

}  // END insertPrevDL()

/* removeDL(): Removes element pointed to by element from double linked list
 * pointed to by list; pointer to data stored in removed element is returned
 * in data.
 * Function returns ztInvalidArg if element is NULL or list size is zero, else
 * it returns ztSuccess.
 *****************************************************************************/

int removeDL (DLIST *list, ELEM *element, void **data) {

  *data = NULL; /* if we fail, this is what you get **/

  if (element == NULL || DL_SIZE(list) == 0){
    printf ("removeDL(): Error NULL element OR empty list.\n");
    return ztInvalidArg;
  }

  *data = element->data; //set pointer

  if (element == list->head) {  /* remove head */

    list->head = element->next;

    if (list->head == NULL)
      list->tail = NULL;
    else
      element->next->prev = NULL;

  }   /* end if */

  else { //other than head

    element->prev->next = element->next;

    if (element->next == NULL)
      list->tail = element->prev;
    else
      element->next->prev = element->prev;

  }

  /** we should be able to free this! but I get:
      corrupted size vs. prev_size
      Aborted

      current GCC v. 10.2.0 & glibc-2.32 FIXME

      free(element);
      we allocated memory for it when inserting --
      ADDED back call to free on 1/17/2022 w.h seems okay now!?
  **/

  free(element);

  list->size--;

  return ztSuccess;

}  /*  END removeDL()  */

void destroyDL (DLIST *list) {

  void			*data;

  ASSERTARGS (list);

  while (list->size > 0){

    if (removeDL (list, DL_TAIL(list),  (void **) &data) == ztSuccess && list->destroy ){
      if (data)
	list->destroy ((void**) &data);
    }
  }

  memset (list, 0, sizeof(DLIST));

  return;

}  /* END destroyDL()  */

/* ListInsertInOrder(): function to insert string in doubly linked list in
 * Alphabetical order - really ASCII character order.
 * We have THREE cases to consider:
 * 1) List is empty; just add string as head.
 * 2) List with one element; add string before or after the only element.
 * 3) List has many elements; add string between the greater and smaller to it.
 *
 * THIS FUNCTION REPLACES ListInsertStr() AND InsertName().
 *
 */
int ListInsertInOrder (DLIST *list, char *str){

  ELEM   *start, *end;
  int    added = 0;
  int    result;

  ASSERTARGS (list && str);

  if (strlen(str) == 0)    /* do not allow empty string element */

    return ztSuccess;

  if (DL_SIZE(list) == 0){  /* empty list; add str as head and we are done  */

    result = insertNextDL (list, DL_HEAD(list), str);
    if (result != ztSuccess){
      printf("ListInsertInOrder(): Error returned by insertNextDL().\n");
      printf("Message: %s\n\n", ztCode2Msg(result));
    }
    return result;
  }

  if ( DL_SIZE(list) == 1 ) {   /* one element list; compare and add, we are done */

    if ( strcmp(list->head->data, str) == 0 )

      return ztSuccess; /* do not allow duplicate */

    else if ( strcmp(list->head->data, str) < 0 )

      result = insertNextDL (list, DL_HEAD(list), str);

    else

      result = insertPrevDL (list, DL_HEAD(list), str);

    if (result != ztSuccess){
      printf("ListInsertInOrder(): Error returned by insert NEXT or PREV().\n");
      printf("Message: %s\n\n", ztCode2Msg(result));
    }

    return result;
  }

  /* list has multiple elements  */

  start = DL_HEAD (list);
  end = DL_TAIL (list);

  if ( (strcmp (str, start->data) == 0) || (strcmp (str, end->data) == 0) )

    return ztSuccess; /* do not allow duplicate */

  /* if str < list head; add it before and we are done */
  if ( strcmp (str, start->data) < 0 ) {

    result = insertPrevDL (list, start, str);
    if (result != ztSuccess){
      printf("ListInsertInOrder(): Error returned by insertPrevDL().\n");
      printf("Message: %s\n\n", ztCode2Msg(result));
    }
    return result;
  }

  /* if str > list tail; add it after the tail and we are done */
  else if ( strcmp (str, end->data) > 0) {

    result = insertNextDL (list, end, str);
    if (result != ztSuccess){
      printf("ListInsertInOrder(): Error returned by insertNextDL().\n");
      printf("Message: %s\n\n", ztCode2Msg(result));
    }
    return result;
  }

  while ( start && start->next ){

    /* do not insert duplicate */
    if ( (strcmp (str, start->data) == 0 ) || (strcmp (str, start->next->data) == 0) )

      return ztSuccess;

    /* does str fit between two adjacent element? */
    if ( (strcmp (str, start->data) > 0 ) && (strcmp(str, start->next->data) < 0) ){

      result = insertNextDL (list, start, str);
      if (result != ztSuccess){
	printf("ListInsertInOrder(): Error returned by insertNextDL().\n");
	printf("Message: %s\n\n", ztCode2Msg(result));
      }

      added = 1;
      return result;
    }

    start = DL_NEXT(start);

  }   /* end while */

  if( ! added ){ // insert next to tail

    result = insertNextDL(list, DL_TAIL(list), str);
    if (result != ztSuccess){
      printf("ListInsertInOrder(): Error returned by insertNextDL().\n");
      printf("Message: %s\n\n", ztCode2Msg(result));
    }
    return result;
  }

  return ztSuccess;   /* we never get to this point!!  */

}   /* END ListInsertInOrder() */

STRING_LIST *initialStringList(){

  STRING_LIST *newList;

  newList = (STRING_LIST *) malloc(sizeof(STRING_LIST));
  if (! newList ){
    fprintf(stderr, "initialStringList(): Error allocating memory.\n");
    return newList;
  }

  initialDL((DLIST *) newList, NULL, NULL);
  newList->listType = STRING_LT;

  return newList;

} /* END initialStringList() */

GPS_LIST *initialGpsList(){

  GPS_LIST *newGpsList;

  newGpsList = (GPS_LIST *) malloc(sizeof(GPS_LIST));
  if (! newGpsList ){
    fprintf(stderr, "initialGpsList(): Error allocating memory.\n");
    return newGpsList;
  }

  initialDL((DLIST *) newGpsList, NULL, NULL);
  newGpsList->listType = GPS_LT;

  return newGpsList;

} /* END initialGpsList() */

void zapGpsList(void **gpsList){

  GPS_LIST    *list;

  ASSERTARGS(gpsList);

  list = (GPS_LIST *) *gpsList;

  if ( ! list )

    return;

  if (list->listType != GPS_LT)

    return;


  destroyDL(list);
  memset(list, 0, sizeof(GPS_LIST));
  free(list);
  list = NULL;

  return;

} /* END zapGpsList() */

SEGMENT *initialSegment(){

  SEGMENT *newSeg;

  newSeg = (SEGMENT *)malloc(sizeof(SEGMENT));
  if (! newSeg){
    fprintf(stderr, "initialSegment(): Error allocating memory.\n");
    return newSeg;
  }

  initialDL(newSeg, NULL, NULL);
  newSeg->listType = SEGMENT_LT;

  return newSeg;

} /* END initialSegment() */

GEOMETRY *initialGeometry(){

  GEOMETRY *newG;

  newG = (GEOMETRY *)malloc(sizeof(GEOMETRY));
  if (! newG){
    fprintf(stderr, "initialGeometry(): Error allocating memory.\n");
    return newG;
  }

  initialDL(newG, NULL, NULL);
  newG->listType = GEOMETRY_LT;

  return newG;

} /* END initialGeometry() */

POLYGON *initialPolygon(){

  POLYGON *newPoly;

  newPoly = (POLYGON *)malloc(sizeof(POLYGON));
  if (! newPoly){
    fprintf(stderr, "initialPolygon(): Error allocating memory.\n");
    return newPoly;
  }

  initialDL((DLIST *)newPoly, NULL, NULL);
  newPoly->listType = POLYGON_LT;

  return newPoly;

} /* END initialPolygon() */

FRQNCY_LIST *initialFrqncyList(){

  FRQNCY_LIST *newFL;

  newFL = (FRQNCY_LIST *)malloc(sizeof(FRQNCY_LIST));
  if (! newFL){
    fprintf(stderr, "initialFrqncyList(): Error allocating memory.\n");
    return newFL;
  }

  initialDL((DLIST *)newFL, NULL, NULL);
  newFL->listType = FRQNCY_LT;

  return newFL;

} /* END initialFrqncyList() */


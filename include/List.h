#ifndef LIST_H
#define LIST_H

#include <stdbool.h>

typedef bool(comparator_t)(void *, void *);

typedef struct _List
{
    int size;
    int capacity;
    void **data;
    comparator_t *comparator;
} List;

void *List_Get(const List *list, int index);

void *List_Get2(const List *list, int index, bool *ok);

bool List_Contains(const List *list, void *element);

void List_Set(const List *list, int index, void *data);

void List_Add(List *list, void *data);

void List_Remove(List *list, int index);

void List_RemoveItem(List *list, void *element);

int List_IndexOf(const List *list, void *element);

void List_Init(List *list);

void List_SetComparator(List *list, comparator_t comparator);

void List_Clear(List *list);

#endif // LIST_H

#include <List.h>
#include <stdlib.h>

#define LIST_COMPARE(v1, v2)                                                   \
    (list->comparator == NULL ? v1 == v2 : list->comparator(v1, v2))

void *List_Get(const List *list, const int index)
{
    bool _ok;
    return List_Get2(list, index, &_ok);
}

void *List_Get2(const List *list, const int index, bool *ok)
{
    if (index < 0 || index >= list->size)
    {
        *ok = false;
        return NULL;
    }
    *ok = true;
    return list->data[index];
}

bool List_Contains(const List *list, void *element)
{
    for (int i = 0; i < list->size; i++)
    {
        if (LIST_COMPARE(element, list->data[i]))
        {
            return true;
        }
    }
    return false;
}

void List_Set(const List *list, const int index, void *data)
{
    if (index < 0 || index >= list->size)
    {
        return;
    }
    list->data[index] = data;
}

static void List_EnsureCapacity(List *list, const int min_capacity)
{
    if (list->capacity >= min_capacity)
        return;

    int new_capacity = list->capacity > 0 ? list->capacity : 4;
    while (new_capacity < min_capacity)
    {
        new_capacity *= 2;
    }

    void **new_data = realloc(list->data, new_capacity * sizeof(void *));
    list->data = new_data;
    list->capacity = new_capacity;
}

void List_Add(List *list, void *data)
{
    List_EnsureCapacity(list, list->size + 1);
    list->data[list->size++] = data;
}

void List_Remove(List *list, const int index)
{
    if (index < 0 || index >= list->size)
    {
        return;
    }
    for (int i = index; i < list->size - 1; i++)
    {
        list->data[i] = list->data[i + 1];
    }
    list->size--;
}

void List_RemoveItem(List *list, void *element)
{
    const int index = List_IndexOf(list, element);
    if (index != -1)
    {
        List_Remove(list, index);
    }
}

int List_IndexOf(const List *list, void *element)
{
    for (int i = 0; i < list->size; i++)
    {
        if (LIST_COMPARE(element, list->data[i]))
        {
            return i;
        }
    }
    return -1;
}

void List_Init(List *list)
{
    list->size = 0;
    list->capacity = 0;
    list->data = NULL;
    list->comparator = NULL;
}

void List_SetComparator(List *list, comparator_t *comparator)
{
    list->comparator = comparator;
}

void List_Clear(List *list)
{
    if (list->data)
    {
        free(list->data);
        list->data = NULL;
    }
    list->size = 0;
    list->capacity = 0;
}

#undef LIST_COMPARE

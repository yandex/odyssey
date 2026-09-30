#include <odyssey.h>
#include <postgres_fe.h>
#include <sql/full/pg_list.h>

#ifndef Max
#define Max(x, y)		((x) > (y) ? (x) : (y))
#endif

#ifndef Min
#define Min(x, y)		((x) < (y) ? (x) : (y))
#endif

bool equal(const void *a, const void *b) { return a == b; }

#define LIST_HEADER_OVERHEAD  \
	((int) ((offsetof(List, initial_elements) - 1) / sizeof(ListCell) + 1))

#define IsPointerList(l)		((l) == NIL || IsA((l), List))
#define IsIntegerList(l)		((l) == NIL || IsA((l), IntList))

static void
check_list_invariants(const List *list)
{
	if (list == NIL)
		return;

	Assert(list->length > 0);
	Assert(list->length <= list->max_length);
	Assert(list->elements != NULL);

	Assert(list->type == T_List ||
		   list->type == T_IntList);
}

static List *
new_list(NodeTag type, int min_size)
{
	List	   *newlist;
	int			max_size;

	Assert(min_size > 0);

	max_size = pg_nextpower2_32(Max(8, min_size + LIST_HEADER_OVERHEAD));
	max_size -= LIST_HEADER_OVERHEAD;

	newlist = (List *) palloc(offsetof(List, initial_elements) +
							  max_size * sizeof(ListCell));
	newlist->type = type;
	newlist->length = min_size;
	newlist->max_length = max_size;
	newlist->elements = newlist->initial_elements;

	return newlist;
}

static void
enlarge_list(List *list, int min_size)
{
	int			new_max_len;

	Assert(min_size > list->max_length);

	new_max_len = pg_nextpower2_32(Max(16, min_size));

	if (list->elements == list->initial_elements)
	{
		list->elements = (ListCell *)
			palloc(new_max_len * sizeof(ListCell));
		memcpy(list->elements, list->initial_elements,
			   list->length * sizeof(ListCell));
	}
	else
	{
		list->elements = (ListCell *)
			repalloc(list->elements, new_max_len * sizeof(ListCell));
	}

	list->max_length = new_max_len;
}

List *
list_make1_impl(NodeTag t, ListCell datum1)
{
	List	   *list = new_list(t, 1);

	list->elements[0] = datum1;
	check_list_invariants(list);
	return list;
}

List *
list_make2_impl(NodeTag t, ListCell datum1, ListCell datum2)
{
	List	   *list = new_list(t, 2);

	list->elements[0] = datum1;
	list->elements[1] = datum2;
	check_list_invariants(list);
	return list;
}

List *
list_make3_impl(NodeTag t, ListCell datum1, ListCell datum2,
				ListCell datum3)
{
	List	   *list = new_list(t, 3);

	list->elements[0] = datum1;
	list->elements[1] = datum2;
	list->elements[2] = datum3;
	check_list_invariants(list);
	return list;
}

List *
list_make4_impl(NodeTag t, ListCell datum1, ListCell datum2,
				ListCell datum3, ListCell datum4)
{
	List	   *list = new_list(t, 4);

	list->elements[0] = datum1;
	list->elements[1] = datum2;
	list->elements[2] = datum3;
	list->elements[3] = datum4;
	check_list_invariants(list);
	return list;
}

List *
list_make5_impl(NodeTag t, ListCell datum1, ListCell datum2,
				ListCell datum3, ListCell datum4, ListCell datum5)
{
	List	   *list = new_list(t, 5);

	list->elements[0] = datum1;
	list->elements[1] = datum2;
	list->elements[2] = datum3;
	list->elements[3] = datum4;
	list->elements[4] = datum5;
	check_list_invariants(list);
	return list;
}

static void
new_head_cell(List *list)
{
	if (list->length >= list->max_length)
		enlarge_list(list, list->length + 1);
	memmove(&list->elements[1], &list->elements[0],
			list->length * sizeof(ListCell));
	list->length++;
}

static void
new_tail_cell(List *list)
{
	if (list->length >= list->max_length)
		enlarge_list(list, list->length + 1);
	list->length++;
}

List *
lappend(List *list, void *datum)
{
	Assert(IsPointerList(list));

	if (list == NIL)
		list = new_list(T_List, 1);
	else
		new_tail_cell(list);

	llast(list) = datum;
	check_list_invariants(list);
	return list;
}

List *
lappend_int(List *list, int datum)
{
	Assert(IsIntegerList(list));

	if (list == NIL)
		list = new_list(T_IntList, 1);
	else
		new_tail_cell(list);

	llast_int(list) = datum;
	check_list_invariants(list);
	return list;
}

List *
lcons(void *datum, List *list)
{
	Assert(IsPointerList(list));

	if (list == NIL)
		list = new_list(T_List, 1);
	else
		new_head_cell(list);

	linitial(list) = datum;
	check_list_invariants(list);
	return list;
}

List *
lcons_int(int datum, List *list)
{
	Assert(IsIntegerList(list));

	if (list == NIL)
		list = new_list(T_IntList, 1);
	else
		new_head_cell(list);

	linitial_int(list) = datum;
	check_list_invariants(list);
	return list;
}

List *
list_concat(List *list1, const List *list2)
{
	int			new_len;

	if (list1 == NIL)
		return list_copy(list2);
	if (list2 == NIL)
		return list1;

	Assert(list1->type == list2->type);

	new_len = list1->length + list2->length;
	if (new_len > list1->max_length)
		enlarge_list(list1, new_len);

	memcpy(&list1->elements[list1->length], &list2->elements[0],
		   list2->length * sizeof(ListCell));
	list1->length = new_len;

	check_list_invariants(list1);
	return list1;
}

List *
list_concat_copy(const List *list1, const List *list2)
{
	List	   *result;
	int			new_len;

	if (list1 == NIL)
		return list_copy(list2);
	if (list2 == NIL)
		return list_copy(list1);

	Assert(list1->type == list2->type);

	new_len = list1->length + list2->length;
	result = new_list(list1->type, new_len);
	memcpy(result->elements, list1->elements,
		   list1->length * sizeof(ListCell));
	memcpy(result->elements + list1->length, list2->elements,
		   list2->length * sizeof(ListCell));

	check_list_invariants(result);
	return result;
}

List *
list_truncate(List *list, int new_size)
{
	if (new_size <= 0)
		return NIL;

	if (new_size < list_length(list))
		list->length = new_size;

	return list;
}

bool
list_member(const List *list, const void *datum)
{
	const ListCell *cell;

	Assert(IsPointerList(list));
	check_list_invariants(list);

	foreach(cell, list)
	{
		if (equal(lfirst(cell), datum))
			return true;
	}

	return false;
}

bool
list_member_ptr(const List *list, const void *datum)
{
	const ListCell *cell;

	Assert(IsPointerList(list));
	check_list_invariants(list);

	foreach(cell, list)
	{
		if (lfirst(cell) == datum)
			return true;
	}

	return false;
}

bool
list_member_int(const List *list, int datum)
{
	const ListCell *cell;

	Assert(IsIntegerList(list));
	check_list_invariants(list);

	foreach(cell, list)
	{
		if (lfirst_int(cell) == datum)
			return true;
	}

	return false;
}

List *
list_delete_nth_cell(List *list, int n)
{
	check_list_invariants(list);

	Assert(n >= 0 && n < list->length);

	if (list->length == 1)
	{
		list_free(list);
		return NIL;
	}

	memmove(&list->elements[n], &list->elements[n + 1],
			(list->length - 1 - n) * sizeof(ListCell));
	list->length--;

	return list;
}

List *
list_delete_cell(List *list, ListCell *cell)
{
	return list_delete_nth_cell(list, cell - list->elements);
}

List *
list_delete_first(List *list)
{
	check_list_invariants(list);

	if (list == NIL)
		return NIL;

	return list_delete_nth_cell(list, 0);
}

List *
list_delete_last(List *list)
{
	check_list_invariants(list);

	if (list == NIL)
		return NIL;

	if (list_length(list) <= 1)
	{
		list_free(list);
		return NIL;
	}

	return list_truncate(list, list_length(list) - 1);
}

List *
list_append_unique(List *list, void *datum)
{
	if (list_member(list, datum))
		return list;
	else
		return lappend(list, datum);
}

List *
list_append_unique_ptr(List *list, void *datum)
{
	if (list_member_ptr(list, datum))
		return list;
	else
		return lappend(list, datum);
}

List *
list_append_unique_int(List *list, int datum)
{
	if (list_member_int(list, datum))
		return list;
	else
		return lappend_int(list, datum);
}

void
list_free(List *list)
{
	if (list == NIL)
		return;

	check_list_invariants(list);

	if (list->elements != list->initial_elements)
		pfree(list->elements);
	pfree(list);
}

List *
list_copy(const List *oldlist)
{
	List	   *newlist;

	if (oldlist == NIL)
		return NIL;

	newlist = new_list(oldlist->type, oldlist->length);
	memcpy(newlist->elements, oldlist->elements,
		   newlist->length * sizeof(ListCell));

	check_list_invariants(newlist);
	return newlist;
}

List *
list_copy_head(const List *oldlist, int len)
{
	List	   *newlist;

	if (oldlist == NIL || len <= 0)
		return NIL;

	len = Min(oldlist->length, len);

	newlist = new_list(oldlist->type, len);
	memcpy(newlist->elements, oldlist->elements, len * sizeof(ListCell));

	check_list_invariants(newlist);
	return newlist;
}

List *
list_copy_tail(const List *oldlist, int nskip)
{
	List	   *newlist;

	if (nskip < 0)
		nskip = 0;

	if (oldlist == NIL || nskip >= oldlist->length)
		return NIL;

	newlist = new_list(oldlist->type, oldlist->length - nskip);
	memcpy(newlist->elements, &oldlist->elements[nskip],
		   newlist->length * sizeof(ListCell));

	check_list_invariants(newlist);
	return newlist;
}

void
list_sort(List *list, list_sort_comparator cmp)
{
	typedef int (*qsort_comparator) (const void *a, const void *b);
	int			len;

	check_list_invariants(list);

	len = list_length(list);
	if (len > 1)
		qsort(list->elements, len, sizeof(ListCell), (qsort_comparator) cmp);
}

int
list_int_cmp(const ListCell *p1, const ListCell *p2)
{
	int			v1 = lfirst_int(p1);
	int			v2 = lfirst_int(p2);

	if (v1 < v2)
		return -1;
	else if (v1 > v2)
		return 1;
	else
		return 0;
}

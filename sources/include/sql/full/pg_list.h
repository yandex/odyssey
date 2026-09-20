#ifndef PG_LIST_H
#define PG_LIST_H

#include <sql/full/nodes.h>

typedef union ListCell
{
	void	   *ptr_value;
	int			int_value;
} ListCell;

typedef struct List
{
	NodeTag		type;
	int			length;
	int			max_length;
	ListCell   *elements;
	ListCell	initial_elements[FLEXIBLE_ARRAY_MEMBER];
} List;

#define NIL						((List *) NULL)

typedef struct ForEachState
{
	const List *l;
	int			i;
} ForEachState;

typedef struct ForBothState
{
	const List *l1;
	const List *l2;
	int			i;
} ForBothState;

typedef struct ForBothCellState
{
	const List *l1;
	const List *l2;
	int			i1;
	int			i2;
} ForBothCellState;

typedef struct ForThreeState
{
	const List *l1;
	const List *l2;
	const List *l3;
	int			i;
} ForThreeState;

typedef struct ForFourState
{
	const List *l1;
	const List *l2;
	const List *l3;
	const List *l4;
	int			i;
} ForFourState;

typedef struct ForFiveState
{
	const List *l1;
	const List *l2;
	const List *l3;
	const List *l4;
	const List *l5;
	int			i;
} ForFiveState;

static inline ListCell *
list_head(const List *l)
{
	return l ? &l->elements[0] : NULL;
}

static inline ListCell *
list_tail(const List *l)
{
	return l ? &l->elements[l->length - 1] : NULL;
}

static inline ListCell *
list_second_cell(const List *l)
{
	if (l && l->length >= 2)
		return &l->elements[1];
	else
		return NULL;
}

static inline int
list_length(const List *l)
{
	return l ? l->length : 0;
}

#define lfirst(lc)				((lc)->ptr_value)
#define lfirst_int(lc)			((lc)->int_value)
#define lfirst_node(type,lc)	castNode(type, lfirst(lc))

#define linitial(l)				lfirst(list_nth_cell(l, 0))
#define linitial_int(l)			lfirst_int(list_nth_cell(l, 0))
#define linitial_node(type,l)	castNode(type, linitial(l))

#define lsecond(l)				lfirst(list_nth_cell(l, 1))
#define lsecond_int(l)			lfirst_int(list_nth_cell(l, 1))
#define lsecond_node(type,l)	castNode(type, lsecond(l))

#define lthird(l)				lfirst(list_nth_cell(l, 2))
#define lthird_int(l)			lfirst_int(list_nth_cell(l, 2))
#define lthird_node(type,l)		castNode(type, lthird(l))

#define lfourth(l)				lfirst(list_nth_cell(l, 3))
#define lfourth_int(l)			lfirst_int(list_nth_cell(l, 3))
#define lfourth_node(type,l)	castNode(type, lfourth(l))

#define llast(l)				lfirst(list_last_cell(l))
#define llast_int(l)			lfirst_int(list_last_cell(l))
#define llast_node(type,l)		castNode(type, llast(l))

static inline ListCell
list_make_ptr_cell(void *v)
{
	ListCell	c;

	c.ptr_value = v;
	return c;
}

static inline ListCell
list_make_int_cell(int v)
{
	ListCell	c;

	c.int_value = v;
	return c;
}

#define list_make1(x1) \
	list_make1_impl(T_List, list_make_ptr_cell(x1))
#define list_make2(x1,x2) \
	list_make2_impl(T_List, list_make_ptr_cell(x1), list_make_ptr_cell(x2))
#define list_make3(x1,x2,x3) \
	list_make3_impl(T_List, list_make_ptr_cell(x1), list_make_ptr_cell(x2), \
					list_make_ptr_cell(x3))
#define list_make4(x1,x2,x3,x4) \
	list_make4_impl(T_List, list_make_ptr_cell(x1), list_make_ptr_cell(x2), \
					list_make_ptr_cell(x3), list_make_ptr_cell(x4))
#define list_make5(x1,x2,x3,x4,x5) \
	list_make5_impl(T_List, list_make_ptr_cell(x1), list_make_ptr_cell(x2), \
					list_make_ptr_cell(x3), list_make_ptr_cell(x4), \
					list_make_ptr_cell(x5))

#define list_make1_int(x1) \
	list_make1_impl(T_IntList, list_make_int_cell(x1))
#define list_make2_int(x1,x2) \
	list_make2_impl(T_IntList, list_make_int_cell(x1), list_make_int_cell(x2))
#define list_make3_int(x1,x2,x3) \
	list_make3_impl(T_IntList, list_make_int_cell(x1), list_make_int_cell(x2), \
					list_make_int_cell(x3))
#define list_make4_int(x1,x2,x3,x4) \
	list_make4_impl(T_IntList, list_make_int_cell(x1), list_make_int_cell(x2), \
					list_make_int_cell(x3), list_make_int_cell(x4))
#define list_make5_int(x1,x2,x3,x4,x5) \
	list_make5_impl(T_IntList, list_make_int_cell(x1), list_make_int_cell(x2), \
					list_make_int_cell(x3), list_make_int_cell(x4), \
					list_make_int_cell(x5))

static inline ListCell *
list_nth_cell(const List *list, int n)
{
	Assert(list != NIL);
	Assert(n >= 0 && n < list->length);
	return &list->elements[n];
}

static inline ListCell *
list_last_cell(const List *list)
{
	Assert(list != NIL);
	return &list->elements[list->length - 1];
}

static inline void *
list_nth(const List *list, int n)
{
	Assert(IsA(list, List));
	return lfirst(list_nth_cell(list, n));
}

static inline int
list_nth_int(const List *list, int n)
{
	Assert(IsA(list, IntList));
	return lfirst_int(list_nth_cell(list, n));
}

#define list_nth_node(type,list,n)	castNode(type, list_nth(list, n))

static inline int
list_cell_number(const List *l, const ListCell *c)
{
	Assert(c >= &l->elements[0] && c < &l->elements[l->length]);
	return c - l->elements;
}

static inline ListCell *
lnext(const List *l, const ListCell *c)
{
	Assert(c >= &l->elements[0] && c < &l->elements[l->length]);
	c++;
	if (c < &l->elements[l->length])
		return (ListCell *) c;
	else
		return NULL;
}

#define foreach(cell, lst)	\
	for (ForEachState cell##__state = {(lst), 0}; \
		 (cell##__state.l != NIL && \
		  cell##__state.i < cell##__state.l->length) ? \
		 (cell = &cell##__state.l->elements[cell##__state.i], true) : \
		 (cell = NULL, false); \
		 cell##__state.i++)

#define foreach_delete_current(lst, var_or_cell)	\
	((List *) (var_or_cell##__state.l = list_delete_nth_cell(lst, var_or_cell##__state.i--)))

#define foreach_current_index(var_or_cell)  (var_or_cell##__state.i)

#define for_each_from(cell, lst, N)	\
	for (ForEachState cell##__state = for_each_from_setup(lst, N); \
		 (cell##__state.l != NIL && \
		  cell##__state.i < cell##__state.l->length) ? \
		 (cell = &cell##__state.l->elements[cell##__state.i], true) : \
		 (cell = NULL, false); \
		 cell##__state.i++)

static inline ForEachState
for_each_from_setup(const List *lst, int N)
{
	ForEachState r = {lst, N};

	Assert(N >= 0);
	return r;
}

#define for_each_cell(cell, lst, initcell)	\
	for (ForEachState cell##__state = for_each_cell_setup(lst, initcell); \
		 (cell##__state.l != NIL && \
		  cell##__state.i < cell##__state.l->length) ? \
		 (cell = &cell##__state.l->elements[cell##__state.i], true) : \
		 (cell = NULL, false); \
		 cell##__state.i++)

static inline ForEachState
for_each_cell_setup(const List *lst, const ListCell *initcell)
{
	ForEachState r = {lst,
	initcell ? list_cell_number(lst, initcell) : list_length(lst)};

	return r;
}

#define foreach_ptr(type, var, lst) foreach_internal(type, *, var, lst, lfirst)
#define foreach_int(var, lst)	foreach_internal(int, , var, lst, lfirst_int)

#define foreach_internal(type, pointer, var, lst, func) \
	for (type pointer var = 0, pointer var##__outerloop = (type pointer) 1; \
		 var##__outerloop; \
		 var##__outerloop = 0) \
		for (ForEachState var##__state = {(lst), 0}; \
			 (var##__state.l != NIL && \
			  var##__state.i < var##__state.l->length && \
			 (var = (type pointer) func(&var##__state.l->elements[var##__state.i]), true)); \
			 var##__state.i++)

#define foreach_node(type, var, lst) \
	for (type * var = 0, *var##__outerloop = (type *) 1; \
		 var##__outerloop; \
		 var##__outerloop = 0) \
		for (ForEachState var##__state = {(lst), 0}; \
			 (var##__state.l != NIL && \
			  var##__state.i < var##__state.l->length && \
			 (var = lfirst_node(type, &var##__state.l->elements[var##__state.i]), true)); \
			 var##__state.i++)

#define forboth(cell1, list1, cell2, list2)							\
	for (ForBothState cell1##__state = {(list1), (list2), 0}; \
		 multi_for_advance_cell(cell1, cell1##__state, l1, i), \
		 multi_for_advance_cell(cell2, cell1##__state, l2, i), \
		 (cell1 != NULL && cell2 != NULL); \
		 cell1##__state.i++)

#define multi_for_advance_cell(cell, state, l, i) \
	(cell = (state.l != NIL && state.i < state.l->length) ? \
	 &state.l->elements[state.i] : NULL)

#define for_both_cell(cell1, list1, initcell1, cell2, list2, initcell2)	\
	for (ForBothCellState cell1##__state = \
			 for_both_cell_setup(list1, initcell1, list2, initcell2); \
		 multi_for_advance_cell(cell1, cell1##__state, l1, i1), \
		 multi_for_advance_cell(cell2, cell1##__state, l2, i2), \
		 (cell1 != NULL && cell2 != NULL); \
		 cell1##__state.i1++, cell1##__state.i2++)

static inline ForBothCellState
for_both_cell_setup(const List *list1, const ListCell *initcell1,
					const List *list2, const ListCell *initcell2)
{
	ForBothCellState r = {list1, list2,
		initcell1 ? list_cell_number(list1, initcell1) : list_length(list1),
	initcell2 ? list_cell_number(list2, initcell2) : list_length(list2)};

	return r;
}

#define forthree(cell1, list1, cell2, list2, cell3, list3) \
	for (ForThreeState cell1##__state = {(list1), (list2), (list3), 0}; \
		 multi_for_advance_cell(cell1, cell1##__state, l1, i), \
		 multi_for_advance_cell(cell2, cell1##__state, l2, i), \
		 multi_for_advance_cell(cell3, cell1##__state, l3, i), \
		 (cell1 != NULL && cell2 != NULL && cell3 != NULL); \
		 cell1##__state.i++)

#define forfour(cell1, list1, cell2, list2, cell3, list3, cell4, list4) \
	for (ForFourState cell1##__state = {(list1), (list2), (list3), (list4), 0}; \
		 multi_for_advance_cell(cell1, cell1##__state, l1, i), \
		 multi_for_advance_cell(cell2, cell1##__state, l2, i), \
		 multi_for_advance_cell(cell3, cell1##__state, l3, i), \
		 multi_for_advance_cell(cell4, cell1##__state, l4, i), \
		 (cell1 != NULL && cell2 != NULL && cell3 != NULL && cell4 != NULL); \
		 cell1##__state.i++)

#define forfive(cell1, list1, cell2, list2, cell3, list3, cell4, list4, cell5, list5) \
	for (ForFiveState cell1##__state = {(list1), (list2), (list3), (list4), (list5), 0}; \
		 multi_for_advance_cell(cell1, cell1##__state, l1, i), \
		 multi_for_advance_cell(cell2, cell1##__state, l2, i), \
		 multi_for_advance_cell(cell3, cell1##__state, l3, i), \
		 multi_for_advance_cell(cell4, cell1##__state, l4, i), \
		 multi_for_advance_cell(cell5, cell1##__state, l5, i), \
		 (cell1 != NULL && cell2 != NULL && cell3 != NULL && \
		  cell4 != NULL && cell5 != NULL); \
		 cell1##__state.i++)

extern List *list_make1_impl(NodeTag t, ListCell datum1);
extern List *list_make2_impl(NodeTag t, ListCell datum1, ListCell datum2);
extern List *list_make3_impl(NodeTag t, ListCell datum1, ListCell datum2,
							 ListCell datum3);
extern List *list_make4_impl(NodeTag t, ListCell datum1, ListCell datum2,
							 ListCell datum3, ListCell datum4);
extern List *list_make5_impl(NodeTag t, ListCell datum1, ListCell datum2,
							 ListCell datum3, ListCell datum4,
							 ListCell datum5);

pg_nodiscard extern List *lappend(List *list, void *datum);
pg_nodiscard extern List *lappend_int(List *list, int datum);

pg_nodiscard extern List *lcons(void *datum, List *list);
pg_nodiscard extern List *lcons_int(int datum, List *list);

pg_nodiscard extern List *list_concat(List *list1, const List *list2);
pg_nodiscard extern List *list_concat_copy(const List *list1, const List *list2);

pg_nodiscard extern List *list_truncate(List *list, int new_size);

extern bool list_member(const List *list, const void *datum);
extern bool list_member_ptr(const List *list, const void *datum);
extern bool list_member_int(const List *list, int datum);

pg_nodiscard extern List *list_delete_first(List *list);
pg_nodiscard extern List *list_delete_last(List *list);
pg_nodiscard extern List *list_delete_nth_cell(List *list, int n);
pg_nodiscard extern List *list_delete_cell(List *list, ListCell *cell);

pg_nodiscard extern List *list_append_unique(List *list, void *datum);
pg_nodiscard extern List *list_append_unique_ptr(List *list, void *datum);
pg_nodiscard extern List *list_append_unique_int(List *list, int datum);

extern void list_free(List *list);

pg_nodiscard extern List *list_copy(const List *oldlist);
pg_nodiscard extern List *list_copy_head(const List *oldlist, int len);
pg_nodiscard extern List *list_copy_tail(const List *oldlist, int nskip);

typedef int (*list_sort_comparator) (const ListCell *a, const ListCell *b);
extern void list_sort(List *list, list_sort_comparator cmp);

extern int	list_int_cmp(const ListCell *p1, const ListCell *p2);

#endif							/* PG_LIST_H */

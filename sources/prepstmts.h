#ifndef OD_PREP_STMTS
#define OD_PREP_STMTS

typedef struct {
	machine_msg_t *msg;
	od_list_t link;
} od_prepstmts_rewrite_list_t;

static inline od_prepstmts_rewrite_list_t *od_prepstmts_rewrite_list_alloc()
{
	od_prepstmts_rewrite_list_t *list =
		malloc(sizeof(od_prepstmts_rewrite_list_t));

	od_list_init(&list->link);
	return list;
}

static inline od_retcode_t
od_prepstmts_msgs_queue(od_prepstmts_rewrite_list_t *list, machine_msg_t *msg)
{
	od_prepstmts_rewrite_list_t *l = od_prepstmts_rewrite_list_alloc();
	l->msg = msg;
	od_list_append(&list->link, &l->link);

	return OK_RESPONSE;
}

static inline od_retcode_t
od_prepstmts_rewrite_list_free(od_prepstmts_rewrite_list_t *l)
{
	od_list_t *i, *j;

	od_list_foreach_safe(&l->link, i, j)
	{
		od_prepstmts_rewrite_list_t *curr;
		curr = od_container_of(i, od_prepstmts_rewrite_list_t, link);

		machine_msg_free(curr->msg);
		od_list_unlink(&curr->link);
		free(curr);
	}

	return OK_RESPONSE;
}

#endif /* OD_PREP_STMTS */

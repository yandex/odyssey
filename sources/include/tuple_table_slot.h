#pragma once

#include <machinarium/machinarium.h>

#include <od_memory.h>

/*
 * tuple_table_slot.
 *
 * A place to store tuples.
 */

typedef struct {
	machine_msg_t *row_description;
	machine_msg_t *data_rows;
} od_tuple_table_slot_t;

static inline od_tuple_table_slot_t *od_tuple_table_slot_create(void)
{
	od_tuple_table_slot_t *tts;
	tts = od_malloc(sizeof(od_tuple_table_slot_t));
	if (tts == NULL) {
		return NULL;
	}

	tts->row_description = machine_msg_create(0);
	if (tts->row_description == NULL) {
		od_free(tts);
		return NULL;
	}

	tts->data_rows = machine_msg_create(0);
	if (tts->data_rows == NULL) {
		machine_msg_free(tts->row_description);
		od_free(tts);
		return NULL;
	}

	return tts;
}

static inline void od_tuple_table_slot_free(od_tuple_table_slot_t *tts)
{
	if (tts == NULL) {
		return;
	}
	if (tts->row_description != NULL) {
		machine_msg_free(tts->row_description);
		tts->row_description = NULL;
	}
	if (tts->data_rows != NULL) {
		machine_msg_free(tts->data_rows);
		tts->data_rows = NULL;
	}
	od_free(tts);
}

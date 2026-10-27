#pragma once

#include <stddef.h>

#include <types.h>
#include <sql/full/ast.h>
#include <sql/full/pg_list.h>

typedef void (*od_sql_full_error_cb_t)(const char *msg, void *userdata);

List *od_sql_full_parse(od_linear_alloc_t *la, const char *input, size_t input_len,
                        od_sql_full_error_cb_t error_cb,
                        void *userdata);

/* Highest bind parameter number, ignoring quoted text and comments; -1 on error. */
int od_sql_full_parameter_count(od_linear_alloc_t *la, const char *input);

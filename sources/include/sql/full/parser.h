#pragma once

#include <stddef.h>
#include <sql/full/ast.h>
#include <sql/full/pg_list.h>

typedef void (*od_sql_full_error_cb_t)(const char *msg, void *userdata);

List *od_sql_full_parse(const char *input, size_t input_len,
                        od_sql_full_error_cb_t error_cb,
                        void *userdata);

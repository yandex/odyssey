#pragma once

#include <stddef.h>
#include <stdint.h>
#include <setjmp.h>

#include <sql/full/scanner.h>
#include <sql/full/gramparse.h>

typedef void (*od_sql_full_error_cb_t)(const char *msg, void *userdata);

typedef struct od_sql_full_parse_ctx {
    /* Scanner/grammar state */
    base_yy_extra_type yyextra;
    core_yyscan_t yyscanner;

    /* Error handling */
    od_sql_full_error_cb_t error_cb;
    void *error_cb_userdata;
    int had_error;

    /* longjmp recovery for YY_FATAL_ERROR */
    jmp_buf fatal_jmp;
    int fatal_set;
} od_sql_full_parse_ctx_t;

void od_sql_full_parse_ctx_init(od_sql_full_parse_ctx_t *ctx,
                                const char *input, size_t input_len,
                                od_sql_full_error_cb_t error_cb,
                                void *userdata);

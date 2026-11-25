#pragma once

/*
 * Odyssey.
 *
 * Scalable PostgreSQL connection pooler.
 */

#include <stdbool.h>
#include <cfg/ctx.h>

typedef struct od_cfg_structured od_cfg_structured_t;
struct fy_node;

int od_cfg_parse_structured(od_cfg_parse_ctx_t *ctx, bool json);
int od_cfg_structured_map(od_cfg_structured_t *s, struct fy_node *root);

int od_cfg_structured_error(od_cfg_structured_t *s, struct fy_node *node,
			    const char *message);
int od_cfg_check_node(od_cfg_structured_t *s, struct fy_node *node);
int od_cfg_emit(od_cfg_structured_t *s, struct fy_node *node, int token);
int od_cfg_emit_default(od_cfg_structured_t *s, struct fy_node *node);
int od_cfg_emit_string(od_cfg_structured_t *s, struct fy_node *node);
int od_cfg_emit_value(od_cfg_structured_t *s, int directive,
		      struct fy_node *node);

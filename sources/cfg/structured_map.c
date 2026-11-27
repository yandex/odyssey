/*
 * Odyssey.
 *
 * Scalable PostgreSQL connection pooler.
 */

#include <odyssey.h>

#include <libfyaml.h>
#include <cfg/keywords.h>
#include <cfg/parse.tab.h>
#include <cfg/structured.h>

/* The native grammar validates scalar settings and builds the model. Only
 * the fields whose representation depends on the collection shape are here.
 */
typedef enum {
	SCALAR,
	STRING_OR_LIST,
	COMMON_NAMES,
	SECTION,
	SECTION_LIST,
	STRING_MAP,
	STRING_LIST
} od_cfg_shape_t;

static const struct {
	int parent;
	int token;
	od_cfg_shape_t shape;
} collections[] = {
	{ 0, LISTEN, SECTION_LIST },
	{ 0, STORAGE, SECTION_LIST },
	{ 0, DATABASE, SECTION_LIST },
	{ 0, LDAP_ENDPOINT, SECTION_LIST },
	{ 0, SHARED_POOL, SECTION_LIST },
	{ 0, CONN_DROP_OPTIONS, SECTION },
	{ 0, ONLINE_RESTART_DROP_OPTIONS, SECTION },
	{ 0, QUERY_PARSING_STANDBY_FUNCTION_LIST, STRING_LIST },
	{ 0, INCLUDE, STRING_OR_LIST },
	{ DATABASE, USER, SECTION_LIST },
	{ DATABASE, GROUP, SECTION_LIST },
	{ STORAGE, WATCHDOG, SECTION },
	{ STORAGE, BALANCING, SECTION },
	{ LISTEN, STORAGE, STRING_OR_LIST },
	{ LISTEN, BALANCING, SECTION },
	{ BALANCING, METHOD, SECTION },
	{ USER, LDAP_STORAGE_CREDENTIALS, SECTION_LIST },
	{ USER, OPTIONS, STRING_MAP },
	{ USER, BACKEND_STARTUP_OPTIONS, STRING_MAP },
	{ USER, AUTH_COMMON_NAME, COMMON_NAMES },
};

static od_cfg_shape_t field_shape(int parent, int token)
{
	/* All three blocks contain the grammar's route_items. */
	if (parent == GROUP || parent == WATCHDOG) {
		parent = USER;
	}
	for (size_t i = 0; i < sizeof(collections) / sizeof(collections[0]);
	     i++) {
		if (collections[i].parent == parent &&
		    collections[i].token == token) {
			return collections[i].shape;
		}
	}
	return SCALAR;
}

static int node_keyword(struct fy_node *node)
{
	size_t len;
	const char *text = fy_node_get_scalar(node, &len);
	return text == NULL ? 0 : od_cfg_keyword_lookup(text, len);
}

static bool section_has_name(int token)
{
	return token == DATABASE || token == USER || token == GROUP ||
	       token == STORAGE || token == SHARED_POOL ||
	       token == LDAP_ENDPOINT || token == LDAP_STORAGE_CREDENTIALS ||
	       token == METHOD;
}

typedef struct {
	struct fy_node_pair *name;
	struct fy_node_pair *fallback;
	struct fy_node_pair *address;
	struct fy_node_pair *connection_type;
} od_cfg_header_t;

static int section_header(od_cfg_structured_t *s, struct fy_node *map,
			  int token, od_cfg_header_t *header)
{
	if (!section_has_name(token)) {
		return 0;
	}

	header->name =
		fy_node_mapping_lookup_pair_by_simple_key(map, "name", 4);
	if (token == DATABASE || token == USER) {
		header->fallback = fy_node_mapping_lookup_pair_by_simple_key(
			map, "default", 7);
		if ((header->name != NULL) == (header->fallback != NULL)) {
			return od_cfg_structured_error(
				s, map,
				"expected exactly one of name or default: true");
		}
	} else if (header->name == NULL) {
		return od_cfg_structured_error(s, map,
					       "section requires a name");
	}

	if (header->fallback != NULL) {
		if (od_cfg_emit_default(
			    s, fy_node_pair_value(header->fallback)) != 0) {
			return -1;
		}
	} else if (od_cfg_emit_string(s, fy_node_pair_value(header->name)) !=
		   0) {
		return -1;
	}

	if (token != USER && token != GROUP) {
		return 0;
	}
	header->address =
		fy_node_mapping_lookup_pair_by_simple_key(map, "address", 7);
	if (header->address != NULL &&
	    od_cfg_emit_string(s, fy_node_pair_value(header->address)) != 0) {
		return -1;
	}
	header->connection_type = fy_node_mapping_lookup_pair_by_simple_key(
		map, "connection_type", 15);
	if (header->connection_type != NULL) {
		struct fy_node *value =
			fy_node_pair_value(header->connection_type);
		if (od_cfg_check_node(s, value) != 0) {
			return -1;
		}
		int type = node_keyword(value);
		if (type != DEFAULT && type != HOST && type != HOSTSSL &&
		    type != HOSTNOSSL && type != LOCAL) {
			return od_cfg_structured_error(
				s, value, "unknown route connection_type");
		}
		return od_cfg_emit(s, value, type);
	}
	return 0;
}

static int emit_fields(od_cfg_structured_t *s, struct fy_node *map, int parent,
		       const od_cfg_header_t *header);

static int emit_section(od_cfg_structured_t *s, struct fy_node *key, int token,
			struct fy_node *value)
{
	if (!fy_node_is_mapping(value)) {
		return od_cfg_structured_error(s, value,
					       "expected a section mapping");
	}
	/* Attribute a repeated section to this entry, even when several
	 * entries share the same outer directive key. */
	void *iter = NULL;
	struct fy_node_pair *first = fy_node_mapping_iterate(value, &iter);
	struct fy_node *start = first ? fy_node_pair_key(first) : key;
	od_cfg_header_t header = { 0 };
	if (od_cfg_emit(s, start, token) != 0 ||
	    section_header(s, value, token, &header) != 0 ||
	    od_cfg_emit(s, value, '{') != 0 ||
	    emit_fields(s, value, token, &header) != 0) {
		return -1;
	}
	return od_cfg_emit(s, value, '}');
}

static int emit_common_name(od_cfg_structured_t *s, struct fy_node *node)
{
	if (!fy_node_is_mapping(node)) {
		return od_cfg_emit_string(s, node);
	}
	struct fy_node_pair *fallback =
		fy_node_mapping_lookup_pair_by_simple_key(node, "default", 7);
	if (fallback == NULL || fy_node_mapping_item_count(node) != 1) {
		return od_cfg_structured_error(
			s, node, "expected a common name or {default: true}");
	}
	if (od_cfg_check_node(s, fy_node_pair_key(fallback)) != 0) {
		return -1;
	}
	return od_cfg_emit_default(s, fy_node_pair_value(fallback));
}

static int emit_string_map(od_cfg_structured_t *s, struct fy_node *map)
{
	if (!fy_node_is_mapping(map)) {
		return od_cfg_structured_error(
			s, map, "expected a mapping of string options");
	}
	void *iter = NULL;
	struct fy_node_pair *pair;
	while ((pair = fy_node_mapping_iterate(map, &iter)) != NULL) {
		if (od_cfg_emit_string(s, fy_node_pair_key(pair)) != 0 ||
		    od_cfg_emit_string(s, fy_node_pair_value(pair)) != 0) {
			return -1;
		}
	}
	return 0;
}

static int emit_string_list(od_cfg_structured_t *s, struct fy_node *seq)
{
	if (!fy_node_is_sequence(seq)) {
		return od_cfg_structured_error(
			s, seq, "expected a sequence of strings");
	}
	void *iter = NULL;
	struct fy_node *item;
	while ((item = fy_node_sequence_iterate(seq, &iter)) != NULL) {
		if (od_cfg_emit_string(s, item) != 0) {
			return -1;
		}
	}
	return 0;
}

static int emit_item(od_cfg_structured_t *s, struct fy_node *key, int token,
		     struct fy_node *value, od_cfg_shape_t shape)
{
	if (shape == SECTION || shape == SECTION_LIST) {
		return emit_section(s, key, token, value);
	}
	if (od_cfg_emit(s, key, token) != 0) {
		return -1;
	}
	switch (shape) {
	case COMMON_NAMES:
		return emit_common_name(s, value);
	case STRING_OR_LIST:
		return od_cfg_emit_string(s, value);
	case STRING_MAP:
	case STRING_LIST:
		if (od_cfg_emit(s, value, '{') != 0) {
			return -1;
		}
		if (shape == STRING_MAP ? emit_string_map(s, value) != 0 :
					  emit_string_list(s, value) != 0) {
			return -1;
		}
		return od_cfg_emit(s, value, '}');
	default:
		if (!fy_node_is_scalar(value)) {
			return od_cfg_structured_error(
				s, value, "expected a scalar setting");
		}
		return od_cfg_emit_value(s, token, value);
	}
}

static int emit_field(od_cfg_structured_t *s, int parent, struct fy_node *key,
		      struct fy_node *value)
{
	int token = node_keyword(key);
	const char *error = NULL;
	if (token == 0) {
		error = "unknown configuration key";
	} else if (parent != GROUP &&
		   (token == GROUP_QUERY || token == GROUP_QUERY_USER ||
		    token == GROUP_QUERY_DB)) {
		error = "only group sections support";
	}
	if (error != NULL) {
		size_t len;
		const char *name = fy_node_get_scalar(key, &len);
		char message[160];
		snprintf(message, sizeof(message), "%s '%.*s'", error,
			 (int)(len < 100 ? len : 100), name);
		return od_cfg_structured_error(s, key, message);
	}
	od_cfg_shape_t shape = field_shape(parent, token);
	if (shape == SECTION_LIST && !fy_node_is_sequence(value)) {
		return od_cfg_structured_error(
			s, value, "expected a sequence of section mappings");
	}
	if ((shape == SECTION_LIST || shape == STRING_OR_LIST ||
	     shape == COMMON_NAMES) &&
	    fy_node_is_sequence(value)) {
		void *iter = NULL;
		struct fy_node *item;
		while ((item = fy_node_sequence_iterate(value, &iter)) !=
		       NULL) {
			if (od_cfg_check_node(s, item) != 0 ||
			    emit_item(s, key, token, item, shape) != 0) {
				return -1;
			}
		}
		return 0;
	}
	return emit_item(s, key, token, value, shape);
}

static int emit_fields(od_cfg_structured_t *s, struct fy_node *map, int parent,
		       const od_cfg_header_t *header)
{
	void *iter = NULL;
	struct fy_node_pair *pair;
	while ((pair = fy_node_mapping_iterate(map, &iter)) != NULL) {
		struct fy_node *key = fy_node_pair_key(pair);
		struct fy_node *value = fy_node_pair_value(pair);
		if (od_cfg_check_node(s, key) != 0 ||
		    od_cfg_check_node(s, value) != 0) {
			return -1;
		}
		if (!fy_node_is_scalar(key)) {
			return od_cfg_structured_error(
				s, key, "expected a scalar configuration key");
		}
		if (pair == header->name || pair == header->fallback ||
		    pair == header->address ||
		    pair == header->connection_type) {
			continue;
		}
		if (emit_field(s, parent, key, value) != 0) {
			return -1;
		}
	}
	return 0;
}

int od_cfg_structured_map(od_cfg_structured_t *s, struct fy_node *root)
{
	if (od_cfg_check_node(s, root) != 0) {
		return -1;
	}
	if (!fy_node_is_mapping(root)) {
		return od_cfg_structured_error(
			s, root, "expected a configuration mapping");
	}
	od_cfg_header_t header = { 0 };
	return emit_fields(s, root, 0, &header);
}

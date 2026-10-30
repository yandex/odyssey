
/*
 * Odyssey.
 *
 * Scalable PostgreSQL connection pooler.
 */

#include <odyssey.h>
#include <wait_event.h>

od_wait_counter_t od_wait_counters[OD_WAIT_COUNT];

typedef struct {
	od_wait_event_t event;
	od_wait_class_t class;
	const char *name;
} od_wait_event_def_t;

static const od_wait_event_def_t od_wait_event_defs[] = {
#define OD_WAIT_EVENT(id, class, name) \
	{ OD_WAIT_##id, OD_WAIT_CLASS_##class, name },
	OD_WAIT_EVENT_LIST(OD_WAIT_EVENT)
#undef OD_WAIT_EVENT
};

static const char *od_wait_class_names[] = {
#define OD_WAIT_CLASS(id, name) name,
	OD_WAIT_CLASS_LIST(OD_WAIT_CLASS)
#undef OD_WAIT_CLASS
};

static_assert(sizeof(od_wait_event_defs) / sizeof(od_wait_event_defs[0]) ==
		      OD_WAIT_COUNT,
	      "wait event definitions must cover the whole enum");

static_assert(sizeof(od_wait_class_names) / sizeof(od_wait_class_names[0]) ==
		      OD_WAIT_CLASS_COUNT,
	      "wait class names must cover the whole enum");

const char *od_wait_event_name(od_wait_event_t event)
{
	if (event >= OD_WAIT_COUNT) {
		return "UNKNOWN";
	}
	return od_wait_event_defs[event].name;
}

od_wait_class_t od_wait_event_class(od_wait_event_t event)
{
	if (event >= OD_WAIT_COUNT) {
		return OD_WAIT_CLASS_NONE;
	}
	return od_wait_event_defs[event].class;
}

const char *od_wait_class_name(od_wait_class_t class)
{
	if (class >= OD_WAIT_CLASS_COUNT) {
		return "Unknown";
	}
	return od_wait_class_names[class];
}

void od_wait_counters_reset(void)
{
	for (int i = 0; i < OD_WAIT_COUNT; i++) {
		atomic_store_explicit(&od_wait_counters[i].counter, 0,
				      memory_order_relaxed);
	}
}

#pragma once

/*
 * Odyssey.
 *
 * Scalable PostgreSQL connection pooler.
 */

typedef enum {
	OD_MSG_STAT,
	OD_MSG_CLIENT_NEW,
	OD_MSG_LOG,
	OD_MSG_SHUTDOWN,
} od_msg_t;

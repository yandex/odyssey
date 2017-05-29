#ifndef OD_MSG_H
#define OD_MSG_H

typedef enum {
	OD_MCLIENT_NEW,
	OD_MROUTER_ROUTE,
	OD_MROUTER_ATTACH,
	OD_MROUTER_DETACH
} od_msg_t;

#endif /* OD_MSG_H */

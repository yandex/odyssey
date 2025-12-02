
/*
 * Odyssey.
 *
 * Scalable PostgreSQL connection pooler.
 */

#include <machinarium.h>
#include <odyssey.h>

od_retcode_t od_conn_eject_info_init(od_conn_eject_info **info)
{
	*info = (od_conn_eject_info *)od_malloc(sizeof(od_conn_eject_info));
	if (*info == NULL) {
		/* TODO: set errno properly */

		return NOT_OK_RESPONSE;
	}
	(*info)->last_conn_drop_ts = -1;
	pthread_mutex_init(&(*info)->mu, NULL);

	return OK_RESPONSE;
}

od_retcode_t od_conn_eject_info_free(od_conn_eject_info *info)
{
	pthread_mutex_destroy(&info->mu);
	od_free(info);

	return OK_RESPONSE;
}

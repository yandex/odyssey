/*
 * Odyssey.
 *
 * Scalable PostgreSQL connection pooler.
 */

#include <kiwi.h>
#include <machinarium.h>
#include <odyssey.h>

od_retcode_t od_thread_global_init(od_thread_global **gl)
{
	*gl = od_malloc(sizeof(od_thread_global));
	if (*gl == NULL) {
		return NOT_OK_RESPONSE;
	}

	od_conn_eject_info_init(&(*gl)->info);

	return OK_RESPONSE;
}

od_thread_global **od_thread_global_get(void)
{
	return (od_thread_global **)machine_thread_private();
}

od_retcode_t od_thread_global_free(od_thread_global *gl)
{
	od_retcode_t rc = od_conn_eject_info_free(gl->info);

	if (rc != OK_RESPONSE) {
		return rc;
	}

	od_free(gl);

	return OK_RESPONSE;
}

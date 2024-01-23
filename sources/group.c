/*
 * Odyssey.
 *
 * Scalable PostgreSQL connection pooler.
 */

#include <kiwi.h>
#include <machinarium.h>
#include <odyssey.h>

int od_group_free(od_group_t *group)
{
	if (group == NULL)
		return NOT_OK_RESPONSE;

	if (group->route_usr)
		free(group->route_usr);
	if (group->route_db)
		free(group->route_db);
	if (group->storage_user)
		free(group->storage_user);
	if (group->storage_db)
		free(group->storage_db);
	if (group->group_name)
		free(group->group_name);
	if (group->group_query)
		free(group->group_query);

	free(group);
	return OK_RESPONSE;
}

void od_group_qry_format(char *qry, char *fmt, ...) {
	va_list args;
	va_start(args, fmt);
	int len = od_vsnprintf(qry, OD_QRY_MAX_SZ, fmt, args);
	va_end(args);

	/* dirty hack */
	qry[len] = '\0';
}

int od_group_parse_val_datarow(machine_msg_t *msg, int *is_group_member)
{
	char *pos = (char *)machine_msg_data(msg) + 1;
	uint32_t pos_size = machine_msg_size(msg) - 1;

	/* size */
	uint32_t size;
	int rc;
	rc = kiwi_read32(&size, &pos, &pos_size);
	if (kiwi_unlikely(rc == -1))
		goto error;
	/* count */
	uint16_t count;
	rc = kiwi_read16(&count, &pos, &pos_size);

	if (kiwi_unlikely(rc == -1))
		goto error;

	if (count != 1)
		goto error;

	/* (not used) */
	uint32_t val_len;
	rc = kiwi_read32(&val_len, &pos, &pos_size);
	if (kiwi_unlikely(rc == -1)) {
		goto error;
	}

	if (strcmp(pos, "f") == 0) {
		*is_group_member = 0;
	} else if (strcmp(pos, "t") == 0) {
		*is_group_member = 1;
	} else {
		goto error;
	}

	return OK_RESPONSE;
error:
	return NOT_OK_RESPONSE;
}

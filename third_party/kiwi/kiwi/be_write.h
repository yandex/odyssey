#ifndef KIWI_BE_WRITE_H
#define KIWI_BE_WRITE_H

/*
 * kiwi.
 *
 * postgreSQL protocol interaction library.
*/

KIWI_API static inline machine_msg_t*
kiwi_be_write_error_as(char *severity, char *code,
                       char *detail, int detail_len,
                       char *hint, int hint_len,
                       char *message, int len)
{
	int size = 1 /* S */ + 6 +
	           1 /* C */ + 6 +
	           1 /* M */ + len + 1 +
	           1 /* zero */;
	if (detail && detail_len > 0)
		size += 1 + /* D */ + detail_len + 1;
	if (hint && hint_len > 0)
		size += 1 + /* H */ + hint_len + 1;
	machine_msg_t *msg;
	msg = machine_msg_create(sizeof(kiwi_header_t) + size);
	if (kiwi_unlikely(msg == NULL))
		return NULL;
	char *pos;
	pos = machine_msg_get_data(msg);
	kiwi_write8(&pos, KIWI_BE_ERROR_RESPONSE);
	kiwi_write32(&pos, sizeof(uint32_t) + size);
	kiwi_write8(&pos, 'S');
	kiwi_write(&pos, severity, 6);
	kiwi_write8(&pos, 'C');
	kiwi_write(&pos, code, 6);
	if (detail && detail_len > 0) {
		kiwi_write8(&pos, 'D');
		kiwi_write(&pos, detail, detail_len);
		kiwi_write8(&pos, 0);
	}
	if (hint && hint_len > 0) {
		kiwi_write8(&pos, 'H');
		kiwi_write(&pos, hint, hint_len);
		kiwi_write8(&pos, 0);
	}
	kiwi_write8(&pos, 'M');
	kiwi_write(&pos, message, len);
	kiwi_write8(&pos, 0);
	kiwi_write8(&pos, 0);
	return msg;
}

KIWI_API static inline machine_msg_t*
kiwi_be_write_error(char *code, char *message, int len)
{
	return kiwi_be_write_error_as("ERROR", code, NULL, 0, NULL, 0, message, len);
}

KIWI_API static inline machine_msg_t*
kiwi_be_write_error_fatal(char *code, char *message, int len)
{
	return kiwi_be_write_error_as("FATAL", code, NULL, 0, NULL, 0, message, len);
}

KIWI_API static inline machine_msg_t*
kiwi_be_write_error_panic(char *code, char *message, int len)
{
	return kiwi_be_write_error_as("PANIC", code, NULL, 0, NULL, 0, message, len);
}

KIWI_API static inline machine_msg_t*
kiwi_be_write_notice(char *message, int len)
{
	int size = sizeof(kiwi_header_t) + len + 1;
	machine_msg_t *msg;
	msg = machine_msg_create(size);
	if (kiwi_unlikely(msg == NULL))
		return NULL;
	char *pos;
	pos = machine_msg_get_data(msg);
	kiwi_write8(&pos, KIWI_BE_NOTICE_RESPONSE);
	kiwi_write32(&pos, sizeof(uint32_t) + len);
	kiwi_write(&pos, message, len);
	kiwi_write8(&pos, 0);
	return msg;
}

KIWI_API static inline machine_msg_t*
kiwi_be_write_authentication_ok(void)
{
	int size = sizeof(kiwi_header_t) + sizeof(uint32_t);
	machine_msg_t *msg;
	msg = machine_msg_create(size);
	if (kiwi_unlikely(msg == NULL))
		return NULL;
	char *pos;
	pos = machine_msg_get_data(msg);
	kiwi_write8(&pos, KIWI_BE_AUTHENTICATION);
	kiwi_write32(&pos, sizeof(uint32_t) + sizeof(uint32_t));
	kiwi_write32(&pos, 0);
	return msg;
}

KIWI_API static inline machine_msg_t*
kiwi_be_write_authentication_clear_text(void)
{
	int size = sizeof(kiwi_header_t) + sizeof(uint32_t);
	machine_msg_t *msg;
	msg = machine_msg_create(size);
	if (kiwi_unlikely(msg == NULL))
		return NULL;
	char *pos;
	pos = machine_msg_get_data(msg);
	kiwi_write8(&pos, KIWI_BE_AUTHENTICATION);
	kiwi_write32(&pos, sizeof(uint32_t) + sizeof(uint32_t));
	kiwi_write32(&pos, 3);
	return msg;
}

KIWI_API static inline machine_msg_t*
kiwi_be_write_authentication_md5(char salt[4])
{
	int size = sizeof(kiwi_header_t) + sizeof(uint32_t) + 4;
	machine_msg_t *msg;
	msg = machine_msg_create(size);
	if (kiwi_unlikely(msg == NULL))
		return NULL;
	char *pos;
	pos = machine_msg_get_data(msg);
	kiwi_write8(&pos, KIWI_BE_AUTHENTICATION);
	kiwi_write32(&pos, sizeof(uint32_t) + sizeof(uint32_t) + 4);
	kiwi_write32(&pos, 5);
	kiwi_write(&pos, salt, 4);
	return msg;
}

KIWI_API static inline machine_msg_t*
kiwi_be_write_authentication_sasl(char *method, int method_len)
{
	int size = sizeof(kiwi_header_t) + sizeof(uint32_t) + method_len;
	machine_msg_t *msg;
	msg = machine_msg_create(size);
	if (kiwi_unlikely(msg == NULL))
		return NULL;
	char *pos;
	pos = machine_msg_get_data(msg);
	kiwi_write8(&pos, KIWI_BE_AUTHENTICATION);
	kiwi_write32(&pos, sizeof(uint32_t) + sizeof(uint32_t) + method_len);
	kiwi_write32(&pos, 10);
	kiwi_write(&pos, method, method_len);
	return msg;
}

KIWI_API static inline machine_msg_t*
kiwi_be_write_authentication_sasl_continue(char *data, int data_len)
{
	int size = sizeof(kiwi_header_t) + sizeof(uint32_t) + data_len;
	machine_msg_t *msg;
	msg = machine_msg_create(size);
	if (kiwi_unlikely(msg == NULL))
		return NULL;
	char *pos;
	pos = machine_msg_get_data(msg);
	kiwi_write8(&pos, KIWI_BE_AUTHENTICATION);
	kiwi_write32(&pos, sizeof(uint32_t) + sizeof(uint32_t) + data_len);
	kiwi_write32(&pos, 11);
	kiwi_write(&pos, data, data_len);
	return msg;
}

KIWI_API static inline machine_msg_t*
kiwi_be_write_authentication_sasl_final(char *data, int data_len)
{
	int size = sizeof(kiwi_header_t) + sizeof(uint32_t) + data_len;
	machine_msg_t *msg;
	msg = machine_msg_create(size);
	if (kiwi_unlikely(msg == NULL))
		return NULL;
	char *pos;
	pos = machine_msg_get_data(msg);
	kiwi_write8(&pos, KIWI_BE_AUTHENTICATION);
	kiwi_write32(&pos, sizeof(uint32_t) + sizeof(uint32_t) + data_len);
	kiwi_write32(&pos, 12);
	kiwi_write(&pos, data, data_len);
	return msg;
}

KIWI_API static inline machine_msg_t*
kiwi_be_write_backend_key_data(uint32_t pid, uint32_t key)
{
	int size = sizeof(kiwi_header_t) + sizeof(uint32_t) + sizeof(uint32_t);
	machine_msg_t *msg;
	msg = machine_msg_create(size);
	if (kiwi_unlikely(msg == NULL))
		return NULL;
	char *pos;
	pos = machine_msg_get_data(msg);
	kiwi_write8(&pos, KIWI_BE_BACKEND_KEY_DATA);
	kiwi_write32(&pos, sizeof(uint32_t) + sizeof(uint32_t) + sizeof(uint32_t));
	kiwi_write32(&pos, pid);
	kiwi_write32(&pos, key);
	return msg;
}

KIWI_API static inline machine_msg_t*
kiwi_be_write_parameter_status(char *key, int key_len, char *value, int value_len)
{
	int size = sizeof(kiwi_header_t) + key_len + value_len;
	machine_msg_t *msg;
	msg = machine_msg_create(size);
	if (kiwi_unlikely(msg == NULL))
		return NULL;
	char *pos;
	pos = machine_msg_get_data(msg);
	kiwi_write8(&pos, KIWI_BE_PARAMETER_STATUS);
	kiwi_write32(&pos, sizeof(uint32_t) + key_len + value_len);
	kiwi_write(&pos, key, key_len);
	kiwi_write(&pos, value, value_len);
	return msg;
}

KIWI_API static inline machine_msg_t*
kiwi_be_write_ready(uint8_t status)
{
	int size = sizeof(kiwi_header_t) + sizeof(uint8_t);
	machine_msg_t *msg;
	msg = machine_msg_create(size);
	if (kiwi_unlikely(msg == NULL))
		return NULL;
	char *pos;
	pos = machine_msg_get_data(msg);
	kiwi_write8(&pos, KIWI_BE_READY_FOR_QUERY);
	kiwi_write32(&pos, sizeof(uint32_t) + sizeof(uint8_t));
	kiwi_write8(&pos, status);
	return msg;
}

KIWI_API static inline machine_msg_t*
kiwi_be_write_complete(char *message, int len)
{
	int size = sizeof(kiwi_header_t) + len;
	machine_msg_t *msg;
	msg = machine_msg_create(size);
	if (kiwi_unlikely(msg == NULL))
		return NULL;
	char *pos;
	pos = machine_msg_get_data(msg);
	kiwi_write8(&pos, KIWI_BE_COMMAND_COMPLETE);
	kiwi_write32(&pos, sizeof(uint32_t) + len);
	kiwi_write(&pos, message, len);
	return msg;
}

KIWI_API static inline machine_msg_t*
kiwi_be_write_empty_query(void)
{
	int size = sizeof(kiwi_header_t);
	machine_msg_t *msg;
	msg = machine_msg_create(size);
	if (kiwi_unlikely(msg == NULL))
		return NULL;
	char *pos;
	pos = machine_msg_get_data(msg);
	kiwi_write8(&pos, KIWI_BE_EMPTY_QUERY_RESPONSE);
	kiwi_write32(&pos, sizeof(uint32_t));
	return msg;
}

KIWI_API static inline machine_msg_t*
kiwi_be_write_parse_complete(void)
{
	int size = sizeof(kiwi_header_t);
	machine_msg_t *msg;
	msg = machine_msg_create(size);
	if (kiwi_unlikely(msg == NULL))
		return NULL;
	char *pos;
	pos = machine_msg_get_data(msg);
	kiwi_write8(&pos, KIWI_BE_PARSE_COMPLETE);
	kiwi_write32(&pos, sizeof(uint32_t));
	return msg;
}

KIWI_API static inline machine_msg_t*
kiwi_be_write_bind_complete(void)
{
	int size = sizeof(kiwi_header_t);
	machine_msg_t *msg;
	msg = machine_msg_create(size);
	if (kiwi_unlikely(msg == NULL))
		return NULL;
	char *pos;
	pos = machine_msg_get_data(msg);
	kiwi_write8(&pos, KIWI_BE_BIND_COMPLETE);
	kiwi_write32(&pos, sizeof(uint32_t));
	return 0;
}

KIWI_API static inline machine_msg_t*
kiwi_be_write_portal_suspended(void)
{
	int size = sizeof(kiwi_header_t);
	machine_msg_t *msg;
	msg = machine_msg_create(size);
	if (kiwi_unlikely(msg == NULL))
		return NULL;
	char *pos;
	pos = machine_msg_get_data(msg);
	kiwi_write8(&pos, KIWI_BE_PORTAL_SUSPENDED);
	kiwi_write32(&pos, sizeof(uint32_t));
	return msg;
}

KIWI_API static inline machine_msg_t*
kiwi_be_write_no_data(void)
{
	int size = sizeof(kiwi_header_t);
	machine_msg_t *msg;
	msg = machine_msg_create(size);
	if (kiwi_unlikely(msg == NULL))
		return NULL;
	char *pos;
	pos = machine_msg_get_data(msg);
	kiwi_write8(&pos, KIWI_BE_NO_DATA);
	kiwi_write32(&pos, sizeof(uint32_t));
	return msg;
}

KIWI_API static inline machine_msg_t*
kiwi_be_write_row_description(void)
{
	int size = sizeof(kiwi_header_t) + sizeof(uint16_t);
	machine_msg_t *msg;
	msg = machine_msg_create(size);
	if (kiwi_unlikely(msg == NULL))
		return NULL;
	char *pos;
	pos = machine_msg_get_data(msg);
	kiwi_write8(&pos, KIWI_BE_ROW_DESCRIPTION);
	kiwi_write32(&pos, sizeof(uint32_t) + sizeof(uint16_t));
	kiwi_write16(&pos, 0);
	return msg;
}

KIWI_API static inline int
kiwi_be_write_row_description_add(machine_msg_t *msg,
                                  char *name, int name_len,
                                  int32_t table_id,
                                  int16_t attrnum,
                                  int32_t type_id,
                                  int16_t type_size,
                                  int32_t type_modifier,
                                  int32_t format_code)
{
	int size = name_len + 1 +
	           sizeof(uint32_t) /* table_id */+
	           sizeof(uint16_t) /* attrnum */ +
	           sizeof(uint32_t) /* type_id */ +
	           sizeof(uint16_t) /* type_size */ +
	           sizeof(uint32_t) /* type_modifier */ +
	           sizeof(uint16_t) /* format_code */;

	int size_written = machine_msg_get_size(msg);
	int rc;
	rc = machine_msg_write(msg, NULL, size);
	if (kiwi_unlikely(rc == -1))
		return -1;
	char *pos;
	pos = (char*)machine_msg_get_data(msg) + size_written;
	kiwi_write(&pos, name, name_len);
	kiwi_write8(&pos, 0);
	kiwi_write32(&pos, table_id);
	kiwi_write16(&pos, attrnum);
	kiwi_write32(&pos, type_id);
	kiwi_write16(&pos, type_size);
	kiwi_write32(&pos, type_modifier);
	kiwi_write16(&pos, format_code);

	kiwi_header_t *header;
	header = machine_msg_get_data(msg);
	uint32_t pos_size = sizeof(uint32_t) + sizeof(uint16_t);
	pos = (char*)&header->len;
	uint32_t total_size;
	uint16_t count;
	kiwi_read32(&total_size, &pos, &pos_size);
	kiwi_read16(&count, &pos, &pos_size);
	total_size += size;
	count++;
	kiwi_write32to((char*)&header->len, total_size);
	kiwi_write16to((char*)&header->len + sizeof(uint32_t), count);
	return 0;
}

KIWI_API static inline machine_msg_t*
kiwi_be_write_row_descriptionf(char *fmt, ...)
{
	machine_msg_t *msg;
	msg = kiwi_be_write_row_description();
	if (kiwi_unlikely(msg == NULL))
		return NULL;

	va_list args;
	va_start(args, fmt);
	while (*fmt) {
		char *name = va_arg(args, char*);
		int   name_len = strlen(name);
		int rc;
		switch (*fmt) {
		case 's':
			rc = kiwi_be_write_row_description_add(msg, name, name_len,
			                                       0, 0, 25 /* TEXTOID */, -1, 0, 0);
			break;
		case 'd':
			rc = kiwi_be_write_row_description_add(msg, name, name_len,
			                                       0, 0, 23 /* INT4OID */, 4, 0, 0);
			break;
		case 'l':
			rc = kiwi_be_write_row_description_add(msg, name, name_len,
			                                       0, 0, 20 /* INT8OID */, 8, 0, 0);
			break;
		}
		if (rc == -1) {
			machine_msg_free(msg);
			return NULL;
		}
		fmt++;
	}
	va_end(args);
	return msg;
}

KIWI_API static inline machine_msg_t*
kiwi_be_write_data_row(void)
{
	int size = sizeof(kiwi_header_t) + sizeof(uint16_t);
	machine_msg_t *msg;
	msg = machine_msg_create(size);
	if (kiwi_unlikely(msg == NULL))
		return NULL;
	char *pos;
	pos = machine_msg_get_data(msg);
	kiwi_write8(&pos, KIWI_BE_DATA_ROW);
	kiwi_write32(&pos, sizeof(uint32_t) + sizeof(uint16_t));
	kiwi_write16(&pos, 0);
	return msg;
}

KIWI_API static inline int
kiwi_be_write_data_row_add(machine_msg_t *msg, char *data, int32_t len)
{
	int is_null = len == -1;
	int size = sizeof(uint32_t) + (is_null ? 0 : len);

	int size_written = machine_msg_get_size(msg);
	int rc;
	rc = machine_msg_write(msg, NULL, size);
	if (kiwi_unlikely(rc == -1))
		return -1;
	char *pos;
	pos = (char*)machine_msg_get_data(msg) + size_written;
	kiwi_write32(&pos, len);
	if (! is_null)
		kiwi_write(&pos, data, len);

	kiwi_header_t *header;
	header = machine_msg_get_data(msg);
	uint32_t pos_size = sizeof(uint32_t) + sizeof(uint16_t);
	pos = (char*)&header->len;
	uint32_t total_size;
	uint16_t count;
	kiwi_read32(&total_size, &pos, &pos_size);
	kiwi_read16(&count, &pos, &pos_size);
	total_size += size;
	count++;
	kiwi_write32to((char*)&header->len, total_size);
	kiwi_write16to((char*)&header->len + sizeof(uint32_t), count);
	return 0;
}

#endif /* KIWI_BE_WRITE_H */

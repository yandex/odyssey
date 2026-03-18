/*
 * Odyssey.
 *
 * Scalable PostgreSQL connection pooler.
 */

#include <odyssey.h>
#include <string.h>
#include <pid.h>
#include <logger.h>
#include <tests/odyssey_test.h>

/* Test default value initialization */
void test_logger_default_max_msg_size(void)
{
	od_logger_t logger;
	od_pid_t pid;

	od_pid_init(&pid);
	od_logger_init(&logger, &pid);

	/* Default should be OD_LOGLINE_MAXLEN (1024) */
	test(logger.log_max_msg_size == OD_LOGLINE_MAXLEN);
	test(logger.log_max_msg_size == 1024);
}

/* Test that zero or negative values fallback to default */
void test_logger_max_msg_size_fallback(void)
{
	od_logger_t logger;
	od_pid_t pid;

	od_pid_init(&pid);
	od_logger_init(&logger, &pid);

	/* Zero should fallback to default */
	od_logger_set_max_msg_size(&logger, 0);
	test(logger.log_max_msg_size == OD_LOGLINE_MAXLEN);

	/* Negative should fallback to default */
	od_logger_set_max_msg_size(&logger, -100);
	test(logger.log_max_msg_size == OD_LOGLINE_MAXLEN);

	/* Positive value should be accepted */
	od_logger_set_max_msg_size(&logger, 2048);
	test(logger.log_max_msg_size == 2048);
}

/* Test that values above limit are capped */
void test_logger_max_msg_size_limit(void)
{
	od_logger_t logger;
	od_pid_t pid;

	od_pid_init(&pid);
	od_logger_init(&logger, &pid);

	/* Value above limit should be capped at OD_LOGLINE_MAXLEN_LIMIT */
	od_logger_set_max_msg_size(&logger, OD_LOGLINE_MAXLEN_LIMIT + 1000);
	test(logger.log_max_msg_size == OD_LOGLINE_MAXLEN_LIMIT);

	/* Exactly at limit should be accepted */
	od_logger_set_max_msg_size(&logger, OD_LOGLINE_MAXLEN_LIMIT);
	test(logger.log_max_msg_size == OD_LOGLINE_MAXLEN_LIMIT);

	/* Below limit should be accepted */
	od_logger_set_max_msg_size(&logger, OD_LOGLINE_MAXLEN_LIMIT - 1);
	test(logger.log_max_msg_size == OD_LOGLINE_MAXLEN_LIMIT - 1);
}

void odyssey_test_logger(void)
{
	odyssey_test(test_logger_default_max_msg_size);
	odyssey_test(test_logger_max_msg_size_fallback);
	odyssey_test(test_logger_max_msg_size_limit);
}

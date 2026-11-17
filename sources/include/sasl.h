#pragma once

typedef enum {
	/* generic parse failure of the client-first-message attributes or a
	 * truncated message */
	OD_SASL_ERROR_MALFORMED = -1,
	/* unexpected gs2-cbind-flag, missing comma or unsupported
	 * channel-binding type in the client-first-message */
	OD_SASL_ERROR_INVALID_CBIND_FLAG = -2,
	/* client requires channel binding ('p') which is not in use;
	 * PostgreSQL parity:
	 * https://github.com/postgres/postgres/blob/c0f2b6dc82718f17a2190f474450885df6c06402/src/backend/libpq/auth-scram.c#L1041-L1045
	 */
	OD_SASL_ERROR_CHANNEL_BINDING_REQUIRED = -3,
	/* client uses authorization identity (authzid); PostgreSQL parity:
	 * https://github.com/postgres/postgres/blob/c0f2b6dc82718f17a2190f474450885df6c06402/src/backend/libpq/auth-scram.c#L1070-L1073
	 */
	OD_SASL_ERROR_AUTH_IDENTITY = -4,
	/* client requires an unsupported mandatory extension; PostgreSQL
	 * parity:
	 * https://github.com/postgres/postgres/blob/c0f2b6dc82718f17a2190f474450885df6c06402/src/backend/libpq/auth-scram.c#L1092-L1094
	 */
	OD_SASL_ERROR_MANDATORY_EXT = -5,
	/* empty client-first-message; PostgreSQL parity:
	 * https://github.com/postgres/postgres/blob/c0f2b6dc82718f17a2190f474450885df6c06402/src/backend/libpq/auth-scram.c#L377-L381
	 */
	OD_SASL_ERROR_EMPTY_MESSAGE = -6,
	/*
	 * Client sent the 'y' channel-binding flag while SSL/TLS is in use
	 * on the server side. PostgreSQL parity:
	 * https://github.com/postgres/postgres/blob/c0f2b6dc82718f17a2190f474450885df6c06402/src/backend/libpq/auth-scram.c#L1020-L1023
	 */
	OD_SASL_ERROR_CHANNEL_BINDING_NEGOTIATION = -7,
	/*
	 * Client channel-binding data does not match the server certificate
	 * hash. PostgreSQL parity:
	 * https://github.com/postgres/postgres/blob/c0f2b6dc82718f17a2190f474450885df6c06402/src/backend/libpq/auth-scram.c#L1352-L1354
	 */
	OD_SASL_ERROR_CHANNEL_BINDING_MISMATCH = -8,
	/*
	 * Unexpected channel-binding attribute in the client-final-message
	 * while channel binding is not in use. PostgreSQL parity:
	 * https://github.com/postgres/postgres/blob/c0f2b6dc82718f17a2190f474450885df6c06402/src/backend/libpq/auth-scram.c#L1370-L1372
	 */
	OD_SASL_ERROR_UNEXPECTED_CBIND_ATTR = -9,
	/*
	 * Out of memory. Not a protocol error: the connection is dropped
	 * without an error response.
	 */
	OD_SASL_ERROR_OUT_OF_MEMORY = -10,
} od_sasl_error_t;

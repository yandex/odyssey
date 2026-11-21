#pragma once

typedef enum {
	OD_SASL_ERROR_AUTH_IDENTITY = -4,
	OD_SASL_ERROR_MANDATORY_EXT = -5,
	/*
	 * Client sent the 'y' channel-binding flag while SSL/TLS is in use
	 * on the server side. PostgreSQL parity:
	 * https://github.com/postgres/postgres/blob/REL_19_RC1/src/backend/libpq/auth-scram.c#L1020-L1023
	 */
	OD_SASL_ERROR_CHANNEL_BINDING_NEGOTIATION = -7,
	/*
	 * Client channel-binding data does not match the server certificate
	 * hash. PostgreSQL parity:
	 * https://github.com/postgres/postgres/blob/REL_19_RC1/src/backend/libpq/auth-scram.c#L1352-L1354
	 */
	OD_SASL_ERROR_CHANNEL_BINDING_MISMATCH = -8,
	/*
	 * Unexpected channel-binding attribute in the client-final-message
	 * while channel binding is not in use. PostgreSQL parity:
	 * https://github.com/postgres/postgres/blob/REL_19_RC1/src/backend/libpq/auth-scram.c#L1370-L1372
	 */
	OD_SASL_ERROR_UNEXPECTED_CBIND_ATTR = -9,
	/*
	 * Out of memory. Not a protocol error: the connection is dropped
	 * without an error response.
	 */
	OD_SASL_ERROR_OUT_OF_MEMORY = -10,
} od_sasl_error_t;

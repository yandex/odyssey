#pragma once

/*
 * Odyssey.
 *
 * Scalable PostgreSQL connection pooler.
 */

#include <od_memory.h>

#include <pg_compat.h>
#include <common/cryptohash.h>
#include <common/scram-common.h>

#define OD_SCRAM_MAX_KEY_LEN SCRAM_MAX_KEY_LEN
#define OD_SCRAM_SHA_256_DEFAULT_ITERATIONS SCRAM_SHA_256_DEFAULT_ITERATIONS

typedef struct od_scram_state od_scram_state_t;

struct od_scram_state {
	char *client_nonce;
	char *client_first_message;
	char *client_final_message;

	char *server_nonce;
	char *server_first_message;

	/* gs2-cbind-flag from the client-first-message ('n', 'y' or 'p') */
	char cbind_flag;

	uint8_t *salted_password;
	int iterations;
	char *salt;

	int use_passthrough_keys;
	uint8_t stored_key[OD_SCRAM_MAX_KEY_LEN];
	uint8_t server_key[OD_SCRAM_MAX_KEY_LEN];
	uint8_t client_key[OD_SCRAM_MAX_KEY_LEN];
};

static inline void od_scram_state_init(od_scram_state_t *state)
{
	memset(state, 0, sizeof(*state));
}

static inline void od_scram_state_free(od_scram_state_t *state)
{
	od_free(state->client_nonce);
	od_free(state->client_first_message);
	od_free(state->client_final_message);
	if (state->server_nonce) {
		od_free(state->server_nonce);
	}
	if (state->server_first_message) {
		od_free(state->server_first_message);
	}
	od_free(state->salted_password);
	od_free(state->salt);

	memset(state, 0, sizeof(*state));
}

machine_msg_t *
od_scram_create_client_first_message(od_scram_state_t *scram_state);

machine_msg_t *
od_scram_create_client_final_message(od_scram_state_t *scram_state,
				     char *password, char *auth_data,
				     size_t auth_data_size);

machine_msg_t *
od_scram_create_server_first_message(od_scram_state_t *scram_state);

machine_msg_t *
od_scram_create_server_final_message(od_scram_state_t *scram_state);

int od_scram_verify_server_signature(od_scram_state_t *scram_state,
				     char *auth_data, size_t auth_data_size);

int od_scram_verify_final_nonce(od_scram_state_t *scram_state,
				char *final_nonce, size_t final_nonce_size);

/*
 * Compare the client-provided channel binding value with the expected
 * base64-encoded value. The sizes must match exactly: PostgreSQL uses a
 * full strcmp() comparison, so trailing garbage is rejected.
 * PostgreSQL parity:
 * https://github.com/postgres/postgres/blob/c0f2b6dc82718f17a2190f474450885df6c06402/src/backend/libpq/auth-scram.c#L1351-L1354
 */
od_retcode_t od_scram_verify_channel_binding(const char *channel_binding,
					     size_t channel_binding_size,
					     const char *b64_message,
					     int b64_message_len);

int od_scram_verify_client_proof(od_scram_state_t *scram_state,
				 uint8_t *client_proof);

int od_scram_parse_verifier(od_scram_state_t *scram_state, char *verifier);

int od_scram_init_from_plain_password(od_scram_state_t *scram_state,
				      char *plain_password);

int od_scram_read_client_first_message(od_scram_state_t *scram_state,
				       char *auth_data, size_t auth_data_size,
				       int channel_binding_in_use,
				       int ssl_in_use);

int od_scram_read_client_final_message(mm_io_t *io,
				       od_scram_state_t *scram_state,
				       char *auth_data, size_t auth_data_size,
				       int channel_binding_in_use,
				       char **final_nonce_ptr,
				       size_t *final_nonce_size_ptr,
				       uint8_t **proof_ptr);

#ifndef ODYSSEY_SCRAM_H
#define ODYSSEY_SCRAM_H

/*
 * Odyssey.
 *
 * Scalable PostgreSQL connection pooler.
*/

typedef struct od_scram_state od_scram_state_t;

struct od_scram_state
{
	char *client_nonce;
	char *client_first_message;
	char *client_final_message;

	char *server_nonce;
	char *server_first_message;

	uint8_t	*salted_password;
	int iterations;
	char *salt;

	uint8_t stored_key[32];
	uint8_t server_key[32];
};

static inline void
od_scram_state_init(od_scram_state_t *state)
{
	memset(state, 0, sizeof(*state));
}

static inline void
od_scram_state_free(od_scram_state_t *state)
{
	free(state->client_nonce);
	free(state->client_first_message);
	free(state->client_final_message);
	free(state->server_nonce);
	free(state->server_first_message);
	free(state->salted_password);
	free(state->salt);

	memset(state, 0, sizeof(*state));
}

machine_msg_t*
od_scram_create_client_first_message(od_scram_state_t *scram_state);

machine_msg_t*
od_scram_create_client_final_message(od_scram_state_t *scram_state,
									 char *password, char *auth_data);

machine_msg_t*
od_scram_create_server_first_message(od_scram_state_t *scram_state);

machine_msg_t*
od_scram_create_server_final_message(od_scram_state_t *scram_state);


int
od_scram_verify_server_signature(od_scram_state_t *scram_state, char *auth_data);

int
od_scram_verify_final_nonce(od_scram_state_t *scram_state, char *final_nonce);

int
od_scram_verify_client_proof(od_scram_state_t *scram_state, char *client_proof);

int
od_scram_parse_verifier(od_scram_state_t *scram_state, char *verifier);

int
od_scram_init_from_plain_password(od_scram_state_t *scram_state, char *plain_password);

int
od_scram_read_client_first_message(od_scram_state_t *scram_state, char *auth_data);

int
od_scram_read_client_final_message(od_scram_state_t *scram_state, char *auth_data,
								   char **final_nonce_ptr, char **proof_ptr);

#endif /* ODYSSEY_SCRAM_H */

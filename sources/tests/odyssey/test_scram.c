#include <odyssey.h>

#include <machinarium/machinarium.h>
#include <machinarium/io.h>

#include <scram.h>

#include <tests/odyssey_test.h>

static int read_final_message_with_proof(const char *proof)
{
	char msg[256];
	int len = snprintf(msg, sizeof(msg), "c=biws,r=nonce,p=%s", proof);

	od_scram_state_t state;
	od_scram_state_init(&state);

	char *final_nonce;
	size_t final_nonce_size;
	uint8_t *client_proof = NULL;
	int rc = od_scram_read_client_final_message(NULL, &state, msg, len,
						    &final_nonce,
						    &final_nonce_size,
						    &client_proof);
	if (rc == 0) {
		od_free(client_proof);
	}
	od_scram_state_free(&state);

	return rc;
}

static void test_client_final_message_proof_len(void)
{
	/* 32 bytes, the SHA-256 key length */
	test(read_final_message_with_proof(
		     "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA=") == 0);

	/* empty, 3 and 31 bytes */
	test(read_final_message_with_proof("") == -1);
	test(read_final_message_with_proof("AAAA") == -1);
	test(read_final_message_with_proof(
		     "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA==") == -1);

	/* 33 bytes */
	test(read_final_message_with_proof(
		     "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA") == -1);
}

void odyssey_test_scram(void)
{
	test_client_final_message_proof_len();
}

#include <odyssey.h>

#include <machinarium/machinarium.h>
#include <machinarium/io.h>

#include <scram.h>
#include <sasl.h>

#include <tests/odyssey_test.h>

static int read_final_message(const char *cbind_value, char cbind_flag,
			      const char *proof, int channel_binding_in_use)
{
	char msg[256];
	int len = snprintf(msg, sizeof(msg), "c=%s,r=nonce,p=%s", cbind_value,
			   proof);

	od_scram_state_t state;
	od_scram_state_init(&state);
	state.cbind_flag = cbind_flag;

	char *final_nonce;
	size_t final_nonce_size;
	uint8_t *client_proof = NULL;
	int rc = od_scram_read_client_final_message(
		NULL, &state, msg, len, channel_binding_in_use, &final_nonce,
		&final_nonce_size, &client_proof);
	if (rc == 0) {
		od_free(client_proof);
	}
	od_scram_state_free(&state);

	return rc;
}

static int read_final_message_with_proof(const char *proof)
{
	return read_final_message("biws", 'n', proof, 0);
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

static void test_client_final_message_channel_binding(void)
{
	/* 'c' attribute must match the gs2-cbind-flag of the
	 * client-first-message */
	test(read_final_message(
		     "biws", 'n',
		     "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA=", 0) == 0);
	test(read_final_message(
		     "eSws", 'y',
		     "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA=", 0) == 0);
	test(read_final_message("biws", 'y',
				"AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA=",
				0) == OD_SASL_ERROR_UNEXPECTED_CBIND_ATTR);
	test(read_final_message("eSws", 'n',
				"AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA=",
				0) == OD_SASL_ERROR_UNEXPECTED_CBIND_ATTR);
	test(read_final_message("junk", 'n',
				"AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA=",
				0) == OD_SASL_ERROR_UNEXPECTED_CBIND_ATTR);

	/* channel binding in use requires a TLS io: this path is covered by
	 * the e2e scram-passthrough tests */
}

static int read_first_message(const char *msg, int channel_binding_in_use,
			      int ssl_in_use)
{
	od_scram_state_t state;
	od_scram_state_init(&state);

	int rc = od_scram_read_client_first_message(&state, (char *)msg,
						    strlen(msg),
						    channel_binding_in_use,
						    ssl_in_use);
	od_scram_state_free(&state);

	return rc;
}

static void test_client_first_message_channel_binding(void)
{
	/* empty message */
	test(read_first_message("", 0, 0) == -6);

	/* unsupported channel-binding flag */
	test(read_first_message("x,,n=user,r=nonce", 0, 0) == -2);

	/* missing comma after the flag */
	test(read_first_message("nx,,n=user,r=nonce", 0, 0) == -2);

	/* valid message without channel binding */
	test(read_first_message("n,,n=user,r=nonce", 0, 0) == 0);

	/* 'y' flag without TLS is fine */
	test(read_first_message("y,,n=user,r=nonce", 0, 0) == 0);

	/* 'y' flag while SSL is in use is a negotiation error */
	test(read_first_message("y,,n=user,r=nonce", 0, 1) ==
	     OD_SASL_ERROR_CHANNEL_BINDING_NEGOTIATION);

	/* 'n' flag while channel binding is in use is a protocol violation */
	test(read_first_message("n,,n=user,r=nonce", 1, 1) == -2);

	/* 'p' flag requires channel binding to be in use */
	test(read_first_message("p=tls-server-end-point,,n=user,r=nonce", 0,
				0) == -3);

	/* valid message with channel binding */
	test(read_first_message("p=tls-server-end-point,,n=user,r=nonce", 1,
				1) == 0);

	/* unsupported channel binding type */
	test(read_first_message("p=gs2,,n=user,r=nonce", 1, 1) == -2);

	/* authorization identity is not supported */
	test(read_first_message("n,a=user,,n=user,r=nonce", 0, 0) ==
	     OD_SASL_ERROR_AUTH_IDENTITY);

	/* mandatory extensions are not supported */
	test(read_first_message("n,,m=ext,n=user,r=nonce", 0, 0) ==
	     OD_SASL_ERROR_MANDATORY_EXT);

	/* non-printable characters in the nonce */
	test(read_first_message("n,,n=user,r=no\x01nce", 0, 0) == -1);

	/* truncated message */
	test(read_first_message("n,", 0, 0) == -1);
	test(read_first_message("n,,", 0, 0) == -1);
}

void odyssey_test_scram(void)
{
	test_client_final_message_proof_len();
	test_client_final_message_channel_binding();
	test_client_first_message_channel_binding();
}

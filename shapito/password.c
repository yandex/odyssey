
/*
 * Shapito.
 *
 * Protocol-level PostgreSQL client library.
*/

#include "shapito.h"

uint32_t shapito_password_salt(shapito_key_t *key)
{
	return rand() ^ key->key ^ key->key_pid;
}

int shapito_password_md5(shapito_password_t *pw,
                         char *user, int user_len,
                         char *password, int password_len,
                         char salt[4])
{
	uint8_t digest_prepare[16];
	char    digest_prepare_sz[32];
	uint8_t digest[16];
	shapito_md5_t ctx;
	if (password_len == 35 && memcmp(password, "md5", 3) == 0) {
		/* digest = md5(digest, salt) */
		shapito_md5_init(&ctx);
		shapito_md5_update(&ctx, password + 3, 32);
		shapito_md5_update(&ctx, salt, 4);
		shapito_md5_final(&ctx, digest);
	} else {
		/* digest = md5(password, user) */
		shapito_md5_init(&ctx);
		shapito_md5_update(&ctx, password, password_len);
		shapito_md5_update(&ctx, user, user_len);
		shapito_md5_final(&ctx, digest_prepare);
		shapito_md5_tostring(digest_prepare_sz, digest_prepare);
		/* digest = md5(digest, salt) */
		shapito_md5_init(&ctx);
		shapito_md5_update(&ctx, digest_prepare_sz, 32);
		shapito_md5_update(&ctx, salt, 4);
		shapito_md5_final(&ctx, digest);
	}

	/* 'md5' + to_string(digest) */
	pw->password_len = 35 + 1;
	pw->password = malloc(pw->password_len);
	if (pw->password == NULL)
		return -1;
	pw->password[0]  = 'm';
	pw->password[1]  = 'd';
	pw->password[2]  = '5';
	shapito_md5_tostring((pw->password + 3), digest);
	pw->password[35] = 0;
	return 0;
}

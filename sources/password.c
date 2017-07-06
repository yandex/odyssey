
/*
 * Shapito.
 *
 * Protocol-level PostgreSQL client library.
*/

#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <assert.h>

#include "sources/macro.h"
#include "sources/md5.h"
#include "sources/key.h"
#include "sources/password.h"

uint32_t so_password_salt(so_key_t *key)
{
	return rand() ^ key->key ^ key->key_pid;
}

int
so_password_md5(so_password_t *pw,
                char *user, int user_len,
                char *password, int password_len,
                char salt[4])
{
	uint8_t digest_prepare[16];
	char    digest_prepare_sz[32];
	uint8_t digest[16];

	/* digest = md5(password, user) */
	so_md5_t ctx;
	so_md5_init(&ctx);
	so_md5_update(&ctx, password, password_len);
	so_md5_update(&ctx, user, user_len);
	so_md5_final(&ctx, digest_prepare);
	so_md5_tostring(digest_prepare_sz, digest_prepare);

	/* digest = md5(digest, salt) */
	so_md5_init(&ctx);
	so_md5_update(&ctx, digest_prepare_sz, 32);
	so_md5_update(&ctx, salt, 4);
	so_md5_final(&ctx, digest);

	/* 'md5' + to_string(digest) */
	pw->password_len = 35 + 1;
	pw->password = malloc(pw->password_len);
	if (pw->password == NULL)
		return -1;
	pw->password[0]  = 'm';
	pw->password[1]  = 'd';
	pw->password[2]  = '5';
	so_md5_tostring((pw->password + 3), digest);
	pw->password[35] = 0;
	return 0;
}

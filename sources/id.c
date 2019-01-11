
/*
 * Odyssey.
 *
 * Scalable PostgreSQL connection pooler.
*/

#include <stdlib.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <inttypes.h>
#include <assert.h>

#include <machinarium.h>
#include <kiwi.h>
#include <odyssey.h>

void
od_id_generate(od_id_t *id, char *prefix)
{
	long int a = machine_lrand48();
	long int b = machine_lrand48();

	char seed[OD_ID_SEEDMAX];
	memcpy(seed + 0, &a, 4);
	memcpy(seed + 4, &b, 2);

	id->id_prefix = prefix;
	id->id_a = a;
	id->id_b = b;

	static const char *hex = "0123456789abcdef";
	int q, w;
	for (q = 0, w = 0; q < OD_ID_SEEDMAX; q++) {
		id->id[w++] = hex[(seed[q] >> 4) & 0x0F];
		id->id[w++] = hex[(seed[q]     ) & 0x0F];
	}
	assert(w == (OD_ID_SEEDMAX * 2));
}

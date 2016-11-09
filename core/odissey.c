
/*
 * odissey.
 *
 * PostgreSQL connection pooler and request router.
*/

#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>

#include <flint.h>

#include "od_macro.h"
#include "od_list.h"
#include "od_log.h"
#include "od_scheme.h"
#include "od_lex.h"
#include "od_config.h"
#include "od_server.h"
#include "od_pool.h"
#include "od.h"

int main(int argc, char *argv[])
{
	od_t odissey;
	od_init(&odissey);
	int rc = od_main(&odissey, argc, argv);
	od_free(&odissey);
	return rc;
}

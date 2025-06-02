#pragma once

/*
 * kiwi.
 *
 * postgreSQL protocol interaction library.
 */

int kiwi_parse_options_and_update_vars(kiwi_vars_t *vars, char *pgoptions,
				       int pgoptions_len);

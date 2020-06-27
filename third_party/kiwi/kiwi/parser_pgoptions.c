
/*
 * kiwi.
 *
 * postgreSQL protocol interaction library.
 */

#include <getopt.h>
#include <wordexp.h>
#include <assert.h>

#include "kiwi.h"

static void
ParseLongOption(const char *string, char **name, char **value)
{
	size_t equal_pos;
	char *cp;

	assert(string);
	assert(name);
	assert(value);

	equal_pos = strcspn(string, "=");

	if (string[equal_pos] == '=') {
		*name = malloc(equal_pos + 1);
		strncpy(*name, string, equal_pos);

		*value = strdup(&string[equal_pos + 1]);
	} else {
		/* no equal sign in string */
		*name  = strdup(string);
		*value = NULL;
	}

	for (cp = *name; *cp; cp++)
		if (*cp == '-')
			*cp = '_';
}

int
kiwi_parse_pgoptions_and_update_vars(kiwi_vars_t *vars,
                                     char *pgoptions,
                                     int pgoptions_len)
{
	int errs = 0;
	int flag;

	int argc;
	char **argv;
	char *old_pgoptions = NULL;

	if (pgoptions[pgoptions_len - 1] != '\0') {
		char *old_pgoptions = pgoptions;
		pgoptions           = malloc(pgoptions_len + 1);
		memcpy(pgoptions, old_pgoptions, pgoptions_len);
		pgoptions[pgoptions_len] = '\0';
	}

	wordexp_t res;
	wordexp(pgoptions, &res, 0);

	argc = res.we_wordc + 1;
	argv = malloc(sizeof(char *) * (argc + 1));
	if (argv == 0) {
		return -1;
	}

	argv[0] = "fake_word"; // needs for getopt function

	memcpy(&argv[1], res.we_wordv, sizeof(char *) * argc);

	while ((flag = getopt(
	          argc, argv, "B:bc:C:D:d:EeFf:h:ijk:lN:nOo:Pp:r:S:sTt:v:W:-:")) !=
	       -1) {
		switch (flag) {
			case '-':
			case 'c': {
				char *name, *value;

				ParseLongOption(optarg, &name, &value);

				kiwi_vars_update(
				  vars, name, strlen(name) + 1, value, strlen(value) + 1);
				break;
			}
			default:
				errs++;
		}
	}

	optind = 1;

	if (old_pgoptions) {
		free(pgoptions);
	}

	return errs;
}

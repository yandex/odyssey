/*
 * Odyssey.
 *
 * Scalable PostgreSQL connection pooler.
 */

/*
 * Functions ported from PostgreSQL libpq,
 * src/interfaces/libpq/fe-connect.c.
 *
 * Portions Copyright (c) 1996-2026, PostgreSQL Global Development Group
 * Distributed under the PostgreSQL Licence
 * (https://www.postgresql.org/about/licence/)
 */

#include <odyssey.h>

#include <sys/stat.h>

#include <machinarium/file.h>
#include <machinarium/memory.h>

#include <common/string.h>
#include <od_memory.h>
#include <util.h>

#include <port.h>

/*
 * Advance past one escaped field of a pgpass line, returning a pointer
 * to the character right after the unescaped ':' separator, or NULL if
 * the line ends before the separator.
 */
static char *pgpass_skip_field(char *buf)
{
	char *tbuf = buf;
	bool bslash = false;

	while (*tbuf != 0)
	{
		if (*tbuf == '\\' && !bslash)
		{
			tbuf++;
			bslash = true;
		}
		if (*tbuf == ':' && !bslash)
			return tbuf + 1;
		bslash = false;
		if (*tbuf == 0)
			return NULL;
		tbuf++;
	}
	return NULL;
}

/*
 * Returns a pointer to the next token or NULL if the current
 * token doesn't match.
 */
char *pwdfMatchesString(char *buf, const char *token)
{
	char	   *tbuf;
	const char *ttok;
	bool		bslash = false;

	if (buf == NULL || token == NULL)
		return NULL;
	tbuf = buf;
	ttok = token;
	if (tbuf[0] == '*' && tbuf[1] == ':')
		return tbuf + 2;
	while (*tbuf != 0)
	{
		if (*tbuf == '\\' && !bslash)
		{
			tbuf++;
			bslash = true;
		}
		if (*tbuf == ':' && *ttok == 0 && !bslash)
			return tbuf + 1;
		bslash = false;
		if (*ttok == 0)
			return NULL;
		if (*tbuf == *ttok)
		{
			tbuf++;
			ttok++;
		}
		else
			return NULL;
	}
	return NULL;
}

/*
 * Port of libpq's passwordFromFile() from fe-connect.c.
 *
 * Reads the pgpass file and returns the password for the first line
 * matching dbname/username, de-escaped and allocated with od_malloc();
 * the caller must free it with od_free().
 *
 * On failure, *errmsg is set to a static error description. It is
 * left NULL on success, or if no password could be found.
 */
char *passwordFromFile(const char *hostname, const char *port,
		       const char *dbname, const char *username,
		       const char *pgpassfile, const char **errmsg)
{
	struct stat stat_buf;
	char	   *buf;
	size_t		size;

	/*
	 * libpq also matches the hostname and port fields, but odyssey
	 * rules are not bound to a specific host or port, so both fields
	 * are skipped.
	 */
	(void)hostname;
	(void)port;

	*errmsg = NULL;

	if (dbname == NULL || dbname[0] == '\0')
		return NULL;

	if (username == NULL || username[0] == '\0')
		return NULL;

	if (stat(pgpassfile, &stat_buf) != 0) {
		*errmsg = "could not read password file";
		return NULL;
	}

	if (!S_ISREG(stat_buf.st_mode)) {
		*errmsg = "password file is not a plain file";
		return NULL;
	}

	/* If password file is insecure, refuse to use it. */
	if (stat_buf.st_mode & (S_IRWXG | S_IRWXO)) {
		*errmsg = "password file has group or world access; "
			  "permissions should be u=rw (0600) or less";
		return NULL;
	}

	/* Read the whole file to accommodate any reasonable line length */
	if (mm_file_read_all(pgpassfile, &buf, &size) != 0) {
		*errmsg = "could not read password file";
		return NULL;
	}

	char *cursor = buf;
	char *line;
	while ((line = od_next_line(&cursor)) != NULL) {
		/* ignore comments */
		if (line[0] != '#')
		{
			char	   *t = line;
			int			len;

			/* strip trailing newline and carriage return */
			len = pg_strip_crlf(t);

			if (len > 0 &&
			    (t = pgpass_skip_field(t)) != NULL &&
			    (t = pgpass_skip_field(t)) != NULL &&
			    (t = pwdfMatchesString(t, dbname)) != NULL &&
			    (t = pwdfMatchesString(t, username)) != NULL)
			{
				/* Found a match. */
				char	   *ret,
					   *p1,
					   *p2;

				ret = od_strdup(t);
				if (ret == NULL) {
					*errmsg = "out of memory";
					break;
				}

				/* De-escape password. */
				for (p1 = p2 = ret; *p1 != ':' && *p1 != '\0';
				     ++p1, ++p2)
				{
					if (*p1 == '\\' && p1[1] != '\0')
						++p1;
					*p2 = *p1;
				}
				*p2 = '\0';

				memset(buf, 0, size);
				mm_free(buf);
				return ret;
			}
		}
	}

	memset(buf, 0, size);
	mm_free(buf);
	return NULL;
}
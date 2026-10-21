#pragma once

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

/*
 * Returns a pointer to the next token or NULL if the current
 * token doesn't match.
 */
char *pwdfMatchesString(char *buf, const char *token);

/*
 * Port of libpq's passwordFromFile(): find a password for the given
 * dbname/username in the pgpass file.
 *
 * Unlike libpq, the hostname and port fields of the line are skipped:
 * odyssey rules are not bound to a specific host or port.
 *
 * Returns the de-escaped password allocated with od_malloc() (the caller
 * must free it with od_free()), or NULL if no password was found or the
 * file cannot be used; in the latter case *errmsg points to a static
 * error description.
 */
char *passwordFromFile(const char *hostname, const char *port,
		       const char *dbname, const char *username,
		       const char *pgpassfile, const char **errmsg);
/* Convert supported pgbouncer.ini settings and mark gaps for manual review. */

#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <pgbouncer_convert.h>

typedef struct {
	char *name;
	char *host;
	char *port;
	char *dbname;
	char *user;
	char *password;
	char *pool_size;
	char *pool_mode;
	char *min_pool_size;
	int blocked;
	int storage_blocked;
} od_pgb_database_t;

typedef struct {
	char *name;
	char *password;
	int blocked;
} od_pgb_user_t;

typedef struct {
	char *listen_addr;
	char *listen_port;
	char *unix_socket_dir;
	char *unix_socket_mode;
	char *auth_type;
	char *auth_file;
	char *pool_mode;
	char *default_pool_size;
	char *min_pool_size;
	char *max_client_conn;
	char *logfile;
	char *pidfile;
	od_pgb_database_t *databases;
	size_t database_count;
	od_pgb_user_t *users;
	size_t user_count;
	char **warnings;
	size_t warning_count;
	int blocked;
} od_pgb_config_t;

static char *od_pgb_trim(char *s)
{
	while (isspace((unsigned char)*s)) {
		s++;
	}
	char *end = s + strlen(s);
	while (end > s && isspace((unsigned char)end[-1])) {
		*--end = '\0';
	}
	return s;
}

static int od_pgb_error(FILE *err, const char *path, size_t line,
			const char *message, const char *value)
{
	if (line) {
		fprintf(err, "%s:%zu: %s: %s\n", path, line, message, value);
	} else {
		fprintf(err, "%s: %s: %s\n", path, message, value);
	}
	return -1;
}

static int od_pgb_set(char **target, const char *value)
{
	if (*target != NULL) {
		return -1;
	}
	*target = strdup(value);
	return *target == NULL ? -1 : 0;
}

static int od_pgb_warn(od_pgb_config_t *config, FILE *err, const char *path,
		       size_t line, const char *message, const char *value)
{
	int len = snprintf(NULL, 0, "WARNING: %s:%zu: %s: %s", path, line,
			   message, value);
	if (len < 0) {
		return -1;
	}
	char *warning = malloc((size_t)len + 1);
	if (warning == NULL) {
		return -1;
	}
	snprintf(warning, (size_t)len + 1, "WARNING: %s:%zu: %s: %s", path,
		 line, message, value);
	/* Warnings are also emitted as single-line configuration comments. */
	for (char *p = warning; *p; p++) {
		if (*p == '\n' || *p == '\r') {
			*p = ' ';
		}
	}
	char **next = realloc(config->warnings,
			      (config->warning_count + 1) * sizeof(*next));
	if (next == NULL) {
		free(warning);
		return -1;
	}
	config->warnings = next;
	config->warnings[config->warning_count++] = warning;
	fprintf(err, "%s\n", warning);
	return 0;
}

static int od_pgb_security_option(const char *key)
{
	return strncmp(key, "auth_", 5) == 0 ||
	       strncmp(key, "client_tls_", 11) == 0 ||
	       strncmp(key, "server_tls_", 11) == 0 ||
	       strcmp(key, "connect_query") == 0;
}

static int od_pgb_global(od_pgb_config_t *config, const char *key,
			 const char *value)
{
	struct {
		const char *name;
		char **field;
	} fields[] = {
		{ "listen_addr", &config->listen_addr },
		{ "listen_port", &config->listen_port },
		{ "unix_socket_dir", &config->unix_socket_dir },
		{ "unix_socket_mode", &config->unix_socket_mode },
		{ "auth_type", &config->auth_type },
		{ "auth_file", &config->auth_file },
		{ "pool_mode", &config->pool_mode },
		{ "default_pool_size", &config->default_pool_size },
		{ "min_pool_size", &config->min_pool_size },
		{ "max_client_conn", &config->max_client_conn },
		{ "logfile", &config->logfile },
		{ "pidfile", &config->pidfile },
	};
	for (size_t i = 0; i < sizeof(fields) / sizeof(fields[0]); i++) {
		if (strcmp(key, fields[i].name) == 0) {
			return od_pgb_set(fields[i].field, value);
		}
	}
	return 1;
}

static int od_pgb_database_option(od_pgb_database_t *db, const char *key,
				  const char *value)
{
	struct {
		const char *name;
		char **field;
	} fields[] = {
		{ "host", &db->host },
		{ "port", &db->port },
		{ "dbname", &db->dbname },
		{ "user", &db->user },
		{ "password", &db->password },
		{ "pool_size", &db->pool_size },
		{ "pool_mode", &db->pool_mode },
		{ "min_pool_size", &db->min_pool_size },
	};
	for (size_t i = 0; i < sizeof(fields) / sizeof(fields[0]); i++) {
		if (strcmp(key, fields[i].name) == 0) {
			return od_pgb_set(fields[i].field, value);
		}
	}
	return 1;
}

static int od_pgb_parse_connstr(od_pgb_config_t *config, od_pgb_database_t *db,
				char *input, FILE *err, const char *path,
				size_t line)
{
	char *p = input;
	while (*p) {
		while (isspace((unsigned char)*p)) {
			p++;
		}
		if (*p == '\0') {
			break;
		}
		char *key = p;
		while (*p && *p != '=' && !isspace((unsigned char)*p)) {
			p++;
		}
		if (isspace((unsigned char)*p)) {
			*p++ = '\0';
			while (isspace((unsigned char)*p)) {
				p++;
			}
		}
		if (*p != '=') {
			return od_pgb_error(err, path, line,
					    "expected key=value", key);
		}
		*p++ = '\0';
		while (isspace((unsigned char)*p)) {
			p++;
		}
		char *value = p;
		if (*p == '\'' || *p == '"') {
			char quote = *p++;
			value = p;
			char *dst = p;
			while (*p) {
				if (*p == quote) {
					if (p[1] != quote) {
						break;
					}
					p++;
				}
				*dst++ = *p++;
			}
			if (*p != quote) {
				return od_pgb_error(err, path, line,
						    "unterminated quote", key);
			}
			*dst = '\0';
			p++;
			if (*p && !isspace((unsigned char)*p)) {
				return od_pgb_error(err, path, line,
						    "invalid value", key);
			}
		} else {
			while (*p && !isspace((unsigned char)*p)) {
				p++;
			}
			if (*p) {
				*p++ = '\0';
			}
		}
		int rc = od_pgb_database_option(db, key, value);
		if (rc < 0) {
			return od_pgb_error(
				err, path, line,
				"duplicate database option or out of memory",
				key);
		}
		if (rc > 0) {
			int security = od_pgb_security_option(key);
			if (security) {
				db->blocked = 1;
			}
			if (od_pgb_warn(
				    config, err, path, line,
				    security ?
					    "unsupported database security option; routes blocked" :
					    "unsupported database option",
				    key) != 0) {
				return -1;
			}
		}
	}
	return 0;
}

static int od_pgb_read_ini(od_pgb_config_t *config, const char *path, FILE *err)
{
	FILE *in = fopen(path, "r");
	if (in == NULL) {
		return od_pgb_error(err, path, 0, "cannot open",
				    strerror(errno));
	}
	char *line = NULL;
	size_t capacity = 0;
	size_t number = 0;
	int section = 0;
	int rc = -1;
	while (getline(&line, &capacity, in) >= 0) {
		number++;
		char *s = od_pgb_trim(line);
		if (!*s || *s == '#' || *s == ';') {
			continue;
		}
		if (strncmp(s, "%include", 8) == 0 &&
		    isspace((unsigned char)s[8])) {
			config->blocked = 1;
			if (od_pgb_warn(
				    config, err, path, number,
				    "unsupported include directive; expand manually, routes blocked",
				    "%include") != 0) {
				goto done;
			}
			continue;
		}
		if (*s == '[') {
			char *close = strchr(s, ']');
			if (close == NULL || *od_pgb_trim(close + 1)) {
				od_pgb_error(err, path, number,
					     "invalid section", s);
				goto done;
			}
			*close = '\0';
			section = strcmp(s + 1, "pgbouncer") == 0 ? 1 :
				  strcmp(s + 1, "databases") == 0 ? 2 :
								    -1;
			if (section == -1) {
				const char *name = s + 1;
				if (strcmp(name, "users") == 0) {
					config->blocked = 1;
					name = "users (routes blocked)";
				}
				if (od_pgb_warn(config, err, path, number,
						"unsupported section",
						name) != 0) {
					goto done;
				}
			}
			continue;
		}
		if (section == -1) {
			continue;
		}
		char *equals = strchr(s, '=');
		if (equals == NULL || section == 0) {
			od_pgb_error(err, path, number, "expected key=value",
				     s);
			goto done;
		}
		*equals++ = '\0';
		char *key = od_pgb_trim(s);
		char *value = od_pgb_trim(equals);
		if (section == 2 && *key == '"') {
			size_t len = strlen(key);
			if (len < 2 || key[len - 1] != '"') {
				od_pgb_error(err, path, number,
					     "invalid database name", key);
				goto done;
			}
			key[len - 1] = '\0';
			key++;
			char *src = key;
			char *dst = key;
			while (*src) {
				if (*src == '"') {
					if (src[1] != '"') {
						od_pgb_error(
							err, path, number,
							"invalid database name",
							key);
						goto done;
					}
					src++;
				}
				*dst++ = *src++;
			}
			*dst = '\0';
		}
		if (!*key) {
			od_pgb_error(err, path, number, "empty key", s);
			goto done;
		}
		if (section == 1) {
			int result = od_pgb_global(config, key, value);
			if (result < 0) {
				od_pgb_error(
					err, path, number,
					"duplicate option or out of memory",
					key);
				goto done;
			}
			if (result > 0) {
				int security = od_pgb_security_option(key);
				if (security) {
					config->blocked = 1;
				}
				if (od_pgb_warn(
					    config, err, path, number,
					    security ?
						    "unsupported security option; routes blocked" :
						    "unsupported option",
					    key) != 0) {
					goto done;
				}
			}
		} else {
			for (size_t i = 0; i < config->database_count; i++) {
				if (strcmp(config->databases[i].name, key) ==
				    0) {
					od_pgb_error(err, path, number,
						     "duplicate database", key);
					goto done;
				}
			}
			od_pgb_database_t *next = realloc(
				config->databases,
				(config->database_count + 1) * sizeof(*next));
			if (next == NULL) {
				goto done;
			}
			config->databases = next;
			od_pgb_database_t *db =
				&config->databases[config->database_count++];
			memset(db, 0, sizeof(*db));
			db->name = strdup(key);
			if (db->name == NULL ||
			    od_pgb_parse_connstr(config, db, value, err, path,
						 number) != 0) {
				goto done;
			}
		}
	}
	if (ferror(in)) {
		od_pgb_error(err, path, number, "read failed", strerror(errno));
	} else {
		rc = 0;
	}
done:
	free(line);
	fclose(in);
	return rc;
}

static char *od_pgb_quoted(char **cursor)
{
	char *p = *cursor;
	while (isspace((unsigned char)*p)) {
		p++;
	}
	if (*p++ != '"') {
		return NULL;
	}
	char *result = malloc(strlen(p) + 1);
	if (result == NULL) {
		return NULL;
	}
	char *dst = result;
	while (*p) {
		if (*p == '"') {
			if (p[1] != '"') {
				break;
			}
			p++;
		}
		*dst++ = *p++;
	}
	if (*p != '"') {
		free(result);
		return NULL;
	}
	*dst = '\0';
	*cursor = p + 1;
	return result;
}

static int od_pgb_read_users(od_pgb_config_t *config, const char *path,
			     FILE *err)
{
	FILE *in = fopen(path, "r");
	if (in == NULL) {
		return od_pgb_error(err, path, 0, "cannot open auth_file",
				    strerror(errno));
	}
	char *line = NULL;
	size_t capacity = 0;
	size_t number = 0;
	int rc = -1;
	while (getline(&line, &capacity, in) >= 0) {
		number++;
		char *p = od_pgb_trim(line);
		if (!*p || *p == '#' || *p == ';') {
			continue;
		}
		char *name = od_pgb_quoted(&p);
		char *password = name ? od_pgb_quoted(&p) : NULL;
		p = od_pgb_trim(p);
		if (name == NULL || password == NULL ||
		    (*p != '\0' && *p != '#' && *p != ';')) {
			free(name);
			free(password);
			od_pgb_error(err, path, number,
				     "invalid auth_file line",
				     "expected quoted user and password");
			goto done;
		}
		for (size_t i = 0; i < config->user_count; i++) {
			if (strcmp(config->users[i].name, name) == 0) {
				od_pgb_error(err, path, number,
					     "duplicate user", name);
				free(name);
				free(password);
				goto done;
			}
		}
		int blocked = strncmp(password, "SCRAM-SHA-256$", 14) == 0;
		if (blocked &&
		    od_pgb_warn(
			    config, err, path, number,
			    "SCRAM secret needs manual backend auth review; user blocked",
			    name) != 0) {
			free(name);
			free(password);
			goto done;
		}
		if (strlen(password) == 35 &&
		    strncmp(password, "md5", 3) == 0) {
			for (size_t i = 3; i < 35; i++) {
				if (!isxdigit((unsigned char)password[i])) {
					od_pgb_error(err, path, number,
						     "invalid MD5 secret",
						     name);
					free(name);
					free(password);
					goto done;
				}
			}
		}
		od_pgb_user_t *next =
			realloc(config->users,
				(config->user_count + 1) * sizeof(*next));
		if (next == NULL) {
			free(name);
			free(password);
			goto done;
		}
		config->users = next;
		config->users[config->user_count++] =
			(od_pgb_user_t){ name, password, blocked };
	}
	if (ferror(in)) {
		od_pgb_error(err, path, number, "read failed", strerror(errno));
	} else if (config->user_count == 0) {
		od_pgb_error(err, path, 0, "auth_file has no users", path);
	} else {
		rc = 0;
	}
done:
	free(line);
	fclose(in);
	return rc;
}

static int od_pgb_uint(const char *value, long min, long max)
{
	if (value == NULL || !*value) {
		return -1;
	}
	for (const char *p = value; *p; p++) {
		if (!isdigit((unsigned char)*p)) {
			return -1;
		}
	}
	errno = 0;
	char *end;
	long parsed = strtol(value, &end, 10);
	return errno == 0 && *end == '\0' && parsed >= min && parsed <= max ?
		       0 :
		       -1;
}

static int od_pgb_validate(od_pgb_config_t *config, const char *path, FILE *err)
{
	if (config->database_count == 0) {
		if (!config->warning_count) {
			return od_pgb_error(err, path, 0, "no databases",
					    "[databases]");
		}
		if (od_pgb_warn(
			    config, err, path, 0,
			    "no databases were converted; default route blocked",
			    "[databases]") != 0) {
			return -1;
		}
		config->databases = calloc(1, sizeof(*config->databases));
		if (config->databases == NULL) {
			return -1;
		}
		config->database_count = 1;
		config->databases[0].name = strdup("*");
		if (config->databases[0].name == NULL) {
			return -1;
		}
		config->databases[0].blocked = 1;
		config->databases[0].storage_blocked = 1;
	}
	if (config->listen_addr == NULL || !*config->listen_addr ||
	    strchr(config->listen_addr, ',') != NULL) {
		if (od_pgb_warn(
			    config, err, path, 0,
			    "one explicit listen_addr is required; using loopback placeholder, routes blocked",
			    "listen_addr") != 0) {
			return -1;
		}
		config->blocked = 1;
		free(config->listen_addr);
		config->listen_addr = strdup("127.0.0.1");
		if (config->listen_addr == NULL) {
			return -1;
		}
	}
	if (config->auth_file == NULL || !*config->auth_file) {
		if (od_pgb_warn(
			    config, err, path, 0,
			    "no auth_file; user credentials must be configured manually, routes blocked",
			    "auth_file") != 0) {
			return -1;
		}
		config->blocked = 1;
	}
	if (config->auth_type == NULL) {
		config->auth_type = strdup("md5");
	}
	if (config->auth_type == NULL) {
		return -1;
	}
	if (strcmp(config->auth_type, "md5") != 0 &&
	    strcmp(config->auth_type, "trust") != 0) {
		if (od_pgb_warn(config, err, path, 0,
				"unsupported auth_type; routes blocked",
				config->auth_type) != 0) {
			return -1;
		}
		config->blocked = 1;
	}
	const char *mode = config->pool_mode ? config->pool_mode : "session";
	if (strcmp(mode, "session") != 0 && strcmp(mode, "transaction") != 0) {
		if (od_pgb_warn(
			    config, err, path, 0,
			    "unsupported pool_mode; using session placeholder, routes blocked",
			    mode) != 0) {
			return -1;
		}
		config->blocked = 1;
		free(config->pool_mode);
		config->pool_mode = NULL;
	}
	if ((config->listen_port &&
	     od_pgb_uint(config->listen_port, 1, 65535)) ||
	    (config->max_client_conn &&
	     od_pgb_uint(config->max_client_conn, 1, INT_MAX)) ||
	    (config->default_pool_size &&
	     od_pgb_uint(config->default_pool_size, 0, INT_MAX)) ||
	    (config->min_pool_size &&
	     od_pgb_uint(config->min_pool_size, 0, INT_MAX))) {
		return od_pgb_error(err, path, 0, "invalid numeric setting",
				    "[pgbouncer]");
	}
	if (config->unix_socket_mode) {
		const char *socket_mode = config->unix_socket_mode;
		if (strlen(socket_mode) != 4 || socket_mode[0] != '0' ||
		    strspn(socket_mode + 1, "01234567") != 3) {
			return od_pgb_error(err, path, 0,
					    "invalid unix_socket_mode",
					    socket_mode);
		}
	}
	for (size_t i = 0; i < config->database_count; i++) {
		od_pgb_database_t *db = &config->databases[i];
		if (strcmp(db->name, "*") == 0 && !db->storage_blocked) {
			if (od_pgb_warn(
				    config, err, path, 0,
				    "database wildcard needs manual routing; default route blocked",
				    db->name) != 0) {
				return -1;
			}
			db->blocked = 1;
		}
		if (!db->storage_blocked &&
		    (db->host == NULL || !*db->host ||
		     strchr(db->host, ',') != NULL || db->host[0] == '/' ||
		     db->host[0] == '@')) {
			if (od_pgb_warn(
				    config, err, path, 0,
				    "database needs one TCP host; using local placeholder, routes blocked",
				    db->name) != 0) {
				return -1;
			}
			db->blocked = 1;
			db->storage_blocked = 1;
		}
		if (db->port && od_pgb_uint(db->port, 1, 65535)) {
			return od_pgb_error(err, path, 0, "invalid port",
					    db->name);
		}
		if (db->pool_size && od_pgb_uint(db->pool_size, 0, INT_MAX)) {
			return od_pgb_error(err, path, 0, "invalid pool_size",
					    db->name);
		}
		if (db->min_pool_size &&
		    od_pgb_uint(db->min_pool_size, 0, INT_MAX)) {
			return od_pgb_error(err, path, 0,
					    "invalid min_pool_size", db->name);
		}
		if (db->pool_mode && strcmp(db->pool_mode, "session") != 0 &&
		    strcmp(db->pool_mode, "transaction") != 0) {
			if (od_pgb_warn(
				    config, err, path, 0,
				    "unsupported database pool_mode; routes blocked",
				    db->name) != 0) {
				return -1;
			}
			db->blocked = 1;
			free(db->pool_mode);
			db->pool_mode = NULL;
		}
		if (db->password && !db->user) {
			if (od_pgb_warn(
				    config, err, path, 0,
				    "password requires forced user; routes blocked",
				    db->name) != 0) {
				return -1;
			}
			db->blocked = 1;
		}
		if (db->user && !db->password) {
			if (od_pgb_warn(
				    config, err, path, 0,
				    "forced user requires password; routes blocked",
				    db->name) != 0) {
				return -1;
			}
			db->blocked = 1;
		}
	}
	return 0;
}

static void od_pgb_print_quoted(FILE *out, const char *value)
{
	fputc('"', out);
	for (const char *p = value; *p; p++) {
		if (*p == '"') {
			fputc('\\', out);
		}
		if (*p == '\n') {
			fputs("\\n", out);
		} else if (*p == '\r') {
			fputs("\\r", out);
		} else {
			fputc(*p, out);
		}
	}
	fputc('"', out);
}

static void od_pgb_print_string(FILE *out, const char *key, const char *value,
				int indent)
{
	if (value == NULL) {
		return;
	}
	fprintf(out, "%*s%s ", indent, "", key);
	od_pgb_print_quoted(out, value);
	fputc('\n', out);
}

static void od_pgb_emit(const od_pgb_config_t *config, FILE *out)
{
	fputs("# Generated from PgBouncer. Review credentials and limits before use.\n"
	      "# This file contains auth_file secrets; restrict its permissions.\n",
	      out);
	if (config->warning_count) {
		fputs("# Resolve these warnings before cutover. Unconvertible routes use authentication block.\n",
		      out);
	}
	for (size_t i = 0; i < config->warning_count; i++) {
		fprintf(out, "# %s\n", config->warnings[i]);
	}
	fputs("log_format \"%p %t %l [%i %s] (%c) %m\\n\"\n", out);
	od_pgb_print_string(out, "pid_file", config->pidfile, 0);
	od_pgb_print_string(out, "log_file", config->logfile, 0);
	int unix_socket_enabled = config->unix_socket_dir == NULL ||
				  *config->unix_socket_dir != '\0';
	if (unix_socket_enabled) {
		od_pgb_print_string(out, "unix_socket_dir",
				    config->unix_socket_dir ?
					    config->unix_socket_dir :
					    "/tmp",
				    0);
		od_pgb_print_string(out, "unix_socket_mode",
				    config->unix_socket_mode ?
					    config->unix_socket_mode :
					    "0777",
				    0);
	}
	fprintf(out, "client_max %s\n",
		config->max_client_conn ? config->max_client_conn : "100");
	fputs("\nlisten {\n", out);
	od_pgb_print_string(out, "host", config->listen_addr, 4);
	fprintf(out, "    port %s\n}\n",
		config->listen_port ? config->listen_port : "6432");
	if (unix_socket_enabled) {
		fprintf(out, "\nlisten {\n    port %s\n}\n",
			config->listen_port ? config->listen_port : "6432");
	}
	for (size_t i = 0; i < config->database_count; i++) {
		const od_pgb_database_t *db = &config->databases[i];
		fputs("\nstorage ", out);
		fputc('"', out);
		fprintf(out, "pgb_%zu", i + 1);
		if (db->storage_blocked) {
			fputs("\" {\n    type \"local\"\n}\n", out);
		} else {
			fputs("\" {\n    type \"remote\"\n", out);
			od_pgb_print_string(out, "host", db->host, 4);
			fprintf(out, "    port %s\n    tls \"prefer\"\n}\n",
				db->port ? db->port : "5432");
		}
		fputs("\ndatabase ", out);
		if (strcmp(db->name, "*") == 0) {
			fputs("default", out);
		} else {
			od_pgb_print_quoted(out, db->name);
		}
		fputs(" {\n", out);
		size_t users = config->user_count ? config->user_count : 1;
		for (size_t j = 0; j < users; j++) {
			const od_pgb_user_t *user =
				config->user_count ? &config->users[j] : NULL;
			fputs("    user ", out);
			if (user) {
				od_pgb_print_quoted(out, user->name);
			} else {
				fputs("default", out);
			}
			fputs(" {\n", out);
			int blocked = config->blocked || db->blocked ||
				      user == NULL || user->blocked;
			od_pgb_print_string(out, "authentication",
					    blocked ? "block" :
					    strcmp(config->auth_type,
						   "trust") == 0 ?
						      "none" :
						      "md5",
					    8);
			if (user) {
				od_pgb_print_string(out, "password",
						    user->password, 8);
			}
			fputs("        storage ", out);
			fprintf(out, "\"pgb_%zu\"\n", i + 1);
			if (strcmp(db->name, "*") != 0) {
				od_pgb_print_string(
					out, "storage_db",
					db->dbname ? db->dbname : db->name, 8);
			}
			od_pgb_print_string(out, "storage_user", db->user, 8);
			od_pgb_print_string(out, "storage_password",
					    db->password, 8);
			od_pgb_print_string(
				out, "pool",
				db->storage_blocked ? "session" :
				db->pool_mode	    ? db->pool_mode :
						      (config->pool_mode ?
							       config->pool_mode :
							       "session"),
				8);
			fprintf(out, "        pool_size %s\n",
				db->pool_size ?
					db->pool_size :
					(config->default_pool_size ?
						 config->default_pool_size :
						 "20"));
			fprintf(out, "        min_pool_size %s\n",
				blocked ? "0" :
				db->min_pool_size ?
					  db->min_pool_size :
					  (config->min_pool_size ?
						   config->min_pool_size :
						   "0"));
			fputs("    }\n", out);
		}
		fputs("}\n", out);
	}
}

static void od_pgb_free(od_pgb_config_t *config)
{
	free(config->listen_addr);
	free(config->listen_port);
	free(config->unix_socket_dir);
	free(config->unix_socket_mode);
	free(config->auth_type);
	free(config->auth_file);
	free(config->pool_mode);
	free(config->default_pool_size);
	free(config->min_pool_size);
	free(config->max_client_conn);
	free(config->logfile);
	free(config->pidfile);
	for (size_t i = 0; i < config->database_count; i++) {
		od_pgb_database_t *db = &config->databases[i];
		free(db->name);
		free(db->host);
		free(db->port);
		free(db->dbname);
		free(db->user);
		free(db->password);
		free(db->pool_size);
		free(db->pool_mode);
		free(db->min_pool_size);
	}
	free(config->databases);
	for (size_t i = 0; i < config->user_count; i++) {
		free(config->users[i].name);
		free(config->users[i].password);
	}
	free(config->users);
	for (size_t i = 0; i < config->warning_count; i++) {
		free(config->warnings[i]);
	}
	free(config->warnings);
}

int od_pgbouncer_convert(const char *path, FILE *out, FILE *err)
{
	od_pgb_config_t config = { 0 };
	int rc = -1;
	if (od_pgb_read_ini(&config, path, err) != 0 ||
	    od_pgb_validate(&config, path, err) != 0) {
		goto done;
	}
	if (config.auth_file && *config.auth_file) {
		rc = od_pgb_read_users(&config, config.auth_file, err);
		if (rc != 0) {
			goto done;
		}
	}
	for (size_t i = 0; i < config.database_count; i++) {
		if (config.databases[i].user && config.user_count > 1) {
			rc = od_pgb_warn(
				&config, err, path, 0,
				"forced user with multiple client users would change pool size; routes blocked",
				config.databases[i].name);
			if (rc != 0) {
				goto done;
			}
			config.databases[i].blocked = 1;
		}
	}
	od_pgb_emit(&config, out);
	rc = ferror(out) ? -1 : 0;
done:
	od_pgb_free(&config);
	return rc;
}

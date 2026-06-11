/*
 * Odyssey.
 *
 * Scalable PostgreSQL connection pooler.
 */

#include <odyssey.h>

#include <machinarium/machinarium.h>

#include <status.h>
#include <od_ldap.h>
#include <client.h>
#include <route.h>
#include <rules.h>
#include <instance.h>
#include <global.h>
#include <frontend.h>
#include <thread_pool.h>

static od_retcode_t od_ldap_error_report_client(od_client_t *cl, int rc)
{
	if (rc == LDAP_SUCCESS) {
		return OK_RESPONSE;
	}

	od_gerror("auth", cl, NULL,
		  "ldap authentication failed for user \"%s\": %s (%d)",
		  cl->startup.user.value, ldap_err2string(rc), rc);

	od_frontend_fatal(cl, KIWI_SYSTEM_ERROR,
			  "ldap authentication failed for user \"%s\": %s (%d)",
			  cl->startup.user.value, ldap_err2string(rc), rc);

	return NOT_OK_RESPONSE;
}

od_retcode_t od_ldap_endpoint_prepare(od_ldap_endpoint_t *le)
{
	const char *scheme;
	scheme = le->ldapscheme; /* ldap or ldaps */
	if (scheme == NULL) {
		scheme = "ldap";
	}

	le->ldapurl = NULL;
	if (!le->ldapserver) {
		/* TODO: support multiple ldap servers */
		return NOT_OK_RESPONSE;
	}

	if (od_asprintf(&le->ldapurl, "%s://%s:%d", scheme, le->ldapserver,
			le->ldapport) != OK_RESPONSE) {
		return NOT_OK_RESPONSE;
	}

	return OK_RESPONSE;
}

od_retcode_t
od_ldap_change_storage_credentials(od_logger_t *logger,
				   od_ldap_storage_credentials_t *lsc,
				   od_client_t *client)
{
	client->ldap_storage_username = lsc->lsc_username;
	client->ldap_storage_username_len = strlen(lsc->lsc_username);
	client->ldap_storage_password = lsc->lsc_password;
	client->ldap_storage_password_len = strlen(lsc->lsc_password);
	od_debug(logger, "auth_ldap", client, NULL,
		 "storage_user changed to %s", lsc->lsc_username);
	return OK_RESPONSE;
}

int search_storage_credentials(od_logger_t *logger, struct berval **values,
			       od_rule_t *rule, od_client_t *client)
{
	char host_db[128];
	od_snprintf(host_db, sizeof(host_db), "%s_%s", rule->storage->host,
		    client->startup.database.value);

	int len = ldap_count_values_len(values);

	if (len == 0) {
		od_error(logger, "auth_ldap", client, NULL,
			 "empty search storage credentials result");
		return LDAP_INSUFFICIENT_ACCESS;
	}

	for (int i = 0; i < len; i++) {
		if (strstr((char *)values[i]->bv_val, host_db)) {
			od_list_t *j;
			od_list_foreach (&rule->ldap_storage_creds_list, j) {
				od_ldap_storage_credentials_t *lsc = NULL;
				lsc = od_container_of(
					j, od_ldap_storage_credentials_t, link);
				char host_db_user[256];
				od_snprintf(host_db_user, sizeof(host_db_user),
					    "%s_%s", host_db, lsc->name);

				if (strstr((char *)values[i]->bv_val,
					   host_db_user)) {
					od_debug(logger, "auth_ldap", client,
						 NULL, "matched group %s",
						 (char *)values[i]->bv_val);
					od_ldap_change_storage_credentials(
						logger, lsc, client);
					return LDAP_SUCCESS;
				}
			}
		}
	}

	od_error(logger, "auth_ldap", client, NULL,
		 "error: route does not match any user attribute");

	return LDAP_INSUFFICIENT_ACCESS;
}

/*---------------------------------------------------------------------------------------- */

/* ldap endpoints ADD/REMOVE API */
od_ldap_endpoint_t *od_ldap_endpoint_alloc(void)
{
	od_ldap_endpoint_t *le = od_malloc(sizeof(od_ldap_endpoint_t));
	if (le == NULL) {
		return NULL;
	}
	od_list_init(&le->link);

	le->name = NULL;

	le->ldapserver = NULL;
	le->ldapport = 0;

	le->ldapscheme = NULL;

	le->ldapprefix = NULL;
	le->ldapsuffix = NULL;
	le->ldapbindpasswd = NULL;
	le->ldapsearchfilter = NULL;
	le->ldapsearchattribute = NULL;
	le->ldapscope = NULL;
	le->ldapbasedn = NULL;
	le->ldapbinddn = NULL;
	/* preparsed connect url */
	le->ldapurl = NULL;

	atomic_store(&le->refs, 1);

	return le;
}

od_ldap_endpoint_t *od_ldap_endpoint_ref(od_ldap_endpoint_t *le)
{
	atomic_fetch_add(&le->refs, 1);

	return le;
}

od_retcode_t od_ldap_endpoint_free(od_ldap_endpoint_t *le)
{
	if (atomic_fetch_sub(&le->refs, 1) > 1) {
		return OK_RESPONSE;
	}

	if (le->name) {
		od_free(le->name);
	}

	if (le->ldapserver) {
		od_free(le->ldapserver);
	}
	if (le->ldapscheme) {
		od_free(le->ldapscheme);
	}

	if (le->ldapprefix) {
		od_free(le->ldapprefix);
	}
	if (le->ldapsuffix) {
		od_free(le->ldapsuffix);
	}
	if (le->ldapbindpasswd) {
		od_free(le->ldapbindpasswd);
	}
	if (le->ldapsearchfilter) {
		od_free(le->ldapsearchfilter);
	}
	if (le->ldapsearchattribute) {
		od_free(le->ldapsearchattribute);
	}
	if (le->ldapscope) {
		od_free(le->ldapscope);
	}
	if (le->ldapbasedn) {
		od_free(le->ldapbasedn);
	}
	if (le->ldapbinddn) {
		od_free(le->ldapbinddn);
	}
	/* preparsed connect url */
	if (le->ldapurl) {
		od_free(le->ldapurl);
	}

	od_list_unlink(&le->link);

	od_free(le);

	return OK_RESPONSE;
}

od_ldap_storage_credentials_t *od_ldap_storage_credentials_alloc(void)
{
	od_ldap_storage_credentials_t *lsc =
		od_malloc(sizeof(od_ldap_storage_credentials_t));
	if (lsc == NULL) {
		return NULL;
	}
	od_list_init(&lsc->link);

	lsc->name = NULL;

	lsc->lsc_username = NULL;
	lsc->lsc_password = NULL;

	atomic_store(&lsc->refs, 1);

	return lsc;
}

od_ldap_storage_credentials_t *
od_ldap_storage_credentials_ref(od_ldap_storage_credentials_t *lsc)
{
	atomic_fetch_add(&lsc->refs, 1);

	return lsc;
}

od_retcode_t
od_ldap_storage_credentials_free(od_ldap_storage_credentials_t *lsc)
{
	if (atomic_fetch_sub(&lsc->refs, 1) > 1) {
		return OK_RESPONSE;
	}

	if (lsc->name) {
		od_free(lsc->name);
	}

	if (lsc->lsc_username) {
		od_free(lsc->lsc_username);
	}

	if (lsc->lsc_password) {
		od_free(lsc->lsc_password);
	}

	od_list_unlink(&lsc->link);

	od_free(lsc);

	return OK_RESPONSE;
}

od_retcode_t od_ldap_endpoint_add(od_ldap_endpoint_t *ldaps,
				  od_ldap_endpoint_t *target)
{
	od_list_t *i;

	od_list_foreach (&(ldaps->link), i) {
		od_ldap_endpoint_t *s =
			od_container_of(i, od_ldap_endpoint_t, link);
		if (strcmp(s->name, target->name) == 0) {
			/* already loaded */
			return NOT_OK_RESPONSE;
		}
	}

	od_list_append(&ldaps->link, &target->link);

	return OK_RESPONSE;
}

od_ldap_endpoint_t *od_ldap_endpoint_find(od_list_t *ldaps, char *name)
{
	od_list_t *i;

	od_list_foreach (ldaps, i) {
		od_ldap_endpoint_t *serv =
			od_container_of(i, od_ldap_endpoint_t, link);
		if (strcmp(serv->name, name) == 0) {
			return serv;
		}
	}

	/* target ldap server was not found */
	return NULL;
}

od_retcode_t od_ldap_endpoint_remove(od_ldap_endpoint_t *ldaps,
				     od_ldap_endpoint_t *target)
{
	od_list_t *i;

	od_list_foreach (&ldaps->link, i) {
		od_ldap_endpoint_t *serv =
			od_container_of(i, od_ldap_endpoint_t, link);
		if (strcmp(serv->name, target->name) == 0) {
			od_list_unlink(&target->link);
			return OK_RESPONSE;
		}
	}

	/* target ldap server was not found */
	return NOT_OK_RESPONSE;
}

od_ldap_storage_credentials_t *
od_ldap_storage_credentials_find(od_list_t *ldap_storage_creds_list, char *name)
{
	od_list_t *i;

	od_list_foreach (ldap_storage_creds_list, i) {
		od_ldap_storage_credentials_t *lsc =
			od_container_of(i, od_ldap_storage_credentials_t, link);
		if (strcmp(lsc->name, name) == 0) {
			return lsc;
		}
	}

	/* target storage user was not found */
	return NULL;
}

static od_thread_pool_t workers;

int od_ldap_workers_init(size_t count)
{
	return od_thread_pool_init(&workers, "ldap", count, 100);
}

void od_ldap_workers_destroy(void)
{
	od_thread_pool_destroy(&workers);
}

static int is_username_valid(od_client_t *client, const char *name)
{
	/* be sure LDAP-injection is not possible */
	for (size_t i = 0; name[i] != '\0'; ++i) {
		char c = name[i];
		if (c == '*' || c == '(' || c == ')' || c == '\\' || c == '/') {
			od_gerror(
				"auth_ldap", client, NULL,
				"invalid character in user name for LDAP authentication");
			return 0;
		}
	}

	return 1;
}

static int init_and_bind(od_instance_t *instance, od_client_t *client,
			 od_ldap_endpoint_t *endpoint, LDAP **conn,
			 int timeout_sec)
{
	unsigned int tcp_usr_timeout_ms = 23500;

	struct timeval timeout_tv;
	timeout_tv.tv_sec = timeout_sec;
	timeout_tv.tv_usec = 0;

	*conn = NULL;

	int rc = ldap_initialize(conn, endpoint->ldapurl);
	if (rc != LDAP_SUCCESS) {
		od_error(&instance->logger, "auth_ldap", client, NULL,
			 "ldap init failed: %d (%s)", rc, ldap_err2string(rc));
		goto error;
	}

	int ldapversion = LDAP_VERSION3;
	rc = ldap_set_option(*conn, LDAP_OPT_PROTOCOL_VERSION, &ldapversion);
	if (rc != LDAP_SUCCESS) {
		goto error;
	}

	rc = ldap_set_option(*conn, LDAP_OPT_NETWORK_TIMEOUT, &timeout_tv);
	if (rc != LDAP_SUCCESS) {
		goto error;
	}

	rc = ldap_set_option(*conn, LDAP_OPT_TIMELIMIT, &timeout_sec);
	if (rc != LDAP_SUCCESS) {
		goto error;
	}

	rc = ldap_set_option(*conn, LDAP_OPT_TIMEOUT, &timeout_tv);
	if (rc != LDAP_SUCCESS) {
		goto error;
	}

#ifdef LDAP_OPT_TCP_USER_TIMEOUT
	rc = ldap_set_option(*conn, LDAP_OPT_TCP_USER_TIMEOUT,
			     &tcp_usr_timeout_ms);
	if (rc != LDAP_SUCCESS) {
		goto error;
	}
#endif

	const char *binddn = endpoint->ldapbinddn ? endpoint->ldapbinddn : "";
	const char *passwd =
		endpoint->ldapbindpasswd ? endpoint->ldapbindpasswd : "";

	rc = ldap_simple_bind_s(*conn, binddn, passwd);
	if (rc != LDAP_SUCCESS) {
		od_error(
			&instance->logger, "auth_ldap", client, NULL,
			"fail to do initial LDAP bind for ldapbinddn \"%s\" on \"%s\": %d (%s)",
			binddn, endpoint->name, rc, ldap_err2string(rc));
		goto error;
	}

	return LDAP_SUCCESS;

error:
	if (*conn != NULL) {
		ldap_unbind_ext_s(*conn, NULL, NULL);
	}
	*conn = NULL;
	return rc;
}

static int resolve_user(od_instance_t *instance, LDAP *ldap, od_rule_t *rule,
			od_client_t *client)
{
	od_ldap_endpoint_t *endp = rule->ldap_endpoint;

	if (endp->ldapbasedn == NULL) {
		char *auth_user = NULL;
		int rc = od_asprintf(&auth_user, "%s%s%s",
				     endp->ldapprefix ? endp->ldapprefix : "",
				     client->startup.user.value,
				     endp->ldapsuffix ? endp->ldapsuffix : "");
		if (rc == -1) {
			return LDAP_NO_MEMORY;
		}
		if (client->ldap_auth_dn != NULL) {
			od_free(client->ldap_auth_dn);
		}
		client->ldap_auth_dn = auth_user;
		return LDAP_SUCCESS;
	}

	/*
	 * inspired by src/backend/libpq/auth.c
	 */

	char *filter = NULL;
	LDAPMessage *search_message;
	LDAPMessage *entry;
	char *attributes[] = { LDAP_NO_ATTRS, NULL };
	char *dn;
	int count;

	if (rule->ldap_storage_credentials_attr) {
		attributes[0] = rule->ldap_storage_credentials_attr;
	}

	if (endp->ldapsearchattribute) {
		od_asprintf(&filter, "(%s=%s)", endp->ldapsearchattribute,
			    client->startup.user.value);
	} else {
		od_asprintf(&filter, "(uid=%s)", client->startup.user.value);
	}
	if (filter == NULL) {
		return LDAP_NO_MEMORY;
	}

	if (endp->ldapsearchfilter) {
		char *prev_filter = od_strdup(filter);
		od_free(filter);
		if (prev_filter == NULL) {
			return LDAP_NO_MEMORY;
		}

		od_asprintf(&filter, "(&%s%s)", prev_filter,
			    endp->ldapsearchfilter);
		od_free(prev_filter);
		if (filter == NULL) {
			return LDAP_NO_MEMORY;
		}
	}

	int rc = ldap_search_s(ldap, endp->ldapbasedn, LDAP_SCOPE_SUBTREE,
			       filter, attributes, 0, &search_message);
	od_debug(&instance->logger, "auth_ldap", client, NULL,
		 "basedn search entries with filter: %s and attrib %s", filter,
		 attributes[0]);
	od_free(filter);

	if (rc != LDAP_SUCCESS) {
		od_error(&instance->logger, "auth_ldap", client, NULL,
			 "basedn search failed: %d (%s)", rc,
			 ldap_err2string(rc));
		return rc;
	}

	count = ldap_count_entries(ldap, search_message);
	if (count != 1) {
		od_error(&instance->logger, "auth_ldap", client, NULL,
			 "invalid basedn search entries count: %d", count);
		ldap_msgfree(search_message);
		return LDAP_INSUFFICIENT_ACCESS;
	}

	entry = ldap_first_entry(ldap, search_message);
	dn = ldap_get_dn(ldap, entry);
	if (dn == NULL) {
		int err = LDAP_OTHER;
		ldap_get_option(ldap, LDAP_OPT_ERROR_NUMBER, &err);
		od_error(&instance->logger, "auth_ldap", client, NULL,
			 "ldap_get_dn failed: %d (%s)", err,
			 ldap_err2string(err));
		ldap_msgfree(search_message);
		return err;
	}

	if (rule->ldap_storage_credentials_attr) {
		struct berval **values = NULL;
		values = ldap_get_values_len(ldap, entry, attributes[0]);
		rc = search_storage_credentials(&instance->logger, values, rule,
						client);
		if (rc != LDAP_SUCCESS) {
			ldap_memfree(dn);
			ldap_value_free_len(values);
			ldap_msgfree(search_message);
			return rc;
		}

		ldap_value_free_len(values);
	}

	char *auth_user = od_strdup(dn);
	ldap_memfree(dn);
	ldap_msgfree(search_message);

	if (auth_user == NULL) {
		return LDAP_NO_MEMORY;
	}

	if (client->ldap_auth_dn != NULL) {
		od_free(client->ldap_auth_dn);
	}
	client->ldap_auth_dn = auth_user;

	return LDAP_SUCCESS;
}

static int ldap_auth_impl(od_client_t *client, kiwi_password_t *password)
{
	od_instance_t *instance = client->global->instance;
	od_rule_t *rule = client->rule;
	LDAP *ldap = NULL;

	int rc = init_and_bind(instance, client, rule->ldap_endpoint, &ldap,
			       5 /* seconds */);
	if (rc != LDAP_SUCCESS) {
		goto error;
	}

	/* can be resolved at routing time */
	if (client->ldap_auth_dn == NULL) {
		rc = resolve_user(instance, ldap, rule, client);
		if (rc != LDAP_SUCCESS) {
			goto error;
		}
	}

	rc = ldap_simple_bind_s(ldap, client->ldap_auth_dn, password->password);
	if (rc != LDAP_SUCCESS) {
		goto error;
	}

	ldap_unbind_ext_s(ldap, NULL, NULL);

	return LDAP_SUCCESS;

error:
	od_error(&instance->logger, "auth_ldap", client, NULL,
		 "ldap auth failed with code %d (%s)", rc, ldap_err2string(rc));

	if (ldap != NULL) {
		ldap_unbind_ext_s(ldap, NULL, NULL);
	}
	return rc;
}

static int resolve_storage_credentials_impl(od_client_t *client,
					    od_rule_t *rule)
{
	od_instance_t *instance = client->global->instance;
	LDAP *ldap = NULL;

	int rc = init_and_bind(instance, client, rule->ldap_endpoint, &ldap,
			       5 /* seconds */);
	if (rc != LDAP_SUCCESS) {
		goto error;
	}

	/* will set client->ldap_auth_dn */
	rc = resolve_user(instance, ldap, rule, client);
	if (rc != LDAP_SUCCESS) {
		goto error;
	}

	ldap_unbind_ext_s(ldap, NULL, NULL);

	return LDAP_SUCCESS;

error:
	od_error(&instance->logger, "auth_ldap", client, NULL,
		 "ldap storage crenedtials resolving failed with code %d (%s)",
		 rc, ldap_err2string(rc));

	if (ldap != NULL) {
		ldap_unbind_ext_s(ldap, NULL, NULL);
	}

	return rc;
}

typedef struct {
	od_client_t *client;
	kiwi_password_t *password;

	int rc;
} auth_arg_t;

static void *do_ldap_auth(void *a)
{
	auth_arg_t *arg = a;

	arg->rc = ldap_auth_impl(arg->client, arg->password);

	return NULL;
}

int od_auth_ldap(od_client_t *client, kiwi_password_t *cl_password)
{
	od_instance_t *instance = client->global->instance;
	od_rule_t *rule = client->rule;

	if (!is_username_valid(client, client->startup.user.value)) {
		return NOT_OK_RESPONSE;
	}

	auth_arg_t arg;
	memset(&arg, 0, sizeof(auth_arg_t));
	arg.client = client;
	arg.password = cl_password;

	od_future_t *future = od_thread_pool_submit(&workers, do_ldap_auth,
						    &arg, NULL, NULL, 0);
	if (future == NULL) {
		int err = mm_errno_get();
		od_error(&instance->logger, "auth_ldap", client, NULL,
			 "can't submit ldap auth task, errno=%d (%s)", err,
			 strerror(err));
		return NOT_OK_RESPONSE;
	}

	if (od_thread_pool_wait(future, UINT32_MAX)) {
		int err = mm_errno_get();
		od_error(&instance->logger, "auth_ldap", client, NULL,
			 "ldap auth wait error, errno=%d (%s)", err,
			 strerror(err));
		od_future_unref(future);

		return NOT_OK_RESPONSE;
	}

	od_future_unref(future);

	if (arg.rc != LDAP_SUCCESS && rule->client_fwd_error) {
		od_ldap_error_report_client(client, arg.rc);
	}

	return arg.rc == LDAP_SUCCESS ? OK_RESPONSE : NOT_OK_RESPONSE;
}

typedef struct {
	od_client_t *client;
	od_rule_t *rule;

	int rc;
} resolve_storage_creds_arg_t;

static void *do_resolve_storage_credentials(void *a)
{
	resolve_storage_creds_arg_t *arg = a;

	arg->rc = resolve_storage_credentials_impl(arg->client, arg->rule);

	return NULL;
}

int od_auth_ldap_resolve_storage_credentials(od_client_t *client,
					     od_rule_t *rule)
{
	od_instance_t *instance = client->global->instance;

	if (!is_username_valid(client, client->startup.user.value)) {
		return NOT_OK_RESPONSE;
	}

	resolve_storage_creds_arg_t arg;
	memset(&arg, 0, sizeof(resolve_storage_creds_arg_t));

	arg.client = client;
	arg.rule = rule;

	od_future_t *future = od_thread_pool_submit(
		&workers, do_resolve_storage_credentials, &arg, NULL, NULL, 0);
	if (future == NULL) {
		int err = mm_errno_get();
		od_error(&instance->logger, "auth_ldap", client, NULL,
			 "can't submit ldap auth task, errno=%d (%s)", err,
			 strerror(err));
		return NOT_OK_RESPONSE;
	}

	if (od_thread_pool_wait(future, UINT32_MAX)) {
		int err = mm_errno_get();
		od_error(&instance->logger, "auth_ldap", client, NULL,
			 "ldap auth wait error, errno=%d (%s)", err,
			 strerror(err));
		od_future_unref(future);

		return NOT_OK_RESPONSE;
	}

	od_future_unref(future);

	if (arg.rc != LDAP_SUCCESS && rule->client_fwd_error) {
		od_ldap_error_report_client(client, arg.rc);
	}

	return arg.rc == LDAP_SUCCESS ? OK_RESPONSE : NOT_OK_RESPONSE;
}

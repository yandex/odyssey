
#ifndef OD_ODYSSEY_LDAP_ENDPOINT_H
#define OD_ODYSSEY_LDAP_ENDPOINT_H

typedef struct {
	char *name;

	char *ldapserver;
	uint64_t ldapport;

	// either null, ldap or ldaps
	char *ldapscheme;

	char *ldapprefix;
	char *ldapsuffix;
	char *ldapbindpasswd;
	char *ldapsearchfilter;
	char *ldapsearchattribute;
	char *ldapscope;

	char *ldapbasedn;
	char *ldapbinddn;
	// preparsed connect url
	char *ldapurl;

	od_list_t link;
} od_ldap_endpoint_t;

/* ldap endpoints ADD/REMOVE API */

extern od_retcode_t od_ldap_endpoint_prepare(od_ldap_endpoint_t *);

extern od_retcode_t od_ldap_endpoint_add(od_ldap_endpoint_t *ldaps,
					 od_ldap_endpoint_t *target);

extern od_ldap_endpoint_t *od_ldap_endpoint_find(od_list_t *ldaps,
						 char *target);

extern od_retcode_t od_ldap_endpoint_remove(od_ldap_endpoint_t *ldaps,
					    od_ldap_endpoint_t *target);
// -------------------------------------------------------
extern od_ldap_endpoint_t *od_ldap_endpoint_alloc();
extern od_retcode_t od_ldap_endpoint_init(od_ldap_endpoint_t *);
extern od_retcode_t od_ldap_endpoint_free(od_ldap_endpoint_t *le);
typedef struct {
	char *name;
	char *lsu_username;
	char *lsu_password;

	od_list_t link;
} od_ldap_storage_user_t;

extern od_ldap_storage_user_t *
od_ldap_storage_user_find(od_list_t *storage_users, char *target);

// -------------------------------------------------------
extern od_ldap_storage_user_t *od_ldap_storage_user_alloc();
extern od_retcode_t od_ldap_storage_user_free(od_ldap_storage_user_t *lsu);
#endif /* OD_ODYSSEY_LDAP_ENDPOINT_H */

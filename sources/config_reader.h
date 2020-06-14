#ifndef ODYSSEY_CONFIG_READER_H
#define ODYSSEY_CONFIG_READER_H

/*
 * Odyssey.
 *
 * Scalable PostgreSQL connection pooler.
 */

int
od_config_reader_import(od_config_t *,
                        od_rules_t *,
                        od_error_t *,
                        od_module_t *,
                        char *);

#endif /* ODYSSEY_CONFIG_READER_H */

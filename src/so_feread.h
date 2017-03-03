#ifndef SO_FEREAD_H_
#define SO_FEREAD_H_

/*
 * soprano.
 *
 * Protocol-level PostgreSQL client library.
*/

int so_feread_ready(int*, uint8_t*, uint32_t);
int so_feread_key(so_key_t*, uint8_t*, uint32_t);
int so_feread_auth(uint32_t*, uint8_t[4], uint8_t*, uint32_t);
int so_feread_parameter(so_paramlist_t*, uint8_t*, uint32_t);

#endif

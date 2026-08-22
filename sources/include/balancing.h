#pragma once

#include <stddef.h>
#include <stdatomic.h>

#include <types.h>
#include <common_const.h>

typedef enum {
	OD_BALANCING_METHOD_UNDEF,
	OD_BALANCING_METHOD_ROUNDROBIN,
	OD_BALANCING_METHOD_LEASTCONN,
	OD_BALANCING_METHOD_WEIGHTED,
	OD_BALANCING_METHOD_WEIGHTED_LEASTCONN,
	OD_BALANCING_METHOD_RESPONSETIME,
} od_balancing_method_t;

od_balancing_method_t od_balancing_method_from_str(const char *str, size_t len);

const char *od_balancing_method_to_str(od_balancing_method_t method);

typedef struct {
	/*
	 * high 32 bit is counter for local endpoints
	 * low 32 bit is counter for global endpoints
	 */
	atomic_uint_fast64_t counter;
} od_method_roundroubin_t;

typedef struct {
	/* TODO: do not use char* here */
	char *host;
	double weight;
} od_balancing_host_weight_t;

typedef struct {
	size_t nweights;
	od_balancing_host_weight_t *weights;
} od_method_weighted_t;

typedef struct {
	size_t nweights;
	od_balancing_host_weight_t *weights;
} od_method_weighted_leastconn_t;

typedef struct {
	double time_weight;
	double conn_weight;
} od_method_responsetime_t;

typedef struct {
	struct {
		od_balancing_method_t type;
		int az_aware;
		od_method_roundroubin_t roundrobin;
		od_method_weighted_t weighted;
		od_method_weighted_leastconn_t weighted_leastconn;
		od_method_responsetime_t responsetime;
	} method;

	int debug_notice;
} od_storage_balancing_t;

void od_storage_balancing_init(od_storage_balancing_t *b);
void od_storage_balancing_destroy(od_storage_balancing_t *b);

typedef int (*od_balancing_filter_fn)(od_storage_endpoint_t *endp, void *arg);

void od_storage_balancing_copy(od_storage_balancing_t *dest,
			       const od_storage_balancing_t *src);

size_t od_storage_balancing_select(od_storage_balancing_t *b,
				   od_balancing_method_t override_method,
				   od_rule_storage_t *storage,
				   od_route_t *route,
				   od_storage_endpoint_t **out, size_t max,
				   od_balancing_filter_fn filter, void *arg);

od_balancing_method_t od_balancing_get_effective(od_client_t *client,
						 od_rule_storage_t *storage);

#include <odyssey.h>

#include <storage.h>
#include <balancing.h>
#include <global.h>
#include <instance.h>
#include <multi_pool.h>
#include <route.h>
#include <util.h>

#define METHOD_RR_STR "roundrobin"
#define METHOD_LEASTCONN_STR "leastconn"
#define METHOD_WEIGHTED_STR "weighted"
#define METHOD_WEIGHTED_LEASTCONN "weightedleastconn"
#define METHOD_RESPONSETIME "responsetime"

od_balancing_method_t od_balancing_method_from_str(const char *str, size_t len)
{
	if (len == strlen(METHOD_RR_STR) &&
	    strncasecmp(METHOD_RR_STR, str, len) == 0) {
		return OD_BALANCING_METHOD_ROUNDROBIN;
	}

	if (len == strlen(METHOD_LEASTCONN_STR) &&
	    strncasecmp(METHOD_LEASTCONN_STR, str, len) == 0) {
		return OD_BALANCING_METHOD_LEASTCONN;
	}

	if (len == strlen(METHOD_WEIGHTED_STR) &&
	    strncasecmp(METHOD_WEIGHTED_STR, str, len) == 0) {
		return OD_BALANCING_METHOD_WEIGHTED;
	}

	if (len == strlen(METHOD_WEIGHTED_LEASTCONN) &&
	    strncasecmp(METHOD_WEIGHTED_LEASTCONN, str, len) == 0) {
		return OD_BALANCING_METHOD_WEIGHTED_LEASTCONN;
	}

	if (len == strlen(METHOD_RESPONSETIME) &&
	    strncasecmp(METHOD_RESPONSETIME, str, len) == 0) {
		return OD_BALANCING_METHOD_RESPONSETIME;
	}

	return OD_BALANCING_METHOD_UNDEF;
}

void od_storage_balancing_init(od_storage_balancing_t *b)
{
	memset(b, 0, sizeof(od_storage_balancing_t));

	b->method.type = OD_BALANCING_METHOD_UNDEF;
	atomic_init(&b->method.roundrobin.counter, 0);

	b->debug_notice = 0;
}

void od_storage_balancing_destroy(od_storage_balancing_t *b)
{
	od_free(b->method.weighted.weights);
	od_free(b->method.weighted_leastconn.weights);
}

void od_storage_balancing_copy(od_storage_balancing_t *dest,
			       const od_storage_balancing_t *src)
{
	memcpy(dest, src, sizeof(od_storage_balancing_t));
}

static int in_same_az(od_instance_t *instance,
		      const od_storage_endpoint_t *endp)
{
	return strcmp(instance->config.availability_zone,
		      endp->address.availability_zone) == 0;
}

#define RR_PACK(l, g) (((((uint64_t)(l)) << 32) | (uint64_t)(g)))
#define RR_LOCAL_UNPACK(v) ((uint32_t)((v) >> 32))
#define RR_GLOBAL_UNPACK(v) ((uint32_t)((v) & 0xffffffffu))

static void rr_next(od_method_roundroubin_t *rr, uint32_t *local,
		    uint32_t *global)
{
	while (1) {
		uint64_t curr = atomic_load(&rr->counter);

		uint32_t l = RR_LOCAL_UNPACK(curr);
		uint32_t g = RR_GLOBAL_UNPACK(curr);

		uint64_t new =
			RR_PACK((l + 1) & 0xffffffffu, (g + 1) & 0xffffffffu);

		if (atomic_compare_exchange_strong(&rr->counter, &curr, new)) {
			*local = l;
			*global = g;
			return;
		}
	}
}

static size_t roundrobin(od_storage_balancing_t *b, od_rule_storage_t *storage,
			 od_storage_endpoint_t **out, size_t max,
			 od_balancing_filter_fn filter, void *arg)
{
	/*
	 * assume the endpoints list contains several hosts,
	 * some of them is in same AZ(+) as instance is, some
	 * in different AZ(-):
	 * [ h1+, h2-, h3-, h4+, h5-, h6- ]
	 *
	 * lets stable-sort them by AZ locality
	 * (with O(n^2) algo, assuming there are no many endpoints):
	 * [ h1+, h4+, h2-, h3-, h5-, h6- ]
	 *
	 * and then let it be two RR counters: for local and global hosts
	 * [ h1+, h4+, h2-, h3-, h5-, h6- ]
	 *         ^              ^
	 *    (AZ idx 1)    (non-AZ idx 2)
	 *
	 * after getting corresponding indexes, lets
	 * rotate each subarray to get resulting
	 * round robin order of connection:
	 * [ h4+, h1+, h5-, h6-, h2-, h3- ]
	 */

	assert(storage->endpoints_count > 1);

	od_instance_t *instance = od_global_get_instance();

	uint32_t local, global;
	rr_next(&b->method.roundrobin, &local, &global);

	size_t count = 0;
	for (size_t i = 0; i < storage->endpoints_count && count < max; ++i) {
		od_storage_endpoint_t *e = &storage->endpoints[i];

		if (filter != NULL && !filter(e, arg)) {
			continue;
		}

		out[count++] = e;
	}

	size_t first_global = 0;
	size_t nlocal = 0;
	size_t nglobal = 0;

	for (size_t i = 0; i < count; ++i) {
		od_storage_endpoint_t *e = out[i];

		if (!in_same_az(instance, e)) {
			++nglobal;
			continue;
		}
		++nlocal;

		if (i != first_global) {
			size_t n = i - first_global;
			memmove(&out[first_global + 1], &out[first_global],
				n * sizeof(out[0]));
			out[first_global] = e;
		}

		++first_global;
	}

	if (nlocal > 0) {
		od_array_rotate_left_p((void **)out, nlocal, local);
	}

	if (nglobal > 0) {
		od_array_rotate_left_p((void **)(&out[nlocal]), nglobal,
				       global);
	}

	return count;
}

static int addr_filter(void *arg, const od_multi_pool_key_t *key)
{
	const od_storage_endpoint_t *endp = (const od_storage_endpoint_t *)arg;

	return od_address_cmp(&key->address, &endp->address) == 0;
}

static int conncount_cmp(void *arg, const void *a, const void *b)
{
	od_route_t *route = arg;
	od_instance_t *instance = od_global_get_instance();

	const od_storage_endpoint_t *f = (const od_storage_endpoint_t *)a;
	const od_storage_endpoint_t *s = (const od_storage_endpoint_t *)b;

	if (f == s || od_address_cmp(&f->address, &s->address) == 0) {
		return 0;
	}

	int f_az = in_same_az(instance, f);
	int s_az = in_same_az(instance, s);

	if (f_az != s_az) {
		/* let az-local hosts be the first */
		return s_az - f_az;
	}

	od_multi_pool_t *mp = od_route_server_pools(route);
	int f_cnt = od_multi_pool_total_locked(mp, addr_filter, (void *)f);
	int s_cnt = od_multi_pool_total_locked(mp, addr_filter, (void *)s);

	/* let less count be the first */
	return f_cnt - s_cnt;
}

static size_t leastconn(od_storage_balancing_t *b, od_route_t *route,
			od_storage_endpoint_t **out, size_t max,
			od_balancing_filter_fn filter, void *arg)
{
	(void)b;

	od_rule_storage_t *storage = route->rule->storage;

	size_t count = 0;
	for (size_t i = 0; i < storage->endpoints_count && count < max; ++i) {
		od_storage_endpoint_t *e = &storage->endpoints[i];

		if (filter != NULL && !filter(e, arg)) {
			continue;
		}

		out[count++] = e;
	}

	/*
	 * get the lock so we have a consistent conn counts
	 * TODO: do not do it?
	 */
	od_route_lock(route);

	od_insertion_sort_p((void **)out, count, conncount_cmp, route);

	od_route_unlock(route);

	return count;
}

static size_t weighted(od_storage_balancing_t *b, od_route_t *route,
		       od_storage_endpoint_t **out, size_t max,
		       od_balancing_filter_fn filter, void *arg)
{
	(void)b;
	(void)route;
	(void)out;
	(void)max;
	(void)filter;
	(void)arg;

	/* not implemented */
	abort();
}

static size_t weighted_leastconn(od_storage_balancing_t *b, od_route_t *route,
				 od_storage_endpoint_t **out, size_t max,
				 od_balancing_filter_fn filter, void *arg)
{
	(void)b;
	(void)route;
	(void)out;
	(void)max;
	(void)filter;
	(void)arg;

	/* not implemented */
	abort();
}

static size_t responsetime(od_storage_balancing_t *b, od_route_t *route,
			   od_storage_endpoint_t **out, size_t max,
			   od_balancing_filter_fn filter, void *arg)
{
	(void)b;
	(void)route;
	(void)out;
	(void)max;
	(void)filter;
	(void)arg;

	/* not implemented */
	abort();
}

size_t od_storage_balancing_select(od_storage_balancing_t *b, od_route_t *route,
				   od_storage_endpoint_t **out, size_t max,
				   od_balancing_filter_fn filter, void *arg)
{
	od_rule_t *rule = route->rule;
	od_rule_storage_t *storage = rule->storage;

	if (max == 0 || storage->endpoints_count == 0) {
		return 0;
	}

	if (storage->endpoints_count == 1) {
		out[0] = &storage->endpoints[0];
		return 1;
	}

	switch (b->method.type) {
	case OD_BALANCING_METHOD_UNDEF:
	case OD_BALANCING_METHOD_ROUNDROBIN:
		return roundrobin(b, storage, out, max, filter, arg);
	case OD_BALANCING_METHOD_LEASTCONN:
		return leastconn(b, route, out, max, filter, arg);
	case OD_BALANCING_METHOD_WEIGHTED:
		return weighted(b, route, out, max, filter, arg);
	case OD_BALANCING_METHOD_WEIGHTED_LEASTCONN:
		return weighted_leastconn(b, route, out, max, filter, arg);
	case OD_BALANCING_METHOD_RESPONSETIME:
		return responsetime(b, route, out, max, filter, arg);
	default:
		abort();
	}
}

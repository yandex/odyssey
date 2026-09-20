#include <string.h>

#include <machinarium/ds/hash.h>

#define prime64_1 (0x9E3779B185EBCA87ULL)
#define prime64_2 (0xC2B2AE3D27D4EB4FULL)
#define prime64_3 (0x165667B19E3779F9ULL)
#define prime64_4 (0x85EBCA77C2B2AE63ULL)
#define prime64_5 (0x27D4EB2F165667C5ULL)

static uint64_t circ_lshift(uint64_t x, int n)
{
	return (x << n) | (x >> (64 - n));
}

static inline uint64_t xxh64_round(uint64_t accn, uint64_t lanen)
{
	accn = accn + (lanen * prime64_2);
	accn = circ_lshift(accn, 31);
	return accn * prime64_1;
}

static inline uint64_t xxh64_merge(uint64_t acc, uint64_t accn)
{
	acc = acc ^ xxh64_round(0, accn);
	acc = acc * prime64_1;
	return acc + prime64_4;
}

static inline uint64_t read_u64(const void *b)
{
	uint64_t v;
	memcpy(&v, b, sizeof(uint64_t));
	return v;
}

static inline uint32_t read_u32(const void *b)
{
	uint32_t v;
	memcpy(&v, b, sizeof(uint32_t));
	return v;
}

mm_hash_t mm_xxh64_hash(const void *data, size_t size, uint64_t seed)
{
	const uint8_t *p = (const uint8_t *)data;
	const uint8_t *end = p + size;
	uint64_t acc;

	if (size >= 4 * sizeof(uint64_t)) {
		uint64_t acc1, acc2, acc3, acc4;

		acc1 = seed + prime64_1 + prime64_2;
		acc2 = seed + prime64_2;
		acc3 = seed + 0;
		acc4 = seed - prime64_1;

		const uint8_t *limit = end - 4 * sizeof(uint64_t);
		while (p <= limit) {
			acc1 = xxh64_round(acc1, read_u64(p));
			p += sizeof(uint64_t);

			acc2 = xxh64_round(acc2, read_u64(p));
			p += sizeof(uint64_t);

			acc3 = xxh64_round(acc3, read_u64(p));
			p += sizeof(uint64_t);

			acc4 = xxh64_round(acc4, read_u64(p));
			p += sizeof(uint64_t);
		}

		acc = circ_lshift(acc1, 1) + circ_lshift(acc2, 7) +
		      circ_lshift(acc3, 12) + circ_lshift(acc4, 18);
		acc = xxh64_merge(acc, acc1);
		acc = xxh64_merge(acc, acc2);
		acc = xxh64_merge(acc, acc3);
		acc = xxh64_merge(acc, acc4);
	} else {
		acc = seed + prime64_5;
	}

	acc = acc + size;

	while (p + sizeof(uint64_t) <= end) {
		acc = acc ^ xxh64_round(0, read_u64(p));
		acc = circ_lshift(acc, 27) * prime64_1 + prime64_4;
		p += sizeof(uint64_t);
	}

	while (p + sizeof(uint32_t) <= end) {
		acc = acc ^ ((uint64_t)read_u32(p) * prime64_1);
		acc = circ_lshift(acc, 23) * prime64_2 + prime64_3;
		p += sizeof(uint32_t);
	}

	while (p < end) {
		acc = acc ^ ((uint64_t)*p * prime64_5);
		acc = circ_lshift(acc, 11) * prime64_1;
		++p;
	}

	acc = acc ^ (acc >> 33);
	acc = acc * prime64_2;
	acc = acc ^ (acc >> 29);
	acc = acc * prime64_3;
	acc = acc ^ (acc >> 32);

	return acc;
}

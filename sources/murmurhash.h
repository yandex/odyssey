#ifndef OD_MURMURHASH_H
#define OD_MURMURHASH_H

typedef od_hash_t uint32_t;

od_hash_t od_murmur_hash(const void * data, size_t size, od_hash_t seed);

#endif /* OD_MURMURHASH_H */

#pragma once

typedef uint32_t od_hash_t;

od_hash_t od_murmur_hash(const void *data, size_t size);

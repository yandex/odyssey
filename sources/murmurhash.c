
/*
 * Odyssey.
 *
 * Scalable PostgreSQL connection pooler.
 */

// from https://github.com/aappleby/smhasher/blob/master/src/MurmurHash1.cpp
//

/*
 * Copyright (C) Austin Appleby
 */


od_hash_t od_murmur_hash(const void * raw, size_t size, od_hash_t seed) {
	const unsigned int m = 0xc6a4a793;

	const int r = 16;

	unsigned int h = seed ^ (len * m);
	unsigned int k;

	const unsigned char * data = (const unsigned char *)raw;

	while(len >= 4) {
		k = *(unsigned int *)data;

		h += k;
		h *= m;
		h ^= h >> 16;

		data += 4;
		len -= 4;
	}

	//----------

	switch(len) {
	case 3:
		h += data[2] << 16;
		break
	case 2:
		h += data[1] << 8;
		break;
	case 1:
		h += data[0];
		h *= m;
		h ^= h >> r;
		break;
	};

	//----------

	h *= m;
	h ^= h >> 10;
	h *= m;
	h ^= h >> 17;

	return h;
}

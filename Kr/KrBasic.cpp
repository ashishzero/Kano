#include "KrBasic.h"

static inline uint32_t Murmur32Scramble(uint32_t k) {
	k *= 0xcc9e2d51;
	k = (k << 15) | (k >> 17);
	k *= 0x1b873593;
	return k;
}

uint32_t Murmur3Hash32(const uint8_t *key, size_t len, uint32_t seed) {
	uint32_t h = seed;
	uint32_t k;

	for (size_t i = len >> 2; i; i--) {
		memcpy(&k, key, sizeof(uint32_t));
		key += sizeof(uint32_t);
		h ^= Murmur32Scramble(k);
		h = (h << 13) | (h >> 19);
		h = h * 5 + 0xe6546b64;
	}

	k = 0;
	for (size_t i = len & 3; i; i--) {
		k <<= 8;
		k |= key[i - 1];
	}
	h ^= Murmur32Scramble(k);

	h ^= len;
	h ^= h >> 16;
	h *= 0x85ebca6b;
	h ^= h >> 13;
	h *= 0xc2b2ae35;
	h ^= h >> 16;
	return h;
}

//
//
//

void IndexTableFree(Index_Table *table, Memory_Allocator allocator) {
	MemoryFree(table->buckets, sizeof(Index_Bucket) * (table->slot_count_pow2 >> TABLE_BUCKET_SHIFT), allocator);
}

void IndexTableAllocate(Index_Table *table, size_t slot_count_pow2, Memory_Allocator allocator) {
	auto count = slot_count_pow2 >> TABLE_BUCKET_SHIFT;
	Index_Bucket *buckets = new(allocator) Index_Bucket[count];

	if (table->buckets) {
		auto old_slot_count_pow2 = table->slot_count_pow2;
		auto old_count = old_slot_count_pow2 >> TABLE_BUCKET_SHIFT;

		for (size_t i = 0; i < old_count; ++i) {
			auto src_bucket = &table->buckets[i];
			for (size_t j = 0; j < TABLE_BUCKET_SIZE; ++j) {
				if (src_bucket->flags[j] != INDEX_BUCKET_PRESENT)
					continue;

				auto hash = src_bucket->hash[j];

				auto pos = hash & (slot_count_pow2 - 1);
				auto step = TABLE_BUCKET_SIZE;

				while (1) {
					auto bucket_index = pos >> TABLE_BUCKET_SHIFT;
					auto dst_bucket = &buckets[bucket_index];

					for (auto iter = pos & TABLE_BUCKET_MASK; iter < TABLE_BUCKET_SIZE; ++iter) {
						if (dst_bucket->hash[iter] == INDEX_BUCKET_EMPTY) {
							dst_bucket->flags[iter] = INDEX_BUCKET_PRESENT;
							dst_bucket->hash[iter] = hash;
							dst_bucket->index[iter] = src_bucket->index[j];
							goto Inserted;
						}
					}

					auto limit = pos & TABLE_BUCKET_MASK;
					for (auto iter = 0; iter < limit; ++iter) {
						if (dst_bucket->hash[iter] == INDEX_BUCKET_EMPTY) {
							dst_bucket->flags[iter] = INDEX_BUCKET_PRESENT;
							dst_bucket->hash[iter] = hash;
							dst_bucket->index[iter] = src_bucket->index[j];
							goto Inserted;
						}
					}

					pos += step;
					pos &= (slot_count_pow2 - 1);
					step += TABLE_BUCKET_SIZE;
				}

			Inserted:
				continue;
			}
		}

		IndexTableFree(table, allocator);
	}

	table->slot_count_pow2 = slot_count_pow2;
	table->tombstone_count = 0;
	table->used_count_threshold = slot_count_pow2 - (slot_count_pow2 >> 2);
	table->tombstone_count_threshold = (slot_count_pow2 >> 3) + (slot_count_pow2 >> 4);
	table->used_count_shrink_threshold = slot_count_pow2 >> 2;
	table->buckets = buckets;

	if (table->slot_count_pow2 <= TABLE_BUCKET_SIZE)
		table->used_count_shrink_threshold = 0;

	table->buckets = buckets;
}

#pragma once
#include "KrCommon.h"

#include <string.h>

template <typename T>
struct Array {
	ptrdiff_t          count;
	T *                data;

	ptrdiff_t          allocated;
	Memory_Allocator   allocator;

	inline Array() : count(0), data(nullptr), allocated(0), allocator(ThreadContext.allocator) {}
	inline Array(Memory_Allocator _allocator) : count(0), data(0), allocated(0), allocator(_allocator) {}
	inline operator Array_View<T>() { return Array_View<T>(data, count); }
	inline operator const Array_View<T>() const { return Array_View<T>(data, count); }
	inline T &operator[](ptrdiff_t i) { Assert(i >= 0 && i < count); return data[i]; }
	inline const T &operator[](ptrdiff_t i) const { Assert(i >= 0 && i < count); return data[i]; }
	inline T *begin() { return data; }
	inline T *end() { return data + count; }
	inline const T *begin() const { return data; }
	inline const T *end() const { return data + count; }
	T &First() { Assert(count); return data[0]; }
	const T &First() const { Assert(count); return data[0]; }
	T &Last() { Assert(count); return data[count - 1]; }
	const T &Last() const { Assert(count); return data[count - 1]; }

	inline ptrdiff_t GetGrowCapacity(ptrdiff_t size) const {
		ptrdiff_t new_capacity = allocated ? (allocated + allocated / 2) : 8;
		return new_capacity > size ? new_capacity : size;
	}

	inline void Reserve(ptrdiff_t new_capacity) {
		if (new_capacity <= allocated)
			return;
		T *new_data = (T *)MemoryReallocate(allocated * sizeof(T), new_capacity * sizeof(T), data, allocator);
		if (new_data) {
			data = new_data;
			allocated = new_capacity;
		}
	}

	inline void Resize(ptrdiff_t new_count) {
		Reserve(new_count);
		count = new_count;
	}

	template <typename... Args> void Emplace(const Args &...args) {
		if (count == allocated) {
			ptrdiff_t n = GetGrowCapacity(allocated + 1);
			Reserve(n);
		}
		data[count] = T(args...);
		count += 1;
	}

	T *Add() {
		if (count == allocated) {
			ptrdiff_t c = GetGrowCapacity(allocated + 1);
			Reserve(c);
		}
		count += 1;
		return data + (count - 1);
	}

	T *AddN(uint32_t n) {
		if (count + n > allocated) {
			ptrdiff_t c = GetGrowCapacity(count + n);
			Reserve(c);
		}
		T *ptr = data + count;
		count += n;
		return ptr;
	}

	void Add(const T &d) {
		T *m = Add();
		*m = d;
	}

	void Copy(Array_View<T> src) {
		if (src.count + count >= allocated) {
			ptrdiff_t c = GetGrowCapacity(src.count + count + 1);
			Reserve(c);
		}
		memcpy(data + count, src.data, src.count * sizeof(T));
		count += src.count;
	}

	void RemoveLast() {
		Assert(count > 0);
		count -= 1;
	}

	void Remove(ptrdiff_t index) {
		Assert(index < count);
		memmove(data + index, data + index + 1, (count - index - 1) * sizeof(T));
		count -= 1;
	}

	void RemoveUnordered(ptrdiff_t index) {
		Assert(index < count);
		data[index] = data[count - 1];
		count -= 1;
	}

	void Insert(ptrdiff_t index, const T &v) {
		Assert(index < count + 1);
		Add();
		for (ptrdiff_t move_index = count - 1; move_index > index; --move_index) {
			data[move_index] = data[move_index - 1];
		}
		data[index] = v;
	}

	void InsertUnordered(ptrdiff_t index, const T &v) {
		Assert(index < count + 1);
		Add();
		data[count - 1] = data[index];
		data[index] = v;
	}

	void Pack() {
		if (count != allocated) {
			data = (T *)MemoryReallocate(allocated * sizeof(T), count * sizeof(T), data, allocator);
			allocated = count;
		}
	}

	void Reset() {
		count = 0;
	}
};

template <typename T>
inline void Free(Array<T> *a) {
	if (a->data)
		MemoryFree(a->data, sizeof(T) * a->allocated, a->allocator);
}

template <typename T>
inline ptrdiff_t Find(Array_View<T> arr, const T &v) {
	for (ptrdiff_t index = 0; index < arr.count; ++index) {
		auto elem = arr.data + index;
		if (*elem == v) {
			return index;
		}
	}
	return -1;
}

template <typename T, typename SearchFunc, typename... Args>
inline ptrdiff_t Find(Array_View<T> arr, SearchFunc func, const Args &...args) {
	for (ptrdiff_t index = 0; index < arr.count; ++index) {
		if (func(arr[index], args...)) {
			return index;
		}
	}
	return -1;
}

//
//
//

uint32_t Murmur3Hash32(const uint8_t *key, size_t len, uint32_t seed);

//
//
//

template <typename K, typename V>
struct Key_Value {
	K key;
	V value;
};

constexpr size_t TABLE_BUCKET_SIZE = sizeof(size_t);
constexpr size_t TABLE_BUCKET_SHIFT = (TABLE_BUCKET_SIZE == 8 ? 3 : 2);
constexpr size_t TABLE_BUCKET_MASK = TABLE_BUCKET_SIZE - 1;

static_assert(TABLE_BUCKET_SIZE == 8 || TABLE_BUCKET_SIZE == 4);


enum Index_Bucket_Flag : int8_t {
	INDEX_BUCKET_EMPTY,
	INDEX_BUCKET_DELETED,
	INDEX_BUCKET_PRESENT
};

struct Index_Bucket {
	Index_Bucket_Flag flags[TABLE_BUCKET_SIZE] = {};
	size_t            hash[TABLE_BUCKET_SIZE]  = {};
	ptrdiff_t         index[TABLE_BUCKET_SIZE] = {};
};

struct Index_Table {
	size_t slot_count_pow2 = 0;
	size_t used_count = 0;
	size_t used_count_threshold = 0;
	size_t used_count_shrink_threshold = 0;
	size_t tombstone_count = 0;
	size_t tombstone_count_threshold = 0;

	Index_Bucket *buckets = nullptr;
};

void IndexTableFree(Index_Table *table, Memory_Allocator allocator);
void IndexTableAllocate(Index_Table *table, size_t slot_count_pow2, Memory_Allocator allocator);

template <typename K, typename V, typename Hash_Method>
ptrdiff_t IndexTableAdd(Index_Table *index, const Hash_Method &hash_method, K key, Array_View<Key_Value<K, V>> storage) {
	auto step = TABLE_BUCKET_SIZE;

	auto hash = hash_method(key);

	auto pos = hash & (index->slot_count_pow2 - 1);

	ptrdiff_t tombstone = -1;

	while (1) {
		auto bucket_index = pos >> TABLE_BUCKET_SHIFT;
		auto bucket = &index->buckets[bucket_index];

		for (auto iter = pos & TABLE_BUCKET_MASK; iter < TABLE_BUCKET_SIZE; ++iter) {
			if (bucket->flags[iter] == INDEX_BUCKET_PRESENT) {
				if (bucket->hash[iter] == hash) {
					auto si = bucket->index[iter];
					if (storage[si].key == key) {
						return si;
					}
				}
			} else if (bucket->flags[iter] == INDEX_BUCKET_EMPTY) {
				pos = (pos & ~TABLE_BUCKET_MASK) + iter;
				goto EmptyFound;
			} else if (tombstone < 0) {
				tombstone = (pos & ~TABLE_BUCKET_MASK) + iter;
			}
		}

		auto limit = pos & TABLE_BUCKET_MASK;
		for (auto iter = 0; iter < limit; ++iter) {
			if (bucket->flags[iter] == INDEX_BUCKET_PRESENT) {
				if (bucket->hash[iter] == hash) {
					auto si = bucket->index[iter];
					if (storage[si].key == key) {
						return si;
					}
				}
			} else if (bucket->flags[iter] == INDEX_BUCKET_EMPTY) {
				pos = (pos & ~TABLE_BUCKET_MASK) + iter;
				goto EmptyFound;
			} else if (tombstone < 0) {
				tombstone = (pos & ~TABLE_BUCKET_MASK) + iter;
			}
		}

		pos += step;
		pos &= (index->slot_count_pow2 - 1);
		step += TABLE_BUCKET_SIZE;
	}

EmptyFound:
	if (tombstone >= 0) {
		pos = tombstone;
		index->tombstone_count -= 1;
	}

	index->used_count += 1;

	auto bucket = &index->buckets[pos >> TABLE_BUCKET_SHIFT];

	auto si = (ptrdiff_t)storage.count;

	bucket->flags[pos & TABLE_BUCKET_MASK] = INDEX_BUCKET_PRESENT;
	bucket->hash[pos & TABLE_BUCKET_MASK] = hash;
	bucket->index[pos & TABLE_BUCKET_MASK] = si;

	return (ptrdiff_t)storage.count;
}


template <typename K, typename V, typename Hash_Method>
ptrdiff_t IndexTableFind(Index_Table *index, Hash_Method &hash_method, const K key, Array_View<Key_Value<K, V>> storage) {
	if (index->used_count == 0) return -1;

	auto hash = hash_method(key);

	auto step = TABLE_BUCKET_SIZE;

	auto pos = hash & (index->slot_count_pow2 - 1);

	while (1) {
		auto bucket_index = pos >> TABLE_BUCKET_SHIFT;
		auto bucket = &index->buckets[bucket_index];

		for (auto iter = pos & TABLE_BUCKET_MASK; iter < TABLE_BUCKET_SIZE; ++iter) {
			if (bucket->flags[iter] == INDEX_BUCKET_PRESENT) {
				if (bucket->hash[iter] == hash) {
					auto si = bucket->index[iter];
					if (storage[si].key == key) {
						return (pos & ~TABLE_BUCKET_MASK) + iter;
					}
				}
			} else if (bucket->flags[iter] == INDEX_BUCKET_EMPTY) {
				return -1;
			}
		}

		auto limit = pos & TABLE_BUCKET_MASK;
		for (auto iter = 0; iter < limit; ++iter) {
			if (bucket->flags[iter] == INDEX_BUCKET_PRESENT) {
				if (bucket->hash[iter] == hash) {
					auto si = bucket->index[iter];
					if (storage[si].key == key) {
						return (pos & ~TABLE_BUCKET_MASK) + iter;
					}
				}
			} else if (bucket->flags[iter] == INDEX_BUCKET_EMPTY) {
				return -1;
			}
		}

		pos += step;
		pos &= (index->slot_count_pow2 - 1);
		step += TABLE_BUCKET_SIZE;
	}

	Unreachable();
	return -1;
}

template <typename K, typename V, typename Hash_Method>
bool IndexTableRemove(Index_Table *index, Hash_Method &hash_method, const K key, Array_View<Key_Value<K, V>> storage) {
	if (!index->buckets) return false;

	auto pos = IndexTableFind<K, V>(index, hash_method, key, storage);
	if (pos < 0) return false;

	Assert(pos < (ptrdiff_t)index->slot_count_pow2);

	auto bucket = &index->buckets[pos >> TABLE_BUCKET_SHIFT];
	auto iter = pos & TABLE_BUCKET_MASK;

	ptrdiff_t old_offset = bucket->index[iter];
	ptrdiff_t last_offset = (ptrdiff_t)storage.count - 1;

	bucket->flags[iter] = INDEX_BUCKET_DELETED;
	bucket->hash[iter] = 0;
	bucket->index[iter] = -1;

	index->used_count -= 1;
	index->tombstone_count += 1;

	if (old_offset != last_offset) {
		auto temp = storage[old_offset];
		storage[old_offset] = storage[last_offset];

		auto ex_key = storage[old_offset].key;
		auto pos = IndexTableFind<K, V>(index, hash_method, ex_key, storage);
		Assert(pos >= 0);

		storage[last_offset] = temp;

		bucket = &index->buckets[pos >> TABLE_BUCKET_SHIFT];
		iter = pos & TABLE_BUCKET_MASK;
		Assert(bucket->index[iter] == last_offset);
		bucket->index[iter] = old_offset;
	}

	return true;
}

//
//
//

template <typename T> struct Table_Hash_Method { 
	size_t operator()(const T v) const { 
		return Murmur3Hash32(&v, sizeof(v), 0x31415926); 
	} 
};
template <> struct Table_Hash_Method<String> { 
	size_t operator()(const String v) const { 
		return Murmur3Hash32(v.data, v.length, 0x31415926); 
	} 
};
template <> struct Table_Hash_Method<ptrdiff_t> { 
	size_t operator()(const ptrdiff_t v) const { 
		return (uint32_t)v; 
	} 
};
template <> 
struct Table_Hash_Method<uint64_t> { 
	size_t operator()(const uint64_t v) const { 
		return (uint32_t)v; 
	} 
};

template <typename K, typename V, typename Hash_Method = Table_Hash_Method<K>>
struct Table {
	using Pair = Key_Value<K, V>;

	Index_Table index;
	Array<Pair> storage;

	Hash_Method hash_method;

	Table() = default;
	Table(Memory_Allocator allocator) : storage(allocator) {}

	inline Pair *begin() { return storage.begin(); }
	inline Pair *end() { return storage.end(); }
	inline const Pair *begin() const { return storage.begin(); }
	inline const Pair *end() const { return storage.end(); }

	inline ptrdiff_t ElementCount() const { return storage.count; }

	V *Find(const K key) {
		auto pos = IndexTableFind<K, V>(&index, hash_method, key, storage);
		if (pos >= 0) {
			auto bucket = &index.buckets[pos >> TABLE_BUCKET_SHIFT];
			auto si = bucket->index[pos & TABLE_BUCKET_MASK];
			return &storage[si].value;
		}
		return nullptr;
	}

	V *FindOrPut(const K key) {
		if (index.used_count >= index.used_count_threshold) {
			auto new_slot_count_pow2 = index.slot_count_pow2 ? index.slot_count_pow2 * 2 : TABLE_BUCKET_SIZE;
			IndexTableAllocate(&index, new_slot_count_pow2, storage.allocator);
		}

		auto result = IndexTableAdd<K, V>(&index, hash_method, key, storage);

		if (result < storage.count) {
			return &storage[result].value;
		}

		auto pair = storage.Add();
		pair->key = key;
		pair->value = V{};

		return &pair->value;
	}

	void Put(const K key, const V &value) {
		auto dst = FindOrPut(key);
		*dst = value;
	}

	void Remove(const K key) {
		if (IndexTableRemove<K, V, Hash_Method>(&index, hash_method, key, storage)) {
			storage.count -= 1;
			if (index.used_count < index.used_count_shrink_threshold && index.slot_count_pow2 > TABLE_BUCKET_SIZE)
				IndexTableAllocate(&index, index.slot_count_pow2 >> 2, storage.allocator);
			else if (index.tombstone_count > index.tombstone_count_threshold)
				IndexTableAllocate(&index, index.slot_count_pow2, storage.allocator);
		}
	}
};

template <typename K, typename V, typename Hash_Method = Table_Hash_Method<String>>
inline void Free(Table<K, V, Hash_Method> *table) {
	IndexTableFree(&table->index, table->storage.allocator);
	Free(&table->storage);
}

template <typename V, typename Hash_Method = Table_Hash_Method<String>>
struct STable {
	using K = String;
	using Pair = Key_Value<K, V>;

	Index_Table index;
	Array<Pair> storage;

	Hash_Method hash_method;

	Memory_Allocator string_allocator = ThreadContext.allocator;

	STable() = default;
	STable(Memory_Allocator allocator) : storage(allocator), string_allocator(allocator) {}
	STable(Memory_Allocator allocator, Memory_Allocator str_allocator) : storage(allocator), string_allocator(str_allocator) {}

	inline Pair *begin() { return storage.begin(); }
	inline Pair *end() { return storage.end(); }
	inline const Pair *begin() const { return storage.begin(); }
	inline const Pair *end() const { return storage.end(); }

	inline ptrdiff_t ElementCount() const { return storage.count; }

	V *Find(const K key) {
		auto pos = IndexTableFind<K, V>(&index, hash_method, key, storage);
		if (pos >= 0) {
			auto bucket = &index.buckets[pos >> TABLE_BUCKET_SHIFT];
			auto si = bucket->index[pos & TABLE_BUCKET_MASK];
			return &storage[si].value;
		}
		return nullptr;
	}

	V *FindOrPut(const K key) {
		if (index.used_count >= index.used_count_threshold) {
			auto new_slot_count_pow2 = index.slot_count_pow2 ? index.slot_count_pow2 * 2 : TABLE_BUCKET_SIZE;
			IndexTableAllocate(&index, new_slot_count_pow2, storage.allocator);
		}

		auto result = IndexTableAdd<K, V>(&index, hash_method, key, storage);

		if (result < storage.count) {
			return &storage[result].value;
		}

		String key_copy;
		key_copy.length = key.length;
		key_copy.data = (uint8_t *)MemoryAllocate(key.length + 1, string_allocator);
		memcpy(key_copy.data, key.data, key.length);
		key_copy.data[key.length] = 0;

		auto pair = storage.Add();
		pair->key = key_copy;
		pair->value = V{};

		return &pair->value;
	}

	void Put(const K key, const V &value) {
		auto dst = FindOrPut(key);
		*dst = value;
	}

	void Remove(const K key) {
		if (IndexTableRemove<K, V, Hash_Method>(&index, hash_method, key, storage)) {
			auto last = storage.Last();
			MemoryFree(last.key.data, last.key.length + 1, string_allocator);
			storage.count -= 1;

			if (index.used_count < index.used_count_shrink_threshold && index.slot_count_pow2 > TABLE_BUCKET_SIZE)
				IndexTableAllocate(&index, index.slot_count_pow2 >> 2, storage.allocator);
			else if (index.tombstone_count > index.tombstone_count_threshold)
				IndexTableAllocate(&index, index.slot_count_pow2, storage.allocator);
		}
	}
};

template <typename V, typename Hash_Method = Table_Hash_Method<String>>
inline void Free(STable<V, Hash_Method> *table) {
	IndexTableFree(&table->index, table->storage.allocator);
	for (auto &pair : table->storage) {
		MemoryFree(pair.key.data, pair.key.length + 1, table->string_allocator);
	}
	Free(&table->storage);
}

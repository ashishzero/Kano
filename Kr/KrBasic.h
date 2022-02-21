#pragma once
#include "KrCommon.h"

#include <string.h>

#define ForEach(c) for (auto it = IterBegin(&(c)); IterEnd(&(c), it); IterNext(&it)) 
#define ForEachTag(name, c) for (auto name = IterBegin(&(c)); IterEnd(&(c), name); IterNext(&name)) 

template <typename T> 
struct Array_View {
	int64_t count;
	T *data;

	inline Array_View() : count(0), data(nullptr) {}
	inline Array_View(T *p, int64_t n) : count(n), data(p) {}
	template <int64_t _Count> constexpr Array_View(const T(&a)[_Count]) : count(_Count), data((T *)a) {}
	inline T &operator[](int64_t index) const { Assert(index < count); return data[index]; }
	inline T *begin() { return data; }
	inline T *end() { return data + count; }
	inline const T *begin() const { return data; }
	inline const T *end() const { return data + count; }
	T &First() { Assert(count); return data[0]; }
	const T &First() const { Assert(count); return data[0]; }
	T &Last() { Assert(count); return data[count - 1]; }
	const T &Last() const { Assert(count); return data[count - 1]; }
};

template <typename T>
struct Array {
	int64_t          count;
	T *data;

	int64_t          allocated;
	Memory_Allocator allocator;

	inline Array() : count(0), data(nullptr), allocated(0), allocator(ThreadContext.allocator) {}
	inline Array(Memory_Allocator _allocator) : count(0), data(0), allocated(0), allocator(_allocator) {}
	inline operator Array_View<T>() { return Array_View<T>(data, count); }
	inline operator const Array_View<T>() const { return Array_View<T>(data, count); }
	inline T &operator[](int64_t i) { Assert(i >= 0 && i < count); return data[i]; }
	inline const T &operator[](int64_t i) const { Assert(i >= 0 && i < count); return data[i]; }
	inline T *begin() { return data; }
	inline T *end() { return data + count; }
	inline const T *begin() const { return data; }
	inline const T *end() const { return data + count; }
	T &First() { Assert(count); return data[0]; }
	const T &First() const { Assert(count); return data[0]; }
	T &Last() { Assert(count); return data[count - 1]; }
	const T &Last() const { Assert(count); return data[count - 1]; }

	inline int64_t GetGrowCapacity(int64_t size) const {
		int64_t new_capacity = allocated ? (allocated + allocated / 2) : 8;
		return new_capacity > size ? new_capacity : size;
	}

	inline void Reserve(int64_t new_capacity) {
		if (new_capacity <= allocated)
			return;
		T *new_data = (T *)MemoryReallocate(allocated * sizeof(T), new_capacity * sizeof(T), data, allocator);
		if (new_data) {
			data = new_data;
			allocated = new_capacity;
		}
	}

	inline void Resize(int64_t new_count) {
		Reserve(new_count);
		count = new_count;
	}

	template <typename... Args> void Emplace(const Args &...args) {
		if (count == allocated) {
			int64_t n = GetGrowCapacity(allocated + 1);
			Reserve(n);
		}
		data[count] = T(args...);
		count += 1;
	}

	T *Add() {
		if (count == allocated) {
			int64_t c = GetGrowCapacity(allocated + 1);
			Reserve(c);
		}
		count += 1;
		return data + (count - 1);
	}

	T *AddN(uint32_t n) {
		if (count + n > allocated) {
			int64_t c = GetGrowCapacity(count + n);
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
			int64_t c = GetGrowCapacity(src.count + count + 1);
			Reserve(c);
		}
		memcpy(data + count, src.data, src.count * sizeof(T));
		count += src.count;
	}

	void RemoveLast() {
		Assert(count > 0);
		count -= 1;
	}

	void Remove(int64_t index) {
		Assert(index < count);
		memmove(data + index, data + index + 1, (count - index - 1) * sizeof(T));
		count -= 1;
	}

	void RemoveUnordered(int64_t index) {
		Assert(index < count);
		data[index] = data[count - 1];
		count -= 1;
	}

	void Insert(int64_t index, const T &v) {
		Assert(index < count + 1);
		Add();
		for (int64_t move_index = count - 1; move_index > index; --move_index) {
			data[move_index] = data[move_index - 1];
		}
		data[index] = v;
	}

	void InsertUnordered(int64_t index, const T &v) {
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
		MemoryFree(a->data, a->allocator);
}

template <typename T>
inline void Free(Array_View<T> *a) {
	if (a->data)
		MemoryFree(a->data);
}

template <typename T>
inline int64_t Find(Array_View<T> arr, const T &v) {
	for (int64_t index = 0; index < arr.count; ++index) {
		auto elem = arr.data + index;
		if (*elem == v) {
			return index;
		}
	}
	return -1;
}

template <typename T, typename SearchFunc, typename... Args>
inline int64_t Find(Array_View<T> arr, SearchFunc func, const Args &...args) {
	for (int64_t index = 0; index < arr.count; ++index) {
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

constexpr size_t TABLE_BUCKET_SIZE = sizeof(size_t);
constexpr size_t TABLE_BUCKET_SHIFT = (TABLE_BUCKET_SIZE == 8 ? 3 : 2);
constexpr size_t TABLE_BUCKET_MASK = TABLE_BUCKET_SIZE - 1;

static_assert(TABLE_BUCKET_SIZE == 8 || TABLE_BUCKET_SIZE == 4);

struct Index_Bucket {
	int8_t flags[TABLE_BUCKET_SIZE] = {};
	size_t hash[TABLE_BUCKET_SIZE] = {};
	ptrdiff_t index[TABLE_BUCKET_SIZE] = {};
};

constexpr size_t TABLE_BUCKET_FLAG_EMPTY = 0;
constexpr size_t TABLE_BUCKET_FLAG_DELETED = 1;
constexpr size_t TABLE_BUCKET_FLAG_PRESENT = 2;

struct Index_Table {
	size_t slot_count_pow2 = 0;
	size_t used_count = 0;
	size_t used_count_threshold = 0;
	size_t used_count_shrink_threshold = 0;
	size_t tombstone_count = 0;
	size_t tombstone_count_threshold = 0;

	Index_Bucket *buckets = nullptr;
};

void IndexTableResize(Index_Table *table, size_t slot_count_pow2, Memory_Allocator allocator);

//
// https://github.com/nothings/stb/blob/master/stb_ds.h
//

template <typename T> 
struct Table_Hash_Method { static uint32_t Hash(const T v) { return Murmur3Hash32(&v, sizeof(v), 0x31415926); } };
template <> 
struct Table_Hash_Method<String> { static uint32_t Hash(const String v) { return Murmur3Hash32(v.data, v.length, 0x31415926); } };
template <> 
struct Table_Hash_Method<int64_t> { static uint32_t Hash(const int64_t v) { return (uint32_t)v; } };
template <> 
struct Table_Hash_Method<uint64_t> { static uint32_t Hash(const uint64_t v) { return (uint32_t)v; } };

template <typename Key_Type, typename Value_Type, typename Hash_Method = Table_Hash_Method<String>>
struct Table {
	struct Pair {
		Key_Type key;
		Value_Type value;
	};

	Index_Table index;
	Array<Pair> storage;

	Table() = default;
	Table(Memory_Allocator allocator) : storage(allocator) {}

	inline Pair *begin() { return storage.begin(); }
	inline Pair *end() { return storage.end(); }
	inline const Pair *begin() const { return storage.begin(); }
	inline const Pair *end() const { return storage.end(); }

	inline int64_t ElementCount() const { return storage.count; }

	inline size_t GetKeyHash(const Key_Type key) const {
		auto hash = Hash_Method::Hash(key);
		return hash;
	}

	ptrdiff_t IndexTableFindSlotPosition(const Key_Type key) {
		if (index.used_count == 0) return -1;

		auto hash = GetKeyHash(key);

		auto step = TABLE_BUCKET_SIZE;

		auto pos = hash & (index.slot_count_pow2 - 1);

		while (1) {
			auto bucket_index = pos >> TABLE_BUCKET_SHIFT;
			auto bucket = &index.buckets[bucket_index];

			for (auto iter = pos & TABLE_BUCKET_MASK; iter < TABLE_BUCKET_SIZE; ++iter) {
				if (bucket->flags[iter] == TABLE_BUCKET_FLAG_PRESENT) {
					if (bucket->hash[iter] == hash) {
						auto si = bucket->index[iter];
						if (storage[si].key == key) {
							return (pos & ~TABLE_BUCKET_MASK) + iter;
						}
					}
				} else if (bucket->flags[iter] == TABLE_BUCKET_FLAG_EMPTY) {
					return -1;
				}
			}

			auto limit = pos & TABLE_BUCKET_MASK;
			for (auto iter = 0; iter < limit; ++iter) {
				if (bucket->flags[iter] == TABLE_BUCKET_FLAG_PRESENT) {
					if (bucket->hash[iter] == hash) {
						auto si = bucket->index[iter];
						if (storage[si].key == key) {
							return (pos & ~TABLE_BUCKET_MASK) + iter;
						}
					}
				} else if (bucket->flags[iter] == TABLE_BUCKET_FLAG_EMPTY) {
					return -1;
				}
			}

			pos += step;
			pos &= (index.slot_count_pow2 - 1);
			step += TABLE_BUCKET_SIZE;
		}

		Unreachable();
		return -1;
	}

	Value_Type *Find(const Key_Type key) {
		auto pos = IndexTableFindSlotPosition(key);
		if (pos >= 0) {
			auto bucket = &index.buckets[pos >> TABLE_BUCKET_SHIFT];
			auto si = bucket->index[pos & TABLE_BUCKET_MASK];
			return &storage[si].value;
		}
		return nullptr;
	}

	Value_Type *FindOrPut(const Key_Type key) {
		if (index.used_count >= index.used_count_threshold) {
			auto new_slot_count_pow2 = index.slot_count_pow2 ? index.slot_count_pow2 * 2 : TABLE_BUCKET_SIZE;
			IndexTableResize(&index, new_slot_count_pow2, storage.allocator);
		}

		auto hash = GetKeyHash(key);

		auto step = TABLE_BUCKET_SIZE;

		auto pos = hash & (index.slot_count_pow2 - 1);

		ptrdiff_t tombstone = -1;

		while (1) {
			auto bucket_index = pos >> TABLE_BUCKET_SHIFT;
			auto bucket = &index.buckets[bucket_index];

			for (auto iter = pos & TABLE_BUCKET_MASK; iter < TABLE_BUCKET_SIZE; ++iter) {
				if (bucket->flags[iter] == TABLE_BUCKET_FLAG_PRESENT) {
					if (bucket->hash[iter] == hash) {
						auto si = bucket->index[iter];
						if (storage[si].key == key) {
							return &storage[si].value;
						}
					}
				} else if (bucket->flags[iter] == TABLE_BUCKET_FLAG_EMPTY) {
					pos = (pos & ~TABLE_BUCKET_MASK) + iter;
					goto EmptyFound;
				} else if (tombstone < 0) {
					tombstone = (pos & ~TABLE_BUCKET_MASK) + iter;
				}
			}

			auto limit = pos & TABLE_BUCKET_MASK;
			for (auto iter = 0; iter < limit; ++iter) {
				if (bucket->flags[iter] == TABLE_BUCKET_FLAG_PRESENT) {
					if (bucket->hash[iter] == hash) {
						auto si = bucket->index[iter];
						if (storage[si].key == key) {
							return &storage[si].value;
						}
					}
				} else if (bucket->flags[iter] == TABLE_BUCKET_FLAG_EMPTY) {
					pos = (pos & ~TABLE_BUCKET_MASK) + iter;
					goto EmptyFound;
				} else if (tombstone < 0) {
					tombstone = (pos & ~TABLE_BUCKET_MASK) + iter;
				}
			}

			pos += step;
			pos &= (index.slot_count_pow2 - 1);
			step += TABLE_BUCKET_SIZE;
		}

	EmptyFound:
		if (tombstone >= 0) {
			pos = tombstone;
			index.tombstone_count -= 1;
		}

		index.used_count += 1;

		auto bucket = &index.buckets[pos >> TABLE_BUCKET_SHIFT];

		bucket->flags[pos & TABLE_BUCKET_MASK] = TABLE_BUCKET_FLAG_PRESENT;
		bucket->hash[pos & TABLE_BUCKET_MASK] = hash;
		bucket->index[pos & TABLE_BUCKET_MASK] = (ptrdiff_t)storage.count;

		auto pair = storage.Add();
		pair->key = key;
		pair->value = Value_Type{};
		return &pair->value;
	}

	void Put(const Key_Type key, const Value_Type &value) {
		auto dst = FindOrPut(key);
		*dst = value;
	}

	void Remove(const Key_Type key) {
		if (!index.buckets) return;

		auto pos = IndexTableFindSlotPosition(key);
		if (pos < 0) return;

		Assert(pos < (ptrdiff_t)index.slot_count_pow2);

		auto bucket = &index.buckets[pos >> TABLE_BUCKET_SHIFT];
		auto iter = pos & TABLE_BUCKET_MASK;

		ptrdiff_t old_offset = bucket->index[iter];
		ptrdiff_t last_offset = (ptrdiff_t)storage.count - 1;

		bucket->flags[iter] = TABLE_BUCKET_FLAG_DELETED;
		bucket->hash[iter] = 0;
		bucket->index[iter] = -1;

		index.used_count -= 1;
		index.tombstone_count += 1;

		if (old_offset != last_offset) {
			storage[old_offset] = storage[last_offset];

			pos = IndexTableFindSlotPosition(storage[old_offset].key);
			Assert(pos >= 0);

			bucket = &index.buckets[pos >> TABLE_BUCKET_SHIFT];
			iter = pos & TABLE_BUCKET_MASK;
			Assert(bucket->index[iter] == last_offset);
			bucket->index[iter] = old_offset;
		}

		storage.count -= 1;

		if (index.used_count < index.used_count_shrink_threshold && index.slot_count_pow2 > TABLE_BUCKET_SIZE)
			IndexTableResize(&index, index.slot_count_pow2 >> 2, storage.allocator);
		else if (index.tombstone_count > index.tombstone_count_threshold)
			IndexTableResize(&index, index.slot_count_pow2, storage.allocator);
	}
};

template <typename Key_Type, typename Value_Type, typename Hash_Method = Table_Hash_Method<String>>
inline void Free(Table<Key_Type, Value_Type, Hash_Method> *table) {
	MemoryFree(table->index.buckets, table->storage.allocator);
	Free(&table->storage);
}

template <typename Value_Type, typename Hash_Method = Table_Hash_Method<String>>
struct STable {
	using Key_Type = String;
	struct Pair {
		Key_Type key;
		Value_Type value;
	};

	Index_Table index;
	Array<Pair> storage;
	Memory_Allocator string_allocator = ThreadContext.allocator;

	STable() = default;
	STable(Memory_Allocator allocator) : storage(allocator), string_allocator(allocator) {}
	STable(Memory_Allocator allocator, Memory_Allocator str_allocator) : storage(allocator), string_allocator(str_allocator) {}

	inline Pair *begin() { return storage.begin(); }
	inline Pair *end() { return storage.end(); }
	inline const Pair *begin() const { return storage.begin(); }
	inline const Pair *end() const { return storage.end(); }

	inline int64_t ElementCount() const { return storage.count; }

	inline size_t GetKeyHash(const Key_Type key) const {
		auto hash = Hash_Method::Hash(key);
		return hash;
	}

	ptrdiff_t IndexTableFindSlotPosition(const Key_Type key) {
		if (index.used_count == 0) return -1;

		auto hash = GetKeyHash(key);

		auto step = TABLE_BUCKET_SIZE;

		auto pos = hash & (index.slot_count_pow2 - 1);

		while (1) {
			auto bucket_index = pos >> TABLE_BUCKET_SHIFT;
			auto bucket = &index.buckets[bucket_index];

			for (auto iter = pos & TABLE_BUCKET_MASK; iter < TABLE_BUCKET_SIZE; ++iter) {
				if (bucket->flags[iter] == TABLE_BUCKET_FLAG_PRESENT) {
					if (bucket->hash[iter] == hash) {
						auto si = bucket->index[iter];
						if (storage[si].key == key) {
							return (pos & ~TABLE_BUCKET_MASK) + iter;
						}
					}
				}
				else if (bucket->flags[iter] == TABLE_BUCKET_FLAG_EMPTY) {
					return -1;
				}
			}

			auto limit = pos & TABLE_BUCKET_MASK;
			for (auto iter = 0; iter < limit; ++iter) {
				if (bucket->flags[iter] == TABLE_BUCKET_FLAG_PRESENT) {
					if (bucket->hash[iter] == hash) {
						auto si = bucket->index[iter];
						if (storage[si].key == key) {
							return (pos & ~TABLE_BUCKET_MASK) + iter;
						}
					}
				}
				else if (bucket->flags[iter] == TABLE_BUCKET_FLAG_EMPTY) {
					return -1;
				}
			}

			pos += step;
			pos &= (index.slot_count_pow2 - 1);
			step += TABLE_BUCKET_SIZE;
		}

		Unreachable();
		return -1;
	}

	Value_Type *Find(const Key_Type key) {
		auto pos = IndexTableFindSlotPosition(key);
		if (pos >= 0) {
			auto bucket = &index.buckets[pos >> TABLE_BUCKET_SHIFT];
			auto si = bucket->index[pos & TABLE_BUCKET_MASK];
			return &storage[si].value;
		}
		return nullptr;
	}

	Value_Type *FindOrPut(const Key_Type key) {
		if (index.used_count >= index.used_count_threshold) {
			auto new_slot_count_pow2 = index.slot_count_pow2 ? index.slot_count_pow2 * 2 : TABLE_BUCKET_SIZE;
			IndexTableResize(&index, new_slot_count_pow2, storage.allocator);
		}

		auto hash = GetKeyHash(key);

		auto step = TABLE_BUCKET_SIZE;

		auto pos = hash & (index.slot_count_pow2 - 1);

		ptrdiff_t tombstone = -1;

		while (1) {
			auto bucket_index = pos >> TABLE_BUCKET_SHIFT;
			auto bucket = &index.buckets[bucket_index];

			for (auto iter = pos & TABLE_BUCKET_MASK; iter < TABLE_BUCKET_SIZE; ++iter) {
				if (bucket->flags[iter] == TABLE_BUCKET_FLAG_PRESENT) {
					if (bucket->hash[iter] == hash) {
						auto si = bucket->index[iter];
						if (storage[si].key == key) {
							return &storage[si].value;
						}
					}
				}
				else if (bucket->flags[iter] == TABLE_BUCKET_FLAG_EMPTY) {
					pos = (pos & ~TABLE_BUCKET_MASK) + iter;
					goto EmptyFound;
				}
				else if (tombstone < 0) {
					tombstone = (pos & ~TABLE_BUCKET_MASK) + iter;
				}
			}

			auto limit = pos & TABLE_BUCKET_MASK;
			for (auto iter = 0; iter < limit; ++iter) {
				if (bucket->flags[iter] == TABLE_BUCKET_FLAG_PRESENT) {
					if (bucket->hash[iter] == hash) {
						auto si = bucket->index[iter];
						if (storage[si].key == key) {
							return &storage[si].value;
						}
					}
				}
				else if (bucket->flags[iter] == TABLE_BUCKET_FLAG_EMPTY) {
					pos = (pos & ~TABLE_BUCKET_MASK) + iter;
					goto EmptyFound;
				}
				else if (tombstone < 0) {
					tombstone = (pos & ~TABLE_BUCKET_MASK) + iter;
				}
			}

			pos += step;
			pos &= (index.slot_count_pow2 - 1);
			step += TABLE_BUCKET_SIZE;
		}

	EmptyFound:
		if (tombstone >= 0) {
			pos = tombstone;
			index.tombstone_count -= 1;
		}

		index.used_count += 1;

		auto bucket = &index.buckets[pos >> TABLE_BUCKET_SHIFT];

		bucket->flags[pos & TABLE_BUCKET_MASK] = TABLE_BUCKET_FLAG_PRESENT;
		bucket->hash[pos & TABLE_BUCKET_MASK] = hash;
		bucket->index[pos & TABLE_BUCKET_MASK] = (ptrdiff_t)storage.count;

		String key_copy;
		key_copy.length = key.length;
		key_copy.data = (uint8_t *)MemoryAllocate(key.length + 1, string_allocator);
		memcpy(key_copy.data, key.data, key.length);
		key_copy.data[key.length] = 0;

		auto pair = storage.Add();
		pair->key = key_copy;
		pair->value = Value_Type{};
		return &pair->value;
	}

	void Put(const Key_Type key, const Value_Type &value) {
		auto dst = FindOrPut(key);
		*dst = value;
	}

	void Remove(const Key_Type key) {
		if (!index.buckets) return;

		auto pos = IndexTableFindSlotPosition(key);
		if (pos < 0) return;

		Assert(pos < (ptrdiff_t)index.slot_count_pow2);

		auto bucket = &index.buckets[pos >> TABLE_BUCKET_SHIFT];
		auto iter = pos & TABLE_BUCKET_MASK;

		ptrdiff_t old_offset = bucket->index[iter];
		ptrdiff_t last_offset = (ptrdiff_t)storage.count - 1;

		bucket->flags[iter] = TABLE_BUCKET_FLAG_DELETED;
		bucket->hash[iter] = 0;
		bucket->index[iter] = -1;

		index.used_count -= 1;
		index.tombstone_count += 1;

		MemoryFree(storage[old_offset].key.data, string_allocator);

		if (old_offset != last_offset) {
			storage[old_offset] = storage[last_offset];

			pos = IndexTableFindSlotPosition(storage[old_offset].key);
			Assert(pos >= 0);

			bucket = &index.buckets[pos >> TABLE_BUCKET_SHIFT];
			iter = pos & TABLE_BUCKET_MASK;
			Assert(bucket->index[iter] == last_offset);
			bucket->index[iter] = old_offset;
		}

		storage.count -= 1;

		if (index.used_count < index.used_count_shrink_threshold && index.slot_count_pow2 > TABLE_BUCKET_SIZE)
			IndexTableResize(&index, index.slot_count_pow2 >> 2, storage.allocator);
		else if (index.tombstone_count > index.tombstone_count_threshold)
			IndexTableResize(&index, index.slot_count_pow2, storage.allocator);
	}
};

template <typename Value_Type, typename Hash_Method = Table_Hash_Method<String>>
inline void Free(STable<Value_Type, Hash_Method> *table) {
	MemoryFree(table->index.buckets, table->storage.allocator);
	for (auto &pair : table->storage) {
		MemoryFree(pair.key.data, table->string_allocator);
	}
	Free(&table->storage);
}

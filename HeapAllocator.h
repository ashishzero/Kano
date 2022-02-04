#include "Common.h"

struct Heap_Allocator
{
	struct Memory
	{
		void *ptr;
		uint64_t size;
	};

	struct Bucket
	{
		uint64_t size;

		union {
			Bucket * next[1];
			uint8_t  ptr[8];
		};
	};

	Bucket *free_list = nullptr;

	uint64_t allocation = 0;
	
	Array<Memory> memories;
};

static inline bool heap_contains_memory(Heap_Allocator *allocator, void *ptr)
{
	for (auto memory : allocator->memories)
	{
		if (ptr >= memory.ptr && ptr < (uint8_t *)memory.ptr + memory.size)
		{
			return true;
		}
	}

	return false;
}

static inline void heap_free(Heap_Allocator *allocator, void *ptr)
{
	Assert(heap_contains_memory(allocator, ptr));
	
	auto buk = (Heap_Allocator::Bucket *)((uint8_t *)ptr - sizeof(Heap_Allocator::Bucket::size));
	buk->next[0] = allocator->free_list;
	allocator->free_list = buk;
}

static inline void *heap_alloc(Heap_Allocator *allocator, uint64_t size)
{
	size = Maximum(size, sizeof(Heap_Allocator::Bucket::ptr));

	Heap_Allocator::Bucket *parent = nullptr;
	for (auto buk = allocator->free_list; buk; buk = buk->next[0])
	{
		if (size <= buk->size)
		{
			if (buk->size > size + sizeof(*buk))
			{
				auto next = (Heap_Allocator::Bucket *)(buk->ptr + size);
				next->size = buk->size - size;
				next->next[0] = buk->next[0];

				if (parent)
					parent->next[0] = next;
				else
					allocator->free_list = next;

				buk->size = size;
				memset(buk->ptr, 0, size);
				return (void *)buk->ptr;
			}
			else
			{
				if (parent)
					parent->next[0] = buk->next[0];
				else
					allocator->free_list = buk->next[0];
				memset(buk->ptr, 0, size);
				return (void *)buk->ptr;
			}
		}

		parent = buk;
	}

	allocator->allocation = Maximum(1024 * 1024, allocator->allocation * 2);
	allocator->allocation = Maximum(allocator->allocation, size);
	allocator->allocation = AlignPower2Up(allocator->allocation, 64);

	Heap_Allocator::Memory mem;
	mem.size = allocator->allocation;
	mem.ptr = malloc(mem.size);

	allocator->memories.add(mem);
	
	auto buk = (Heap_Allocator::Bucket *)mem.ptr;
	buk->size = mem.size - sizeof(buk->size);
	buk->next[0] = allocator->free_list;
	allocator->free_list = buk;

	return heap_alloc(allocator, size);
}

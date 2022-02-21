#include "KrCommon.h"

#include <string.h>

thread_local Thread_Context ThreadContext;
static Thread_Context_Params ThreadContextDefaultParams;

bool operator==(const String a, const String b) {
	if (a.length != b.length)
		return false;
	return memcmp(a.data, b.data, a.length) == 0;
}

bool operator!=(const String a, const String b) {
	if (a.length != b.length)
		return true;
	return memcmp(a.data, b.data, a.length) != 0;
}


uint8_t *AlignPointer(uint8_t *location, size_t alignment) {
	return (uint8_t *)((size_t)(location + (alignment - 1)) & ~(alignment - 1));
}

size_t AlignSize(size_t location, size_t alignment) {
	return ((location + (alignment - 1)) & ~(alignment - 1));
}

Memory_Arena *MemoryArenaCreate(size_t max_size) {
	max_size = AlignPower2Up(max_size, 64 * 1024);
	uint8_t *mem = (uint8_t *)VirtualMemoryAllocate(0, max_size);
	if (mem) {
		size_t commit_size = Minimum(MEMORY_ARENA_COMMIT_SIZE, max_size);
		if (VirtualMemoryCommit(mem, commit_size)) {
			Memory_Arena *arena = (Memory_Arena *)mem;
			arena->current = sizeof(Memory_Arena);
			arena->reserved = max_size;
			arena->committed = commit_size;
			return arena;
		}
	}
	return nullptr;
}

void MemoryArenaDestroy(Memory_Arena *arena) {
	VirtualMemoryFree(arena, arena->reserved);
}

void MemoryArenaReset(Memory_Arena *arena) {
	arena->current = 0;
}

size_t MemoryArenaSizeLeft(Memory_Arena *arena) {
	return arena->reserved - arena->current;
}

void *PushSize(Memory_Arena *arena, size_t size) {
	void *ptr = 0;
	uint8_t *mem = (uint8_t *)arena;
	if (arena->current + size <= arena->reserved) {
		ptr = mem + arena->current;
		arena->current += size;
		if (arena->current > arena->committed) {
			size_t committed = AlignPower2Up(arena->current, MEMORY_ARENA_COMMIT_SIZE);
			committed = Minimum(committed, arena->reserved);
			if (VirtualMemoryCommit(mem + arena->committed, committed - arena->committed)) {
				arena->committed = committed;
			} else {
				arena->current -= size;
				ptr = 0;
			}
		}
	}
	return ptr;
}

void *PushSizeAligned(Memory_Arena *arena, size_t size, uint32_t alignment) {
	uint8_t *mem = (uint8_t *)arena;
	uint8_t *aligned_current = AlignPointer(mem + arena->current, alignment);
	uint8_t *next_current = aligned_current + size;
	size_t alloc_size = next_current - (mem + arena->current);
	if (PushSize(arena, alloc_size))
		return aligned_current;
	return 0;
}

bool SetAllocationPosition(Memory_Arena *arena, size_t pos) {
	if (pos < arena->current) {
		arena->current = pos;
		size_t committed = AlignPower2Up(pos, MEMORY_ARENA_COMMIT_SIZE);
		committed = Minimum(committed, arena->reserved);

		if (committed < arena->committed) {
			VirtualMemoryDecommit(arena + committed, arena->committed - committed);
			arena->committed = committed;
		}
		return true;
	} else {
		Assert(pos >= sizeof(Memory_Arena));
		if (PushSize(arena, pos - arena->current))
			return true;
		return false;
	}
}

Temporary_Memory BeginTemporaryMemory(Memory_Arena *arena) {
	Temporary_Memory mem;
	mem.arena = arena;
	mem.position = arena->current;
	return mem;
}

void EndTemporaryMemory(Temporary_Memory *temp) {
	temp->arena->current = temp->position;
}

void FreeTemporaryMemory(Temporary_Memory *temp) {
	SetAllocationPosition(temp->arena, temp->position);
}

Memory_Arena *ThreadScratchpad() {
	return ThreadContext.scratchpad.arena[0];
}

Memory_Arena *ThreadScratchpadI(uint32_t i) {
	Assert(i < ArrayCount(ThreadContext.scratchpad.arena));
	return ThreadContext.scratchpad.arena[i];
}

Memory_Arena *ThreadUnusedScratchpad(Memory_Arena **arenas, uint32_t count) {
	for (auto thread_arena : ThreadContext.scratchpad.arena) {
		bool conflict = false;
		for (uint32_t index = 0; index < count; ++index) {
			if (thread_arena == arenas[index]) {
				conflict = true;
				break;
			}
		}
		if (!conflict)
			return thread_arena;
	}

	return nullptr;
}

void ResetThreadScratchpad() {
	for (auto &thread_arena : ThreadContext.scratchpad.arena) {
		MemoryArenaReset(thread_arena);
	}
}

static void *MemoryArenaAllocatorAllocate(size_t size, void *context) {
	Memory_Arena *arena = (Memory_Arena *)context;
	return PushSizeAligned(arena, size, sizeof(size_t));
}

static void *MemoryArenaAllocatorReallocate(void *ptr, size_t previous_size, size_t new_size, void *context) {
	Memory_Arena *arena = (Memory_Arena *)context;

	if (previous_size > new_size)
		return ptr;

	uint8_t *mem = (uint8_t *)arena;
	if (mem + arena->current == ((uint8_t *)ptr + previous_size)) {
		if (PushSize(arena, new_size - previous_size))
			return ptr;
		return 0;
	}

	void *new_ptr = PushSizeAligned(arena, new_size, sizeof(size_t));
	if (new_ptr) {
		memmove(new_ptr, ptr, previous_size);
		return new_ptr;
	}
	return 0;
}

static void MemoryArenaAllocatorFree(void *ptr, void *context) {}

static void *MemoryArenaAllocatorProc(Allocation_Kind kind, void *mem, size_t prev_size, size_t new_size, void *context) {
	if (kind == ALLOCATION_KIND_ALLOC) {
		return MemoryArenaAllocatorAllocate(new_size, context);
	} else if (kind == ALLOCATION_KIND_REALLOC) {
		return MemoryArenaAllocatorReallocate(mem, prev_size, new_size, context);
	} else {
		MemoryArenaAllocatorFree(mem, context);
		return nullptr;
	}
}

Memory_Allocator MemoryArenaAllocator(Memory_Arena *arena) {
	Memory_Allocator allocator;
	allocator.proc = MemoryArenaAllocatorProc;
	allocator.context = arena;
	return allocator;
}

static void *NullMemoryAllocatorProc(Allocation_Kind kind, void *mem, size_t prev_size, size_t new_size, void *context) {
	return nullptr;
}

Memory_Allocator NullMemoryAllocator() {
	Memory_Allocator allocator;
	allocator.proc = NullMemoryAllocatorProc;
	allocator.context = NULL;
	return allocator;
}

static void *DefaultMemoryAllocate(size_t size, void *context);
static void *DefaultMemoryReallocate(void *ptr, size_t previous_size, size_t new_size, void *context);
static void DefaultMemoryFree(void *ptr, void *context);

static void *DefaultMemorAllocatorProc(Allocation_Kind kind, void *mem, size_t prev_size, size_t new_size, void *context) {
	if (kind == ALLOCATION_KIND_ALLOC) {
		return DefaultMemoryAllocate(new_size, context);
	} else if (kind == ALLOCATION_KIND_REALLOC) {
		return DefaultMemoryReallocate(mem, prev_size, new_size, context);
	} else {
		DefaultMemoryFree(mem, context);
		return nullptr;
	}
}

static void InitOSContent();

void InitThreadContext(uint32_t scratchpad_size, Thread_Context_Params *params) {
	InitOSContent();

	if (scratchpad_size) {
		for (auto &thread_arena : ThreadContext.scratchpad.arena) {
			thread_arena = MemoryArenaCreate(scratchpad_size);
		}
	} else {
		memset(&ThreadContext.scratchpad, 0, sizeof(ThreadContext.scratchpad));
	}

	if (!params) {
		if (!ThreadContextDefaultParams.allocator.proc) {
			ThreadContextDefaultParams.allocator.proc = DefaultMemorAllocatorProc;
		}

		params = &ThreadContextDefaultParams;
	}

	ThreadContext.allocator = params->allocator;
}

//
//
//

void *MemoryAllocate(size_t size, Memory_Allocator allocator) {
	return allocator.proc(ALLOCATION_KIND_ALLOC, nullptr, 0, size, allocator.context);
}

void *MemoryReallocate(size_t old_size, size_t new_size, void *ptr, Memory_Allocator allocator) {
	return allocator.proc(ALLOCATION_KIND_REALLOC, ptr, old_size, new_size, allocator.context);
}

void MemoryFree(void *ptr, Memory_Allocator allocator) {
	allocator.proc(ALLOCATION_KIND_FREE, ptr, 0, 0, allocator.context);
}

void *operator new(size_t size, Memory_Allocator allocator) {
	return allocator.proc(ALLOCATION_KIND_ALLOC, nullptr, 0, size, allocator.context);
}

void *operator new[](size_t size, Memory_Allocator allocator) {
	return allocator.proc(ALLOCATION_KIND_ALLOC, nullptr, 0, size, allocator.context);
}

void *operator new(size_t size) {
	return ThreadContext.allocator.proc(ALLOCATION_KIND_ALLOC, nullptr, 0, size, ThreadContext.allocator.context);
}

void *operator new[](size_t size) {
	return ThreadContext.allocator.proc(ALLOCATION_KIND_ALLOC, nullptr, 0, size, ThreadContext.allocator.context);
}

void operator delete(void *ptr, Memory_Allocator allocator) {
	allocator.proc(ALLOCATION_KIND_FREE, ptr, 0, 0, allocator.context);
}

void operator delete[](void *ptr, Memory_Allocator allocator) {
	allocator.proc(ALLOCATION_KIND_FREE, ptr, 0, 0, allocator.context);
}

void operator delete(void *ptr) noexcept {
	ThreadContext.allocator.proc(ALLOCATION_KIND_FREE, ptr, 0, 0, ThreadContext.allocator.context);
}

void operator delete[](void *ptr) noexcept {
	ThreadContext.allocator.proc(ALLOCATION_KIND_FREE, ptr, 0, 0, ThreadContext.allocator.context);
}

//
//
//

#if PLATFORM_WINDOWS == 1
#define MICROSOFT_WINDOWS_WINBASE_H_DEFINE_INTERLOCKED_CPLUSPLUS_OVERLOADS 0
#include <Windows.h>

static void InitOSContent() {
	SetConsoleCP(CP_UTF8);
}

static void *DefaultMemoryAllocate(size_t size, void *context) {
	HANDLE heap = GetProcessHeap();
	return HeapAlloc(heap, 0, size);
}

static void *DefaultMemoryReallocate(void *ptr, size_t previous_size, size_t new_size, void *context) {
	HANDLE heap = GetProcessHeap();
	if (ptr) {
		return HeapReAlloc(heap, 0, ptr, new_size);
	} else {
		return HeapAlloc(heap, 0, new_size);
	}
}

static void DefaultMemoryFree(void *ptr, void *context) {
	HANDLE heap = GetProcessHeap();
	HeapFree(heap, 0, ptr);
}

void *VirtualMemoryAllocate(void *ptr, size_t size) {
	return VirtualAlloc(ptr, size, MEM_RESERVE, PAGE_READWRITE);
}

bool VirtualMemoryCommit(void *ptr, size_t size) {
	return VirtualAlloc(ptr, size, MEM_COMMIT, PAGE_READWRITE) != NULL;
}

bool VirtualMemoryDecommit(void *ptr, size_t size) {
	return VirtualFree(ptr, size, MEM_DECOMMIT);
}

bool VirtualMemoryFree(void *ptr, size_t size) {
	return VirtualFree(ptr, 0, MEM_RELEASE);
}

#endif

#if PLATFORM_LINUX == 1
#include <sys/mman.h>
#include <stdlib.h>

static void InitOSContent() {}

static void *DefaultMemoryAllocate(size_t size, void *context) {
	return malloc(size);
}

static void *DefaultMemoryReallocate(void *ptr, size_t previous_size, size_t new_size, void *context) {
	return realloc(ptr, new_size);
}

static void DefaultMemoryFree(void *ptr, void *context) {
	free(ptr);
}

void *VirtualMemoryAllocate(void *ptr, size_t size) {
	void *result = mmap(ptr, size, PROT_NONE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
	if (result == MAP_FAILED)
		return NULL;
	return result;
}

bool VirtualMemoryCommit(void *ptr, size_t size) {
	return mprotect(ptr, size, PROT_READ | PROT_WRITE) == 0;
}

bool VirtualMemoryDecommit(void *ptr, size_t size) {
	return mprotect(ptr, size, PROT_NONE) == 0;
}

bool VirtualMemoryFree(void *ptr, size_t size) {
	return munmap(ptr, size) == 0;
}

#endif

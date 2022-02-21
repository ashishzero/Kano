#pragma once

#include <stdarg.h>
#include <stdint.h>
#include <stddef.h>

#if defined(__clang__) || defined(__ibmxl__)
#define COMPILER_CLANG 1
#elif defined(_MSC_VER)
#define COMPILER_MSVC 1
#elif defined(__GNUC__)
#define COMPILER_GCC 1
#elif defined(__MINGW32__) || defined(__MINGW64__)
#define COMPILER_MINGW 1
#elif defined(__INTEL_COMPILER)
#define COMPILER_INTEL 1
#else
#error Missing Compiler detection
#endif

#if !defined(COMPILER_CLANG)
#define COMPILER_CLANG 0
#endif
#if !defined(COMPILER_MSVC)
#define COMPILER_MSVC 0
#endif
#if !defined(COMPILER_GCC)
#define COMPILER_GCC 0
#endif
#if !defined(COMPILER_INTEL)
#define COMPILER_INTEL 0
#endif

#if defined(__ANDROID__) || defined(__ANDROID_API__)
#define PLATFORM_ANDROID 1
#elif defined(__gnu_linux__) || defined(__linux__) || defined(linux) || defined(__linux)
#define PLATFORM_LINUX 1
#elif defined(macintosh) || defined(Macintosh)
#define PLATFORM_MAC 1
#elif defined(__APPLE__) && defined(__MACH__)
#defined PLATFORM_MAC 1
#elif defined(__APPLE__)
#define PLATFORM_IOS 1
#elif defined(_WIN64) || defined(_WIN32)
#define PLATFORM_WINDOWS 1
#else
#error Missing Operating System Detection
#endif

#if !defined(PLATFORM_ANDRIOD)
#define PLATFORM_ANDRIOD 0
#endif
#if !defined(PLATFORM_LINUX)
#define PLATFORM_LINUX 0
#endif
#if !defined(PLATFORM_MAC)
#define PLATFORM_MAC 0
#endif
#if !defined(PLATFORM_IOS)
#define PLATFORM_IOS 0
#endif
#if !defined(PLATFORM_WINDOWS)
#define PLATFORM_WINDOWS 0
#endif

#if defined(__amd64__) || defined(__amd64) || defined(__x86_64__) || defined(__x86_64) || defined(_M_AMD64) || \
    defined(_M_X64)
#define ARCH_X64 1
#elif defined(i386) || defined(__i386) || defined(__i386__) || defined(_M_IX86) || defined(_X86_)
#define ARCH_X86 1
#elif defined(__arm__) || defined(__thumb__) || defined(_ARM) || defined(_M_ARM) || defined(_M_ARMT)
#define ARCH_ARM 1
#elif defined(__aarch64__)
#define ARCH_ARM64 1
#else
#error Missing Architecture Identification
#endif

#if !defined(ARCH_X64)
#define ARCH_X64 0
#endif
#if !defined(ARCH_X86)
#define ARCH_X86 0
#endif
#if !defined(ARCH_ARM)
#define ARCH_ARM 0
#endif
#if !defined(ARCH_ARM64)
#define ARCH_ARM64 0
#endif

#if defined(__GNUC__)
#define __PROCEDURE__ __FUNCTION__
#elif defined(__DMC__) && (__DMC__ >= 0x810)
#define __PROCEDURE__ __PRETTY_PROCEDURE__
#elif defined(__FUNCSIG__)
#define __PROCEDURE__ __FUNCSIG__
#elif (defined(__INTEL_COMPILER) && (__INTEL_COMPILER >= 600)) || (defined(__IBMCPP__) && (__IBMCPP__ >= 500))
#define __PROCEDURE__ __PROCEDURE__
#elif defined(__BORLANDC__) && (__BORLANDC__ >= 0x550)
#define __PROCEDURE__ __FUNC__
#elif defined(__STDC_VERSION__) && (__STDC_VERSION__ >= 199901)
#define __PROCEDURE__ __func__
#elif defined(__cplusplus) && (__cplusplus >= 201103)
#define __PROCEDURE__ __func__
#elif defined(_MSC_VER)
#define __PROCEDURE__ __FUNCSIG__
#else
#define __PROCEDURE__ "_unknown_"
#endif

#if defined(HAVE_SIGNAL_H) && !defined(__WATCOMC__)
#include <signal.h> // raise()
#endif

#if defined(_MSC_VER)
#define TriggerBreakpoint() __debugbreak()
#elif ((!defined(__NACL__)) && \
       ((defined(__GNUC__) || defined(__clang__)) && (defined(__i386__) || defined(__x86_64__))))
#define TriggerBreakpoint() __asm__ __volatile__("int $3\n\t")
#elif defined(__386__) && defined(__WATCOMC__)
#define TriggerBreakpoint() _asm { int 0x03}
#elif defined(HAVE_SIGNAL_H) && !defined(__WATCOMC__)
#define TriggerBreakpoint() raise(SIGTRAP)
#else
#define TriggerBreakpoint() ((int *)0) = 0
#endif

#if defined(COMPILER_GCC)
#define INLINE_PROCEDURE static inline
#else
#define INLINE_PROCEDURE inline
#endif

#if !defined(BUILD_DEBUG) && !defined(BUILD_DEVELOPER) && !defined(BUILD_RELEASE)
#if defined(_DEBUG) || defined(DEBUG)
#define BUILD_DEBUG
#elif defined(NDEBUG)
#define BUILD_RELEASE
#else
#define BUILD_DEBUG
#endif
#endif

#if !defined(ASSERTION_HANDLED)
#define AssertHandle(reason, file, line, proc) TriggerBreakpoint()
#else
void AssertHandle(const char *reason, const char *file, int line, const char *proc);
#endif

#if defined(BUILD_DEBUG) || defined(BUILD_DEVELOPER)
#define DebugTriggerbreakpoint TriggerBreakpoint
#define Assert(x)                                                             \
    do                                                                        \
    {                                                                         \
        if (!(x))                                                             \
            AssertHandle("Assert Failed", __FILE__, __LINE__, __PROCEDURE__); \
    } while (0)
#else
#define DebugTriggerbreakpoint()
#define Assert(x) \
    do            \
    {             \
        0;        \
    } while (0)
#endif

#if defined(BUILD_DEBUG) || defined(BUILD_DEVELOPER)
#define Unimplemented() AssertHandle("Unimplemented procedure", __FILE__, __LINE__, __PROCEDURE__);
#else
#define Unimplemented() TriggerBreakpoint();
#endif

#ifdef __GNUC__
[[noreturn]] inline __attribute__((always_inline)) void Unreachable() { DebugTriggerbreakpoint(); __builtin_unreachable(); }
#elif defined(_MSC_VER)
[[noreturn]] __forceinline void Unreachable() { DebugTriggerbreakpoint(); __assume(false); }
#else // ???
inline void Unreachable() { TriggerBreakpoint(); }
#endif

#define NoDefaultCase() default: Unreachable(); break

//
//
//

#define ArrayCount(a) (sizeof(a) / sizeof((a)[0]))
#define Minimum(a, b) (((a) < (b)) ? (a) : (b))
#define Maximum(a, b) (((a) > (b)) ? (a) : (b))
#define Clamp(a, b, v) Minimum(b, Maximum(a, v))

#define SetFlag(expr, flag) ((expr) |= (flag))
#define ClearFlag(expr, flag) ((expr) &= ~(flag))
#define ToggleFlag(expr, flag) ((expr) ^= (flag))
#define IsPower2(value) (((value) != 0) && ((value) & ((value)-1)) == 0)
#define AlignPower2Up(x, p) (((x) + (p)-1) & ~((p)-1))
#define AlignPower2Down(x, p) ((x) & ~((p)-1))

#define ByteSwap16(a) ((((a)&0x00FF) << 8) | (((a)&0xFF00) >> 8))
#define ByteSwap32(a) \
    ((((a)&0x000000FF) << 24) | (((a)&0x0000FF00) << 8) | (((a)&0x00FF0000) >> 8) | (((a)&0xFF000000) >> 24))
#define ByteSwap64(a)                                                                                                  \
    ((((a)&0x00000000000000FFULL) << 56) | (((a)&0x000000000000FF00ULL) << 40) | (((a)&0x0000000000FF0000ULL) << 24) | \
     (((a)&0x00000000FF000000ULL) << 8) | (((a)&0x000000FF00000000ULL) >> 8) | (((a)&0x0000FF0000000000ULL) >> 24) |   \
     (((a)&0x00FF000000000000ULL) >> 40) | (((a)&0xFF00000000000000ULL) >> 56))

#define KiloBytes(n) ((n)*1024u)
#define MegaBytes(n) (KiloBytes(n) * 1024u)
#define GigaBytes(n) (MegaBytes(n) * 1024u)
#define MEMORY_ARENA_COMMIT_SIZE MegaBytes(64)

struct String {
	int64_t length;
	uint8_t *data;

	String() : data(0), length(0) {}
	template <int64_t _Length>
	constexpr String(const char(&a)[_Length]) : data((uint8_t *)a), length(_Length - 1) {}
	String(const uint8_t *_Data, int64_t _Length) : data((uint8_t *)_Data), length(_Length) {}
	String(const char *_Data, int64_t _Length) : data((uint8_t *)_Data), length(_Length) {}
	const uint8_t &operator[](const int64_t index) const { Assert(index < length); return data[index]; }
	uint8_t &operator[](const int64_t index) { Assert(index < length); return data[index]; }
};

bool operator==(const String a, const String b);
bool operator!=(const String a, const String b);

#define _zConcatInternal(x, y) x##y
#define _zConcat(x, y) _zConcatInternal(x, y)

template <typename T>
struct Exit_Scope {
	T lambda;
	Exit_Scope(T lambda) : lambda(lambda) {
	}
	~Exit_Scope() {
		lambda();
	}
};
struct Exit_Scope_Help {
	template <typename T>
	Exit_Scope<T> operator+(T t) {
		return t;
	}
};
#define Defer const auto &_zConcat(defer__, __LINE__) = Exit_Scope_Help() + [&]()

//
//
//

uint8_t *AlignPointer(uint8_t *location, size_t alignment);
size_t AlignSize(size_t location, size_t alignment);

struct Memory_Arena {
	size_t current;
	size_t reserved;
	size_t committed;
};

Memory_Arena *MemoryArenaCreate(size_t max_size);
void MemoryArenaDestroy(Memory_Arena *arena);
void MemoryArenaReset(Memory_Arena *arena);
size_t MemoryArenaSizeLeft(Memory_Arena *arena);

void *PushSize(Memory_Arena *arena, size_t size);
void *PushSizeAligned(Memory_Arena *arena, size_t size, uint32_t alignment);
bool SetAllocationPosition(Memory_Arena *arena, size_t pos);

#define PushType(arena, type) (type *)PushSize(arena, sizeof(type))
#define PushArray(arena, type, count) (type *)PushSize(arena, sizeof(type) * (count))
#define PushArrayAligned(arena, type, count, alignment) (type *)PushSizeAligned(arena, sizeof(type) * (count), alignment)

typedef struct Temporary_Memory {
	Memory_Arena *arena;
	size_t position;
} Temporary_Memory;

Temporary_Memory BeginTemporaryMemory(Memory_Arena *arena);
void EndTemporaryMemory(Temporary_Memory *temp);
void FreeTemporaryMemory(Temporary_Memory *temp);


#ifndef THREAD_CONTEXT_SCRATCHPAD_MAX_ARENAS
#define THREAD_CONTEXT_SCRATCHPAD_MAX_ARENAS 2
#endif // !THREAD_CONTEXT_SCRATCHPAD_MAX_ARENAS

struct Thread_Scratchpad {
	Memory_Arena *arena[THREAD_CONTEXT_SCRATCHPAD_MAX_ARENAS];
};

//
//
//

enum Allocation_Kind { ALLOCATION_KIND_ALLOC, ALLOCATION_KIND_REALLOC, ALLOCATION_KIND_FREE };
typedef void *(*Memory_Allocator_Proc)(Allocation_Kind kind, void *mem, size_t prev_size, size_t new_size, void *context);

struct Memory_Allocator {
	Memory_Allocator_Proc proc;
	void *context;
};

//
//
//

typedef struct Thread_Context {
	Thread_Scratchpad scratchpad;
	Memory_Allocator  allocator;
} Thread_Context;

extern thread_local Thread_Context ThreadContext;

//
//
//

Memory_Arena *ThreadScratchpad();
Memory_Arena *ThreadScratchpadI(uint32_t i);
Memory_Arena *ThreadUnusedScratchpad(Memory_Arena **arenas, uint32_t count);
void ResetThreadScratchpad();

Memory_Allocator MemoryArenaAllocator(Memory_Arena *arena);
Memory_Allocator NullMemoryAllocator();

//
//
//

struct Thread_Context_Params {
	Memory_Allocator allocator;
};

void InitThreadContext(uint32_t scratchpad_size, Thread_Context_Params *params = nullptr);


void *MemoryAllocate(size_t size, Memory_Allocator allocator = ThreadContext.allocator);
void *MemoryReallocate(size_t old_size, size_t new_size, void *ptr, Memory_Allocator allocator = ThreadContext.allocator);
void MemoryFree(void *ptr, Memory_Allocator allocator = ThreadContext.allocator);

void *operator new(size_t size, Memory_Allocator allocator);
void *operator new[](size_t size, Memory_Allocator allocator);
void *operator new(size_t size);
void *operator new[](size_t size);
void  operator delete(void *ptr, Memory_Allocator allocator);
void  operator delete[](void *ptr, Memory_Allocator allocator);
void  operator delete(void *ptr) noexcept;
void  operator delete[](void *ptr) noexcept;

//
//
//

void *VirtualMemoryAllocate(void *ptr, size_t size);
bool VirtualMemoryCommit(void *ptr, size_t size);
bool VirtualMemoryDecommit(void *ptr, size_t size);
bool VirtualMemoryFree(void *ptr, size_t size);

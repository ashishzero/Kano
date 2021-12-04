#pragma once

#include <stdarg.h>
#include <stdint.h>
#include <stddef.h>
#include <string.h>

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
#define OS_ANDROID 1
#elif defined(__gnu_linux__) || defined(__linux__) || defined(linux) || defined(__linux)
#define PLATFORM_OS_LINUX 1
#elif defined(macintosh) || defined(Macintosh)
#define PLATFORM_OS_MAC 1
#elif defined(__APPLE__) && defined(__MACH__)
#defined PLATFORM_OS_MAC 1
#elif defined(__APPLE__)
#define PLATFORM_OS_IOS 1
#elif defined(_WIN64) || defined(_WIN32)
#define PLATFORM_OS_WINDOWS 1
#else
#error Missing Operating System Detection
#endif

#if !defined(PLATFORM_OS_ANDRIOD)
#define PLATFORM_OS_ANDRIOD 0
#endif
#if !defined(PLATFORM_OS_LINUX)
#define PLATFORM_OS_LINUX 0
#endif
#if !defined(PLATFORM_OS_MAC)
#define PLATFORM_OS_MAC 0
#endif
#if !defined(PLATFORM_OS_IOS)
#define PLATFORM_OS_IOS 0
#endif
#if !defined(PLATFORM_OS_WINDOWS)
#define PLATFORM_OS_WINDOWS 0
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
#define handle_assertion(reason, file, line, proc) TriggerBreakpoint()
#else
#if defined(__cplusplus)
extern void handle_assertion(const char *reason, const char *file, int line, const char *proc);
#endif
#endif

#if defined(BUILD_DEBUG) || defined(BUILD_DEVELOPER)
#define DebugTriggerbreakpoint TriggerBreakpoint
#define Assert(x)                                                             \
    do                                                                        \
    {                                                                         \
        if (!(x))                                                             \
            handle_assertion("Assert Failed", __FILE__, __LINE__, __PROCEDURE__); \
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
#define Unimplemented() handle_assertion("Unimplemented procedure", __FILE__, __LINE__, __PROCEDURE__);
#define Unreachable() handle_assertion("Unreachable code path", __FILE__, __LINE__, __PROCEDURE__);
#define NoDefaultCase()                                                     \
    default:                                                                \
        handle_assertion("No default case", __FILE__, __LINE__, __PROCEDURE__); \
        break
#else
#define Unimplemented() TriggerBreakpoint();
#define Unreachable() TriggerBreakpoint();
#define NoDefaultCase()      \
    default:                 \
        TriggerBreakpoint(); \
        break
#endif

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

struct String {
	int64_t length;
	uint8_t *data;

	String() : data(0), length(0) {
	}
	template <int64_t _Length>
	constexpr String(const char(&a)[_Length]) : data((uint8_t *)a), length(_Length - 1) {
	}
	String(const uint8_t *_Data, int64_t _Length) : data((uint8_t *)_Data), length(_Length) {
	}
	const uint8_t &operator[](const int64_t index) const {
		Assert(index < length);
		return data[index];
	}
	uint8_t &operator[](const int64_t index) {
		Assert(index < length);
		return data[index];
	}

	inline bool operator==(String b) {
		if (length != b.length) return false;
		return memcmp(data, b.data, length) == 0;
	}
	inline bool operator!=(String b) {
		return !(*this == b);
	}
};

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

template <typename T>
struct Array_View {
	int64_t count = 0;
	T *data = nullptr;

	Array_View() = default;
	Array_View(T *p, int64_t n) : count(n), data(p) {}

	template <int64_t _Count>
	constexpr Array_View(const T(&a)[_Count]) : count(_Count), data((T *)a) {}

	T &operator[](int64_t index) const { Assert(index < count); return data[index]; }
	T *begin() { return data; }
	T *end() { return data + count; }
	const T *begin() const { return data; }
	const T *end() const { return data + count; }
};

template <typename T>
struct Array {
	int64_t count = 0;
	T *data = nullptr;
	int64_t capacity = 0;

	operator Array_View<T>() { return Array_View<T>(data, count); }
	operator const Array_View<T>() const { return Array_View<T>(data, count); }

	T &operator[](int64_t i) { Assert(i >= 0 && i < count); return data[i]; }
	const T &operator[](int64_t i) const { Assert(i >= 0 && i < count); return data[i]; }
	T *begin() { return data; }
	T *end() { return data + count; }
	const T *begin() const { return data; }
	const T *end() const { return data + count; }

	int64_t _get_grow_capacity(int64_t size) const {
		auto new_capacity = capacity ? (capacity + capacity / 2) : 8;
		return new_capacity > size ? new_capacity : size;
	}

	void reserve(int64_t new_capacity) {
		if (new_capacity <= capacity)
			return;
		T *_data = (T *)realloc(data, new_capacity * sizeof(T));
		if (_data) {
			data = _data;
			capacity = new_capacity;
		}
	}

	T *first() { Assert(count); return data; }
	const T *first() const { Assert(count); return data; }
	T *last() { Assert(count); return data + count - 1; }
	const T *last() const { Assert(count); return data + count - 1; }

	template <typename... Args> void emplace(const Args &...args) {
		if (count == capacity) {
			auto n = _get_grow_capacity(capacity + 1);
			reserve(n);
		}
		data[count] = T(args...);
		count += 1;
	}

	T *add() {
		if (count == capacity) {
			auto c = _get_grow_capacity(capacity + 1);
			reserve(c);
		}
		count += 1;
		return data + (count - 1);
	}

	T *addn(uint32_t n) {
		if (count + n > capacity) {
			auto c = _get_grow_capacity(count + n);
			reserve(c);
		}
		T *ptr = data + count;
		count += n;
		return ptr;
	}

	void add(const T &d) {
		T *m = add();
		*m = d;
	}

	void copy(Array_View<T> src) {
		if (src.count + count >= capacity) {
			auto c = _get_grow_capacity(src.count + count + 1);
			reserve(c);
		}
		memcpy(data + count, src.data, src.count * sizeof(T));
		count += src.count;
	}

	void remove_last() {
		Assert(count > 0);
		count -= 1;
	}

	void remove(int64_t index) {
		Assert(index < count);
		memmove(data + index, data + index + 1, (count - index - 1) * sizeof(T));
		count -= 1;
	}

	void remove_unordered(int64_t index) {
		Assert(index < count);
		data[index] = data[count - 1];
		count -= 1;
	}

	void insert(int64_t index, const T &v) {
		Assert(index < count + 1);
		add();
		for (auto move_index = count - 1; move_index > index; --move_index) {
			data[move_index] = data[move_index - 1];
		}
		data[index] = v;
	}

	void insert_unordered(int64_t index, const T &v) {
		Assert(index < count + 1);
		add();
		data[count - 1] = data[index];
		data[index] = v;
	}

	int64_t find(const T &v) const {
		for (int64_t index = 0; index < count; ++index) {
			auto elem = data + index;
			if (*elem == v) {
				return index;
			}
		}
		return -1;
	}

	template <typename SearchFunc, typename... Args>
	int64_t find(SearchFunc func, const Args &...args) const {
		for (int64_t index = 0; index < count; ++index) {
			if (func(data[index], args...)) {
				return index;
			}
		}
		return -1;
	}

	void reset() {
		count = 0;
	}
};

template <typename T> inline void array_free(Array<T> *a) {
	if (a->data)
		free(a->data, &a->allocator);
}

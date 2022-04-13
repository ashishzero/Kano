#pragma once

#include "KrCommon.h"
#include <string.h>
#include <stdio.h>
#include <stdarg.h>

INLINE_PROCEDURE String FmtStrV(Memory_Arena *arena, const char *fmt, va_list list) {
	va_list args;
	va_copy(args, list);
	int   len = 1 + vsnprintf(NULL, 0, fmt, args);
	char *buf = (char *)PushSize(arena, len);
	vsnprintf(buf, len, fmt, list);
	va_end(args);
	return String(buf, len - 1);
}

INLINE_PROCEDURE String FmtStr(Memory_Arena *arena, const char *fmt, ...) {
	va_list args;
	va_start(args, fmt);
	String string = FmtStrV(arena, fmt, args);
	va_end(args);
	return string;
}

INLINE_PROCEDURE bool IsSpace(uint32_t ch) {
	return ch == ' ' || ch == '\f' || ch == '\n' || ch == '\r' || ch == '\t' || ch == '\v';
}

INLINE_PROCEDURE String StrTrim(String str) {
	ptrdiff_t trim = 0;
	for (ptrdiff_t index = 0; index < str.length; ++index) {
		if (IsSpace(str.data[index]))
			trim += 1;
		else
			break;
	}

	str.data += trim;
	str.length -= trim;

	for (ptrdiff_t index = str.length - 1; index >= 0; --index) {
		if (IsSpace(str.data[index])) {
			str.data[index] = '\0';
			str.length -= 1;
		}
		else
			break;
	}

	return str;
}

INLINE_PROCEDURE String StrRemovePrefix(String str, ptrdiff_t count) {
	Assert(str.length >= count);
	str.data += count;
	str.length -= count;
	return str;
}

INLINE_PROCEDURE String StrRemoveSuffix(String str, ptrdiff_t count) {
	Assert(str.length >= count);
	str.length -= count;
	return str;
}

INLINE_PROCEDURE ptrdiff_t StrCopy(String src, char *const dst, ptrdiff_t dst_size, ptrdiff_t count) {
	ptrdiff_t copied = (ptrdiff_t)Clamp(dst_size, src.length, count);
	memcpy(dst, src.data, copied);
	return copied;
}

INLINE_PROCEDURE String StrDuplicate(String src) {
	String dst;
	dst.data = (uint8_t *)MemoryAllocate(src.length + 1, ThreadContext.allocator);
	memcpy(dst.data, src.data, src.length);
	dst.length = src.length;
	dst.data[dst.length] = 0;
	return dst;
}

INLINE_PROCEDURE String StrDuplicateArena(String src, Memory_Arena *arena) {
	String dst;
	dst.data = (uint8_t *)PushSize(arena, src.length + 1);
	memcpy(dst.data, src.data, src.length);
	dst.length = src.length;
	dst.data[dst.length] = 0;
	return dst;
}

INLINE_PROCEDURE String SubStr(String str, ptrdiff_t index, ptrdiff_t count) {
	Assert(index < str.length);
	count = (ptrdiff_t)Minimum(str.length - index, count);
	return String(str.data + index, count);
}

INLINE_PROCEDURE int StrCompare(String a, String b) {
	ptrdiff_t count = (ptrdiff_t)Minimum(a.length, b.length);
	return memcmp(a.data, b.data, count);
}

INLINE_PROCEDURE int StrCompareCaseInsensitive(String a, String b) {
	ptrdiff_t count = (ptrdiff_t)Minimum(a.length, b.length);
	for (ptrdiff_t index = 0; index < count; ++index) {
		if (a.data[index] != b.data[index] && a.data[index] + 32 != b.data[index] && a.data[index] != b.data[index] + 32) {
			return a.data[index] - b.data[index];
		}
	}
	return 0;
}

INLINE_PROCEDURE bool StrMatch(String a, String b) {
	if (a.length != b.length)
		return false;
	return StrCompare(a, b) == 0;
}

INLINE_PROCEDURE bool StrMatchCaseInsensitive(String a, String b) {
	if (a.length != b.length)
		return false;
	return StrCompareCaseInsensitive(a, b) == 0;
}

INLINE_PROCEDURE bool StrStartsWith(String str, String sub) {
	if (str.length < sub.length)
		return false;
	return StrCompare(String(str.data, sub.length), sub) == 0;
}

INLINE_PROCEDURE bool StrStartsWithCaseInsensitive(String str, String sub) {
	if (str.length < sub.length)
		return false;
	return StrCompareCaseInsensitive(String(str.data, sub.length), sub) == 0;
}

INLINE_PROCEDURE bool StrStartsWithCharacter(String str, uint8_t c) {
	return str.length && str.data[0] == c;
}

INLINE_PROCEDURE bool StrStartsWithCharacterCaseInsensitive(String str, uint8_t c) {
	return str.length && (str.data[0] == c || str.data[0] + 32 == c || str.data[0] == c + 32);
}

INLINE_PROCEDURE bool StrEndsWith(String str, String sub) {
	if (str.length < sub.length)
		return false;
	return StrCompare(String(str.data + str.length - sub.length, sub.length), sub) == 0;
}

INLINE_PROCEDURE bool StringEndsWithCaseInsensitive(String str, String sub) {
	if (str.length < sub.length)
		return false;
	return StrCompareCaseInsensitive(String(str.data + str.length - sub.length, sub.length), sub) == 0;
}

INLINE_PROCEDURE bool StrEndsWithCharacter(String str, uint8_t c) {
	return str.length && str.data[str.length - 1] == c;
}

INLINE_PROCEDURE bool StrEndsWithCharacterCaseInsensitive(String str, uint8_t c) {
	return str.length &&
		(str.data[str.length - 1] == c || str.data[str.length - 1] + 32 == c || str.data[str.length - 1] == c + 32);
}

INLINE_PROCEDURE char *StrNullTerminated(char *buffer, String str) {
	memcpy(buffer, str.data, str.length);
	buffer[str.length] = 0;
	return buffer;
}

INLINE_PROCEDURE char *StrNullTerminatedArena(Memory_Arena *arena, String str) {
	return StrNullTerminated((char *)PushSize(arena, str.length + 1), str);
}

INLINE_PROCEDURE ptrdiff_t StrFind(String str, String key, ptrdiff_t pos) {
	ptrdiff_t index = Clamp(0, str.length - 1, pos);
	while (str.length >= key.length) {
		if (StrCompare(String(str.data, key.length), key) == 0) {
			return index;
		}
		index += 1;
		str = StrRemovePrefix(str, 1);
	}
	return -1;
}

INLINE_PROCEDURE ptrdiff_t StrFindCharacter(String str, uint8_t key, ptrdiff_t pos) {
	for (ptrdiff_t index = Clamp(0, str.length - 1, pos); index < str.length; ++index)
		if (str.data[index] == key)
			return index;
	return -1;
}

INLINE_PROCEDURE ptrdiff_t StrReverseFind(String str, String key, ptrdiff_t pos) {
	ptrdiff_t index = Clamp(0, str.length - key.length, pos);
	while (index >= 0) {
		if (StrCompare(String(str.data + index, key.length), key) == 0)
			return index;
		index -= 1;
	}
	return -1;
}

INLINE_PROCEDURE ptrdiff_t StrReverseFindCharacter(String str, uint8_t key, ptrdiff_t pos) {
	for (ptrdiff_t index = Clamp(0, str.length - 1, pos); index >= 0; --index)
		if (str.data[index] == key)
			return index;
	return -1;
}

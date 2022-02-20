#pragma once

#include "Kr/KrCommon.h"

constexpr uint32_t STRING_BUILDER_BUCKET_SIZE = 16 * 1024;

struct String_Builder {
	struct Bucket {
		Bucket *next = nullptr;
		int32_t written = 0;
		uint8_t data[STRING_BUILDER_BUCKET_SIZE];
	};

	Bucket head;
	Bucket *current = &head;

	int64_t written = 0;

	Bucket *free_list = nullptr;

	Memory_Allocator allocator = ThreadContext.allocator;
};

int WriteBuffer(String_Builder *builder, void *buffer, int64_t size);
int Write(String_Builder *builder, bool value);
int Write(String_Builder *builder, char value);
int Write(String_Builder *builder, uint8_t value);
int Write(String_Builder *builder, int32_t value);
int Write(String_Builder *builder, uint32_t value);
int Write(String_Builder *builder, int64_t value);
int Write(String_Builder *builder, uint64_t value);
int Write(String_Builder *builder, float value);
int Write(String_Builder *builder, double value);
int Write(String_Builder *builder, void *value);
int Write(String_Builder *builder, const char *value);
int Write(String_Builder *builder, String value);

template <int64_t _Length>
int Write(String_Builder *builder, const char(&a)[_Length]) {
	return Write(builder, String(a, _Length));
}

int WriteFormatted(String_Builder *builder, const char *format);

template <typename Type, typename ...Args>
int WriteFormatted(String_Builder *builder, const char *format, Type value, Args... args) {
	int written = 0;

	for (; *format; ++format) {
		if (*format == '%') {
			written += Write(builder, value);
			return written + WriteFormatted(builder, format + 1, args...);
		}
		written += Write(builder, *format);
	}

	return written;
}

String BuildString(String_Builder *builder, Memory_Allocator &allocator = ThreadContext.allocator);

void ResetBuilder(String_Builder *builder);
void FreeBuilder(String_Builder *builder);

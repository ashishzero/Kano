#pragma once
#include "Common.h"

constexpr uint32_t STRING_BUILDER_BUCKET_SIZE = 2048;

struct String_Builder
{
	struct Bucket
	{
		uint8_t data[STRING_BUILDER_BUCKET_SIZE];
		Bucket *next = nullptr;
	};

	Bucket   first;
	Bucket * current;
	uint32_t write_index;

	String_Builder()
	{
		current     = &first;
		write_index = 0;
	}
};

char * string_builder_push(String_Builder *builder, uint32_t size);
String string_builder_copy(String_Builder *builder, String src);
String string_builder_copy(String_Builder *builder, uint8_t *src, int64_t size);

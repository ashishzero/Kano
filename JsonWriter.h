#pragma once
#include "StringBuilder.h"

struct Json_Writer {
	bool depth[4096] = {};
	int depth_index = 0;

	String_Builder *builder = nullptr;

	void next_element() {
		if (depth[depth_index])
			Write(builder, ",");
		else
			depth[depth_index] = true;
	}

	void push_scope() {
		Assert(depth_index < ArrayCount(depth));
		next_element();
		depth_index += 1;
		depth[depth_index] = false;
	}

	void pop_scope() {
		depth_index -= 1;
	}

	void begin_object() {
		push_scope();
		Write(builder, "{");
	}

	void end_object() {
		Write(builder, "}");
		pop_scope();
	}

	void begin_array() {
		push_scope();
		Write(builder, "[");
	}

	void end_array() {
		Write(builder, "]");
		pop_scope();
	}

	void write_key(const char *key) {
		push_scope();
		WriteFormatted(builder, "\"%\": ", key);
	}

	template <typename ...Args>
	void write_single_value(const char *fmt, Args... args) {
		Write(builder, "\"");
		WriteFormatted(builder, fmt, args...);
		Write(builder, "\"");
		pop_scope();
	}

	void begin_string_value() {
		Write(builder, "\"");
	}

	void append_builder(String_Builder *src) {
		for (auto buk = &src->head; buk; buk = buk->next) {
			WriteBuffer(builder, buk->data, buk->written);
		}
	}

	template <typename ...Args>
	void append_string_value(const char *fmt, Args... args) {
		WriteFormatted(builder, fmt, args...);
	}

	void end_string_value() {
		Write(builder, "\"");
		pop_scope();
	}

	template <typename ...Args>
	void write_key_value(const char *key, const char *fmt, Args... args) {
		next_element();
		WriteFormatted(builder, "\"%\": ", key);
		Write(builder, "\"");
		WriteFormatted(builder, fmt, args...);
		Write(builder, "\"");
	}
};

#pragma once
#include "StringBuilder.h"

struct Json_Writer {
	bool elements[4096 * 2] = {};
	int index = 0;

	String_Builder *builder = nullptr;

	void next_element(bool is_key = false) {
		if (elements[index])
			Write(builder, ",");
		if (is_key)
			elements[index] = 0;
		else
			elements[index] = true;
	}

	void push_scope() {
		next_element();
		Assert(index >= 0 && index < ArrayCount(elements) - 1);
		index += 1;
		elements[index] = 0;
	}

	void pop_scope() {
		Assert(index > 0 && index < ArrayCount(elements));
		index -= 1;
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
		next_element(true);
		WriteFormatted(builder, "\"%\": ", key);
	}

	template <typename ...Args>
	void write_single_value(const char *fmt, Args... args) {
		next_element();
		Write(builder, "\"");
		WriteFormatted(builder, fmt, args...);
		Write(builder, "\"");
	}

	void begin_string_value() {
		next_element();
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

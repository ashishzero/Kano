#pragma once

#include "Common.h"
#include "SyntaxNode.h"

#include <stdio.h>
#include <stdlib.h>

struct Error_Node
{
	String          message;
	Syntax_Location location;
	Error_Node *    next;
};

struct Error_List
{
	Error_Node  first;
	Error_Node *last;
};

static inline String string_vprint(const char *fmt, va_list list)
{
	va_list args;
	va_copy(args, list);
	int   len = 1 + vsnprintf(NULL, 0, fmt, args);
	char *buf = (char *)malloc(len);
	vsnprintf(buf, len, fmt, list);
	va_end(args);
	return String((uint8_t *)buf, len - 1);
}

static inline String string_print(const char *fmt, ...)
{
	va_list args;
	va_start(args, fmt);
	String string = string_vprint(fmt, args);
	va_end(args);
	return string;
}

static inline void error_new(Error_List *list, const Syntax_Location &location, String message)
{
	auto error       = new Error_Node;
	error->message   = message;
	error->next      = nullptr;
	error->location  = location;

	list->last->next = error;
	list->last       = error;
}

static inline void error_vfmt(Error_List *list, const Syntax_Location &location, const char *fmt, va_list args)
{
	String message = string_vprint(fmt, args);
	error_new(list, location, message);
}

static inline void error_fmt(Error_List *list, const Syntax_Location &location, const char *fmt, ...)
{
	va_list args;
	va_start(args, fmt);
	String message = string_vprint(fmt, args);
	va_end(args);
	error_new(list, location, message);
}

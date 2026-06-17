// src/common/string.h
#ifndef BEVEL_STRING_H
#define BEVEL_STRING_H


#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include "common/arena.h" 

// String - это "строковое представление" (string view)
typedef struct String {   // <-- добавьте тег
    const char* data;
    size_t length;
} String;

// Макросы для создания строк
#define STRING_FROM_CSTR(cstr) ((String){(cstr), (cstr) ? strlen(cstr) : 0})
#define STRING_FROM_LITERAL(lit) ((String){(lit), sizeof(lit) - 1})
#define STRING_EMPTY ((String){NULL, 0})

// Объявления функций (прототипы)
String string_new(const char* data, size_t length);
String string_from_cstr(const char* cstr);
bool string_equals(String a, String b);
bool string_starts_with(String s, String prefix);
bool string_ends_with(String s, String suffix);
int string_compare(String a, String b);
String string_substr(String s, size_t start, size_t length);
uint32_t string_hash(String s);
char* string_to_cstr(String s);

// Конкатенация двух строк
String string_concat(Arena* arena, String a, String b);

// Конкатенация трёх строк
String string_concat3(Arena* arena, String a, String b, String c);

#endif // BEVEL_STRING_H
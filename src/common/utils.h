// common/utils.h
#ifndef BEVEL_UTILS_H
#define BEVEL_UTILS_H

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#include "string.h"
#include "arena.h"

// Классификация символов (для лексера)
bool is_alpha(char c);
bool is_digit(char c);
bool is_alnum(char c);
bool is_whitespace(char c);
bool is_hex_digit(char c);

// Работа с файлами
char* read_file(const char* path, size_t* out_size);
bool  write_file(const char* path, const char* data, size_t size);
bool  file_exists(const char* path);

// Парсинг чисел
int64_t parse_int(String s, int base, bool* out_ok);
uint64_t parse_uint(String s, int base, bool* out_ok);
double  parse_float(String s, bool* out_ok);

// Форматирование строк в арене
char* format_string(Arena* arena, const char* fmt, ...);

// Хеш-функции
uint32_t hash_string(const char* str);
uint32_t hash_bytes(const void* data, size_t length);

// Вывод с цветами (ANSI)
void print_colored(const char* color, const char* text);
void print_error(const char* fmt, ...);
void print_warning(const char* fmt, ...);
void print_info(const char* fmt, ...);

#endif // BEVEL_UTILS_H
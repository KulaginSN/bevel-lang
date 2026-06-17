// common/utils.c
#include "utils.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <ctype.h>
#include <sys/stat.h>

// ===== Классификация символов =====

bool is_alpha(char c) {
    return (c >= 'a' && c <= 'z') ||
           (c >= 'A' && c <= 'Z') ||
           c == '_';
}

bool is_digit(char c) {
    return c >= '0' && c <= '9';
}

bool is_alnum(char c) {
    return is_alpha(c) || is_digit(c);
}

bool is_whitespace(char c) {
    return c == ' ' || c == '\t' || c == '\r' || c == '\n';
}

bool is_hex_digit(char c) {
    return is_digit(c) ||
           (c >= 'a' && c <= 'f') ||
           (c >= 'A' && c <= 'F');
}

// ===== Работа с файлами =====

char* read_file(const char* path, size_t* out_size) {
    FILE* file = fopen(path, "rb");
    if (!file) {
        fprintf(stderr, "[Utils] Cannot open file: %s\n", path);
        return NULL;
    }
    
    fseek(file, 0, SEEK_END);
    long size = ftell(file);
    fseek(file, 0, SEEK_SET);
    
    if (size < 0) {
        fclose(file);
        return NULL;
    }
    
    char* buffer = (char*)malloc(size + 1);
    if (!buffer) {
        fclose(file);
        return NULL;
    }
    
    size_t read = fread(buffer, 1, size, file);
    buffer[read] = '\0';
    fclose(file);
    
    if (out_size) *out_size = read;
    return buffer;
}

bool write_file(const char* path, const char* data, size_t size) {
    FILE* file = fopen(path, "wb");
    if (!file) return false;
    size_t written = fwrite(data, 1, size, file);
    fclose(file);
    return written == size;
}

bool file_exists(const char* path) {
    struct stat st;
    return stat(path, &st) == 0;
}

// ===== Парсинг чисел =====

int64_t parse_int(String s, int base, bool* out_ok) {
    char* cstr = string_to_cstr(s);
    if (!cstr) { if (out_ok) *out_ok = false; return 0; }
    
    char* endptr;
    int64_t result = strtoll(cstr, &endptr, base);
    bool ok = (endptr != cstr) && (*endptr == '\0');
    
    free(cstr);
    if (out_ok) *out_ok = ok;
    return result;
}

uint64_t parse_uint(String s, int base, bool* out_ok) {
    char* cstr = string_to_cstr(s);
    if (!cstr) { if (out_ok) *out_ok = false; return 0; }
    
    char* endptr;
    uint64_t result = strtoull(cstr, &endptr, base);
    bool ok = (endptr != cstr) && (*endptr == '\0');
    
    free(cstr);
    if (out_ok) *out_ok = ok;
    return result;
}

double parse_float(String s, bool* out_ok) {
    char* cstr = string_to_cstr(s);
    if (!cstr) { if (out_ok) *out_ok = false; return 0.0; }
    
    char* endptr;
    double result = strtod(cstr, &endptr);
    bool ok = (endptr != cstr) && (*endptr == '\0');
    
    free(cstr);
    if (out_ok) *out_ok = ok;
    return result;
}

// ===== Форматирование =====

char* format_string(Arena* arena, const char* fmt, ...) {
    va_list args;
    va_list args_copy;
    
    // Определяем необходимый размер
    va_start(args, fmt);
    va_copy(args_copy, args);
    int size = vsnprintf(NULL, 0, fmt, args_copy);
    va_end(args_copy);
    va_end(args);
    
    if (size < 0) return NULL;
    
    // Выделяем в арене
    char* buffer = (char*)arena_alloc(arena, size + 1, 1);
    if (!buffer) return NULL;
    
    va_start(args, fmt);
    vsnprintf(buffer, size + 1, fmt, args);
    va_end(args);
    
    return buffer;
}

// ===== Хеширование =====

uint32_t hash_string(const char* str) {
    if (!str) return 0;
    return hash_bytes(str, strlen(str));
}

uint32_t hash_bytes(const void* data, size_t length) {
    // FNV-1a
    uint32_t hash = 2166136261u;
    const uint8_t* bytes = (const uint8_t*)data;
    for (size_t i = 0; i < length; i++) {
        hash ^= bytes[i];
        hash *= 16777619u;
    }
    return hash;
}

// ===== Цветной вывод =====

#define COLOR_RESET   "\033[0m"
#define COLOR_RED     "\033[31m"
#define COLOR_YELLOW  "\033[33m"
#define COLOR_BLUE    "\033[34m"
#define COLOR_GREEN   "\033[32m"
#define COLOR_BOLD    "\033[1m"

void print_colored(const char* color, const char* text) {
    printf("%s%s%s", color, text, COLOR_RESET);
}

void print_error(const char* fmt, ...) {
    fprintf(stderr, "%s%serror%s: ", COLOR_BOLD, COLOR_RED, COLOR_RESET);
    va_list args;
    va_start(args, fmt);
    vfprintf(stderr, fmt, args);
    va_end(args);
    fprintf(stderr, "\n");
}

void print_warning(const char* fmt, ...) {
    fprintf(stderr, "%swarning%s: ", COLOR_YELLOW, COLOR_RESET);
    va_list args;
    va_start(args, fmt);
    vfprintf(stderr, fmt, args);
    va_end(args);
    fprintf(stderr, "\n");
}

void print_info(const char* fmt, ...) {
    printf("%s%sinfo%s: ", COLOR_BOLD, COLOR_BLUE, COLOR_RESET);
    va_list args;
    va_start(args, fmt);
    vprintf(fmt, args);
    va_end(args);
    printf("\n");
}
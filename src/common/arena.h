// src/common/arena.h
#ifndef BEVEL_ARENA_H
#define BEVEL_ARENA_H

#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>

// Forward declaration для String (чтобы избежать циклической зависимости)
typedef struct String String;

typedef struct {
    char*  buffer;
    size_t capacity;
    size_t offset;
} Arena;

// Создание и уничтожение
Arena* arena_init(size_t capacity);
void   arena_free(Arena* arena);

// Выделение памяти с выравниванием
void*  arena_alloc(Arena* arena, size_t size, size_t align);

// Удобные макросы
#define ARENA_ALLOC(arena, type) \
    ((type*)arena_alloc((arena), sizeof(type), _Alignof(type)))

#define ARENA_ARRAY(arena, type, count) \
    ((type*)arena_alloc((arena), sizeof(type) * (count), _Alignof(type)))

// Скопировать данные в арену
void* arena_copy(Arena* arena, const void* data, size_t size);

// Скопировать строку в арену
String arena_string(Arena* arena, String s);
const char* arena_cstr(Arena* arena, const char* cstr);

// Mark/Release — для временных выделений
size_t arena_mark(Arena* arena);
void   arena_release(Arena* arena, size_t mark);

// Статистика
size_t arena_used(Arena* arena);
size_t arena_remaining(Arena* arena);
float  arena_usage_percent(Arena* arena);

#endif // BEVEL_ARENA_H
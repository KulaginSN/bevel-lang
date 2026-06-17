// src/common/arena.c
#include "common/arena.h"
#include "common/string.h"
#include <stdlib.h>   // Для malloc, free
#include <stdio.h>    // Для fprintf, stderr
#include <string.h>   // Для memset, memcpy

Arena* arena_init(size_t capacity) {
    Arena* arena = (Arena*)malloc(sizeof(Arena));
    if (!arena) {
        fprintf(stderr, "[Arena] Failed to allocate arena structure\n");
        return NULL;
    }
    
    arena->buffer = (char*)malloc(capacity);
    if (!arena->buffer) {
        fprintf(stderr, "[Arena] Failed to allocate %zu bytes\n", capacity);
        free(arena);
        return NULL;
    }
    
    arena->capacity = capacity;
    arena->offset = 0;
    return arena;
}

void arena_free(Arena* arena) {
    if (!arena) return;
    if (arena->buffer) {
        free(arena->buffer);
    }
    free(arena);
}

static size_t align_up(size_t offset, size_t align) {
    return (offset + align - 1) & ~(align - 1);
}

void* arena_alloc(Arena* arena, size_t size, size_t align) {
    if (!arena || size == 0) return NULL;
    
    size_t aligned_offset = align_up(arena->offset, align);
    size_t new_offset = aligned_offset + size;
    
    if (new_offset > arena->capacity) {
        fprintf(stderr, "[Arena] Out of memory! Requested %zu bytes, "
                "available %zu bytes (capacity %zu, used %zu)\n",
                size, arena->capacity - arena->offset,
                arena->capacity, arena->offset);
        return NULL;
    }
    
    void* ptr = arena->buffer + aligned_offset;
    arena->offset = new_offset;
    
    // Инициализация нулями для безопасности
    memset(ptr, 0, size);
    return ptr;
}

void* arena_copy(Arena* arena, const void* data, size_t size) {
    void* ptr = arena_alloc(arena, size, 1);
    if (ptr && data) {
        memcpy(ptr, data, size);
    }
    return ptr;
}

String arena_string(Arena* arena, String s) {
    if (!s.data || s.length == 0) return STRING_EMPTY;
    char* data = (char*)arena_copy(arena, s.data, s.length);
    return string_new(data, s.length);
}

const char* arena_cstr(Arena* arena, const char* cstr) {
    if (!cstr) return NULL;
    size_t len = strlen(cstr);
    char* copy = (char*)arena_alloc(arena, len + 1, 1);
    if (copy) {
        memcpy(copy, cstr, len);
        copy[len] = '\0';
    }
    return copy;
}

size_t arena_mark(Arena* arena) {
    return arena ? arena->offset : 0;
}

void arena_release(Arena* arena, size_t mark) {
    if (!arena) return;
    if (mark <= arena->offset) {
        arena->offset = mark;
    }
}

size_t arena_used(Arena* arena) {
    return arena ? arena->offset : 0;
}

size_t arena_remaining(Arena* arena) {
    return arena ? arena->capacity - arena->offset : 0;
}

float arena_usage_percent(Arena* arena) {
    if (!arena || arena->capacity == 0) return 0.0f;
    return (float)arena->offset / (float)arena->capacity * 100.0f;
}
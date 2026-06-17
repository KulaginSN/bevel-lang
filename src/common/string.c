// src/common/string.c
#include "common/string.h"
#include <stdlib.h>
#include <ctype.h>

String string_new(const char* data, size_t length) {
    return (String){data, length};
}

String string_from_cstr(const char* cstr) {
    return (String){cstr, cstr ? strlen(cstr) : 0};
}

bool string_equals(String a, String b) {
    if (a.length != b.length) return false;
    if (a.data == b.data) return true;
    if (!a.data || !b.data) return false;
    return memcmp(a.data, b.data, a.length) == 0;
}

bool string_starts_with(String s, String prefix) {
    if (prefix.length > s.length) return false;
    return memcmp(s.data, prefix.data, prefix.length) == 0;
}

bool string_ends_with(String s, String suffix) {
    if (suffix.length > s.length) return false;
    return memcmp(s.data + s.length - suffix.length, suffix.data, suffix.length) == 0;
}

int string_compare(String a, String b) {
    size_t min_len = a.length < b.length ? a.length : b.length;
    int cmp = memcmp(a.data, b.data, min_len);
    if (cmp != 0) return cmp;
    if (a.length < b.length) return -1;
    if (a.length > b.length) return 1;
    return 0;
}

String string_substr(String s, size_t start, size_t length) {
    if (start >= s.length) return STRING_EMPTY;
    if (start + length > s.length) length = s.length - start;
    return string_new(s.data + start, length);
}

uint32_t string_hash(String s) {
    uint32_t hash = 2166136261u;
    for (size_t i = 0; i < s.length; i++) {
        hash ^= (uint8_t)s.data[i];
        hash *= 16777619u;
    }
    return hash;
}

char* string_to_cstr(String s) {
    char* result = (char*)malloc(s.length + 1);
    if (!result) return NULL;
    if (s.data && s.length > 0) {
        memcpy(result, s.data, s.length);
    }
    result[s.length] = '\0';
    return result;
}

String string_concat(Arena* arena, String a, String b) {
    if (!a.data && !b.data) return STRING_EMPTY;
    if (!a.data || a.length == 0) return b;
    if (!b.data || b.length == 0) return a;
    
    size_t new_length = a.length + b.length;
    char* new_data = ARENA_ARRAY(arena, char, new_length);
    
    memcpy(new_data, a.data, a.length);
    memcpy(new_data + a.length, b.data, b.length);
    
    return string_new(new_data, new_length);
}

String string_concat3(Arena* arena, String a, String b, String c) {
    String ab = string_concat(arena, a, b);
    return string_concat(arena, ab, c);
}
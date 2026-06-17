// src/common/file.c
#include "common/file.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

char* read_file_to_arena(Arena* arena, const char* filepath, size_t* out_size) {
    FILE* file = fopen(filepath, "rb");
    if (!file) {
        fprintf(stderr, "Error: Cannot open file '%s'\n", filepath);
        return NULL;
    }

    // Определяем размер файла
    if (fseek(file, 0, SEEK_END) != 0) {
        fprintf(stderr, "Error: Cannot seek in file '%s'\n", filepath);
        fclose(file);
        return NULL;
    }

    long size = ftell(file);
    if (size < 0) {
        fprintf(stderr, "Error: Cannot determine size of file '%s'\n", filepath);
        fclose(file);
        return NULL;
    }

    rewind(file);

    // Выделяем память в арене (+1 для завершающего '\0'), выравнивание 1
    char* buffer = (char*)arena_alloc(arena, (size_t)size + 1, 1);
    if (!buffer) {
        fprintf(stderr, "Error: Out of memory while reading file '%s'\n", filepath);
        fclose(file);
        return NULL;
    }

    // Читаем файл
    size_t bytes_read = fread(buffer, 1, (size_t)size, file);
    if (bytes_read != (size_t)size) {
        fprintf(stderr, "Error: Failed to read entire file '%s'\n", filepath);
        fclose(file);
        return NULL;
    }

    fclose(file);
    
    // Гарантируем null-терминацию для безопасности
    buffer[size] = '\0';
    
    if (out_size) {
        *out_size = (size_t)size;
    }

    return buffer;
}

const char* get_output_filename(Arena* arena, const char* input_path, const char* new_extension) {
    // Находим последнюю точку в имени файла
    const char* last_dot = strrchr(input_path, '.');
    
    size_t base_len;
    if (last_dot != NULL) {
        base_len = (size_t)(last_dot - input_path);
    } else {
        base_len = strlen(input_path);
    }
    
    // Выделяем память: длина базового имени + '.' + длина расширения + '\0', выравнивание 1
    size_t new_len = base_len + 1 + strlen(new_extension) + 1;
    char* result = (char*)arena_alloc(arena, new_len, 1);
    
    if (!result) return NULL;
    
    // Копируем базовое имя
    memcpy(result, input_path, base_len);
    result[base_len] = '.';
    memcpy(result + base_len + 1, new_extension, strlen(new_extension));
    result[new_len - 1] = '\0';
    
    return result;
}
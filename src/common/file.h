// src/common/file.h
#ifndef BEVEL_FILE_H
#define BEVEL_FILE_H

#include "common/arena.h"
#include <stddef.h>

/**
 * Читает весь файл в память, выделенную через Arena.
 * 
 * @param arena Арена для выделения памяти.
 * @param filepath Путь к файлу.
 * @param out_size Указатель на переменную, куда будет записан размер файла в байтах.
 * @return Указатель на содержимое файла (с завершающим '\0') или NULL при ошибке.
 */
char* read_file_to_arena(Arena* arena, const char* filepath, size_t* out_size);

/**
 * Генерирует имя выходного файла на основе входного.
 * Например: "main.bv" + "c" -> "main.c"
 * 
 * @param arena Арена для выделения памяти под строку.
 * @param input_path Путь к входному файлу.
 * @param new_extension Новое расширение без точки (например, "c", "ll", "s").
 * @return Указатель на новую строку с именем файла.
 */
const char* get_output_filename(Arena* arena, const char* input_path, const char* new_extension);

#endif // BEVEL_FILE_H
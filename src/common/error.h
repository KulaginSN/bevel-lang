// common/error.h
#ifndef BEVEL_ERROR_H
#define BEVEL_ERROR_H

#include <stdbool.h>
#include "string.h"
#include "arena.h"

typedef enum {
    ERROR_LEXER,
    ERROR_PARSER,
    ERROR_SEMANTIC,
    ERROR_TYPE,
    ERROR_LINKER,
    ERROR_INTERNAL
} ErrorKind;

typedef struct {
    ErrorKind   kind;
    int         line;
    int         column;
    int         length;        // Длина "виновного" участка
    const char* filename;
    const char* message;
    const char* hint;          // Может быть NULL
} Error;

typedef struct {
    Error*      errors;
    int         count;
    int         capacity;
    Arena*      arena;
    const char* source;        // Исходный код для контекста
    const char* filename;
} ErrorReporter;

// Инициализация и очистка
void error_reporter_init(ErrorReporter* reporter, const char* source,
                         const char* filename, Arena* arena);
void error_reporter_free(ErrorReporter* reporter);

// Добавление ошибок
void error_reporter_add(ErrorReporter* reporter, ErrorKind kind,
                        int line, int column, int length,
                        const char* message, const char* hint);

// Удобные обёртки
void error_reporter_error(ErrorReporter* r, int line, int col, int len, const char* msg);
void error_reporter_error_with_hint(ErrorReporter* r, int line, int col, int len,
                                     const char* msg, const char* hint);
void error_reporter_parser_error(ErrorReporter* r, int line, int col, const char* msg);
void error_reporter_semantic_error(ErrorReporter* r, int line, int col, const char* msg);

// Вывод
void error_reporter_print(ErrorReporter* reporter, bool with_colors);

// Статус
bool error_reporter_has_errors(ErrorReporter* reporter);
int  error_reporter_count(ErrorReporter* reporter);

#endif // BEVEL_ERROR_H
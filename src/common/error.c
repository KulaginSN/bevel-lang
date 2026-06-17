// common/error.c
#include "common/error.h"
#include "common/utils.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define INITIAL_CAPACITY 16

#define C_RESET   "\033[0m"
#define C_RED     "\033[31m"
#define C_YELLOW  "\033[33m"
#define C_BLUE    "\033[34m"
#define C_GRAY    "\033[90m"
#define C_BOLD    "\033[1m"

void error_reporter_init(ErrorReporter* r, const char* source,
                         const char* filename, Arena* arena) {
    r->errors   = NULL;
    r->count    = 0;
    r->capacity = 0;
    r->arena    = arena;
    r->source   = source;
    r->filename = filename;
}

void error_reporter_free(ErrorReporter* r) {
    if (r->errors) {
        free(r->errors);
        r->errors = NULL;
    }
    r->count = r->capacity = 0;
}

static void ensure_capacity(ErrorReporter* r) {
    if (r->count < r->capacity) return;
    
    int new_cap = r->capacity == 0 ? INITIAL_CAPACITY : r->capacity * 2;
    Error* new_arr = (Error*)realloc(r->errors, new_cap * sizeof(Error));
    if (!new_arr) {
        fprintf(stderr, "[ErrorReporter] Out of memory\n");
        return;
    }
    r->errors   = new_arr;
    r->capacity = new_cap;
}

void error_reporter_add(ErrorReporter* r, ErrorKind kind,
                        int line, int column, int length,
                        const char* message, const char* hint) {
    ensure_capacity(r);
    if (r->count >= r->capacity) return;
    
    // Копируем строки в арену для долгого хранения
    const char* msg  = r->arena ? arena_cstr(r->arena, message) : message;
    const char* h    = (hint && r->arena) ? arena_cstr(r->arena, hint) : hint;
    const char* file = r->filename;
    
    Error e = {
        .kind     = kind,
        .line     = line,
        .column   = column > 0 ? column : 1,
        .length   = length > 0 ? length : 1,
        .filename = file,
        .message  = msg,
        .hint     = h
    };
    r->errors[r->count++] = e;
}

void error_reporter_error(ErrorReporter* r, int line, int col, int len, const char* msg) {
    error_reporter_add(r, ERROR_LEXER, line, col, len, msg, NULL);
}

void error_reporter_error_with_hint(ErrorReporter* r, int line, int col, int len,
                                     const char* msg, const char* hint) {
    error_reporter_add(r, ERROR_LEXER, line, col, len, msg, hint);
}

void error_reporter_parser_error(ErrorReporter* r, int line, int col, const char* msg) {
    error_reporter_add(r, ERROR_PARSER, line, col, 1, msg, NULL);
}

void error_reporter_semantic_error(ErrorReporter* r, int line, int col, const char* msg) {
    error_reporter_add(r, ERROR_SEMANTIC, line, col, 1, msg, NULL);
}

static const char* kind_to_str(ErrorKind k) {
    switch (k) {
        case ERROR_LEXER:    return "lexer";
        case ERROR_PARSER:   return "parser";
        case ERROR_SEMANTIC: return "semantic";
        case ERROR_TYPE:     return "type";
        case ERROR_LINKER:   return "linker";
        case ERROR_INTERNAL: return "internal";
    }
    return "unknown";
}

static const char* find_line_start(const char* source, int line) {
    if (!source || line < 1) return NULL;
    const char* p = source;
    int cur = 1;
    while (cur < line && *p) {
        if (*p == '\n') cur++;
        p++;
    }
    return cur == line ? p : NULL;
}

static size_t line_length(const char* start) {
    if (!start) return 0;
    const char* p = start;
    while (*p && *p != '\n' && *p != '\r') p++;
    return p - start;
}

void error_reporter_print(ErrorReporter* r, bool with_colors) {
    if (r->count == 0) return;
    
    const char* red    = with_colors ? C_RED    : "";
    const char* yellow = with_colors ? C_YELLOW : "";
    const char* blue   = with_colors ? C_BLUE   : "";
    const char* gray   = with_colors ? C_GRAY   : "";
    const char* bold   = with_colors ? C_BOLD   : "";
    const char* reset  = with_colors ? C_RESET  : "";
    
    for (int i = 0; i < r->count; i++) {
        Error* e = &r->errors[i];
        
        // Заголовок ошибки
        printf("%s%serror[%s%s]%s: %s%s%s\n", bold, red, yellow, kind_to_str(e->kind), reset, bold, e->message, reset);
        
        // Локация
        printf("%s --> %s:%d:%d%s\n",
               blue, e->filename ? e->filename : "<unknown>",
               e->line, e->column, reset);
        
        // Контекст из исходника
        if (r->source) {
            const char* line_start = find_line_start(r->source, e->line);
            if (line_start) {
                size_t len = line_length(line_start);
                
                char num_buf[16];
                snprintf(num_buf, sizeof(num_buf), "%d", e->line);
                int num_w = (int)strlen(num_buf);
                
                printf("%s%*s |%s\n", gray, num_w, "", reset);
                printf("%s%*d |%s %.*s\n", blue, num_w, e->line, reset, (int)len, line_start);
                printf("%s%*s |%s ", gray, num_w, "", reset);
                
                // Пробелы до места ошибки
                for (int j = 1; j < e->column; j++) printf(" ");
                
                // Подчёркивание
                printf("%s", red);
                int max_carets = (int)len - (e->column - 1);
                if (max_carets < 0) max_carets = 0;
                int carets = e->length < max_carets ? e->length : max_carets;
                if (carets < 1) carets = 1;
                for (int j = 0; j < carets; j++) printf("^");
                printf("%s\n", reset);
            }
        }
        
        // Подсказка
        if (e->hint) {
            printf("%s   = hint: %s%s\n", gray, e->hint, reset);
        }
        printf("\n");
    }
    
    if (r->count > 1) {
        printf("%s%serror%s: aborting due to %d errors\n",
               bold, red, reset, r->count);
    }
}

bool error_reporter_has_errors(ErrorReporter* r) {
    return r->count > 0;
}

int error_reporter_count(ErrorReporter* r) {
    return r->count;
}
// src/lexer/lexer.h
#ifndef BEVEL_LEXER_H
#define BEVEL_LEXER_H

#include "lexer/token.h"
#include "common/arena.h"
#include "common/error.h"

// Возвращает текущий уровень отступа (глубину стека INDENT)

typedef struct {
    const char* source;
    const char* start;
    const char* current;
    int line;
    int column;
    Arena* arena;
    ErrorReporter* errors;
    int indent_stack[32];
    int indent_depth;
    bool at_line_start;
    int pending_dedents;
} Lexer;

void lexer_init(Lexer* lexer, const char* source, Arena* arena, ErrorReporter* errors);
Token lexer_next_token(Lexer* lexer);
Token lexer_peek_token(Lexer* lexer); 
int lexer_indent_level(Lexer* l);

#endif // BEVEL_LEXER_H
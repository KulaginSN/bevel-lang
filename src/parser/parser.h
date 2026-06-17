// src/parser/parser.h
#ifndef BEVEL_PARSER_H
#define BEVEL_PARSER_H

#include "parser/ast.h"
#include "lexer/lexer.h"
#include "common/arena.h"
#include "common/error.h"

typedef struct {
    Lexer lexer;
    Token current;
    Token previous;
    Arena* arena;
    ErrorReporter* errors;
    bool had_error;
} Parser;

// Инициализация парсера
void parser_init(Parser* p, const char* source, Arena* arena, ErrorReporter* errors);

// Главный метод парсинга (возвращает корень AST или NULL при ошибке)
ASTNode* parser_parse(Parser* p);

// Отладочная печать AST (простая версия)
void ast_print(ASTNode* node, int depth);

#endif // BEVEL_PARSER_H
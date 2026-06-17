// src/semantic/analyzer.h
#ifndef BEVEL_ANALYZER_H
#define BEVEL_ANALYZER_H

#include "semantic/types.h"
#include "semantic/symbols.h"
#include "semantic/generics.h"
#include "parser/ast.h"
#include "common/arena.h"
#include "common/error.h"


typedef struct Analyzer {
    Arena* arena;
    SymbolTable symbols;
    ErrorReporter* errors;
    int error_count;
    
    // Контекст анализа
    Type* current_return_type;
    Type* current_struct_type;
    int in_loop_depth;
    
    // НОВОЕ: Контекст generic параметров
    struct GenericParamNode* current_generic_params;
    int current_generic_param_count;
    
    // НОВОЕ: Контекст where clauses
    struct WhereClauseNode* current_where_clauses;
    int current_where_clause_count;
} Analyzer;

// Инициализация анализатора
void analyzer_init(Analyzer* a, Arena* arena, ErrorReporter* errors);

// Главный метод: анализирует всё AST
// Возвращает true, если ошибок нет
bool analyzer_analyze(Analyzer* a, ASTNode* ast);

// Получить количество найденных ошибок
int analyzer_error_count(Analyzer* a);

#endif // BEVEL_ANALYZER_H
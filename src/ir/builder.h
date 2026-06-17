// src/ir/builder.h
#ifndef BEVEL_IR_BUILDER_H
#define BEVEL_IR_BUILDER_H

#include "ir/ir.h"
#include "parser/ast.h"
#include "semantic/analyzer.h"
#include "common/error.h"

// Простая таблица символов для IR builder
typedef struct IRScope {
    struct {
        String name;
        IRValue* value;
    } symbols[256];
    int symbol_count;
    struct IRScope* parent;
} IRScope;

// Контекст текущего цикла для break/continue
typedef struct {
    IRBlock* continue_block;  // Куда прыгать на continue (к условию)
    IRBlock* break_block;     // Куда прыгать на break (после цикла)
} LoopContext;

typedef struct {
    IRModule* module;
    IRFunction* current_function;
    IRBlock* current_block;
    Analyzer* analyzer;
    Arena* arena;
    ErrorReporter* errors;
    int error_count;
    
    IRScope* current_scope;
    
    // Стек активных циклов (для break/continue)
    LoopContext loop_stack[32];
    int loop_depth;
} IRBuilder;

// Инициализация builder
void ir_builder_init(IRBuilder* builder, Arena* arena, ErrorReporter* errors, Analyzer* analyzer);

// Построение IR из AST
IRModule* ir_builder_build(IRBuilder* builder, ASTNode* ast);

// Получить количество ошибок
int ir_builder_error_count(IRBuilder* builder);

#endif // BEVEL_IR_BUILDER_H
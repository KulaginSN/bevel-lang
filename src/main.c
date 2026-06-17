// src/main.c
#include "common/arena.h"
#include "common/error.h"
#include "common/file.h"          // <-- НОВОЕ
#include "lexer/lexer.h"
#include "parser/parser.h"
#include "parser/printer.h"
#include "semantic/analyzer.h"
#include "ir/builder.h"
#include "ir/printer.h"
#include "optimizer/optimizer.h"
#include "codegen/codegen.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void print_usage(const char* prog) {
    printf("Usage: %s [options] <input.bv>\n", prog);
    printf("Options:\n");
    printf("  --tokens       Print token stream (debug)\n");
    printf("  --ast          Print AST (debug)\n");
    printf("  --ir           Print IR output (default: on)\n");
    printf("  --no-ir        Disable IR output\n");
    printf("  --target=c     Generate C code (default)\n");
    printf("  --target=llvm  Generate LLVM IR\n");
    printf("  --target=x86   Generate x86 Assembly\n");
    printf("  --help         Show this help\n");
}

int main(int argc, char* argv[]) {
    bool show_tokens = false;
    bool show_ast = false;
    bool show_ir = true;
    CodegenTarget target = TARGET_C;
    const char* input_file = NULL;
    
    // 1. Разбор аргументов командной строки
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--tokens") == 0) show_tokens = true;
        else if (strcmp(argv[i], "--ast") == 0) show_ast = true;
        else if (strcmp(argv[i], "--ir") == 0) show_ir = true;
        else if (strcmp(argv[i], "--no-ir") == 0) show_ir = false;
        else if (strcmp(argv[i], "--target=c") == 0) target = TARGET_C;
        else if (strcmp(argv[i], "--target=llvm") == 0) target = TARGET_LLVM;
        else if (strcmp(argv[i], "--target=x86") == 0) target = TARGET_X86;
        else if (strcmp(argv[i], "--help") == 0) {
            print_usage(argv[0]);
            return 0;
        } else if (argv[i][0] != '-') {
            // Первый аргумент без '-' считаем входным файлом
            input_file = argv[i];
        } else {
            fprintf(stderr, "Unknown option: %s\n", argv[i]);
            print_usage(argv[0]);
            return 1;
        }
    }
    
    // 2. Проверка наличия входного файла
    if (!input_file) {
        fprintf(stderr, "Error: No input file specified.\n");
        print_usage(argv[0]);
        return 1;
    }

    // 3. Инициализация арены
    Arena* arena = arena_init(1024 * 1024);
    if (!arena) {
        fprintf(stderr, "Failed to allocate arena\n");
        return 1;
    }

    // 4. Чтение файла
    size_t source_size = 0;
    char* source_code = read_file_to_arena(arena, input_file, &source_size);
    if (!source_code) {
        arena_free(arena);
        return 1;
    }

    // 5. Определение имени выходного файла
    const char* ext = (target == TARGET_LLVM) ? "ll" : 
                      (target == TARGET_X86) ? "s" : "c";
    const char* output_file = get_output_filename(arena, input_file, ext);
    if (!output_file) {
        fprintf(stderr, "Failed to generate output filename\n");
        arena_free(arena);
        return 1;
    }

    ErrorReporter reporter;
    error_reporter_init(&reporter, source_code, input_file, arena);

    // ===== ОТЛАДКА: Вывод всех токенов (опционально) =====
    if (show_tokens) {
        printf("\n=== Token Stream (Debug) ===\n");
        Lexer debug_lexer;
        lexer_init(&debug_lexer, source_code, arena, &reporter);
        Token token;
        int count = 0;
        do {
            token = lexer_next_token(&debug_lexer);
            const char* type_name = token_type_name(token.type);
            printf("[%02d:%02d] %-15s", token.line, token.column, type_name);
            
            if (token.lexeme.length > 0 && token.lexeme.data) {
                printf(" | %.*s", (int)token.lexeme.length, token.lexeme.data);
            }
            printf("\n");
            count++;
        } while (token.type != TOKEN_EOF && token.type != TOKEN_ERROR && count < 500);
        printf("Total tokens: %d\n\n", count);
    }

    // ===== ЭТАП 1: Лексический и синтаксический анализ =====
    printf("=== Parsing Source Code ===\n");
    Parser parser;
    parser_init(&parser, source_code, arena, &reporter);
    ASTNode* ast = parser_parse(&parser);

    if (error_reporter_has_errors(&reporter)) {
        printf("\n=== Syntax Errors ===\n");
        error_reporter_print(&reporter, true);
        error_reporter_free(&reporter);
        arena_free(arena);
        return 1;
    }
    printf("✅ Parsing finished successfully!\n");

    // ===== ЭТАП 2: Печать AST (опционально) =====
    if (show_ast) {
        printf("\n=== Abstract Syntax Tree ===\n");
        ast_print_colored(ast, 0);
    }

    // ===== ЭТАП 3: Семантический анализ =====
    printf("\n=== Semantic Analysis ===\n");
    Analyzer analyzer;
    analyzer_init(&analyzer, arena, &reporter);
    bool semantic_ok = analyzer_analyze(&analyzer, ast);

    if (!semantic_ok) {
        printf("\n=== Semantic Errors ===\n");
        error_reporter_print(&reporter, true);
        printf("\n❌ Compilation failed with %d semantic error(s)\n", 
               analyzer_error_count(&analyzer));
        error_reporter_free(&reporter);
        arena_free(arena);
        return 1;
    }
    printf("✅ Semantic analysis passed!\n");

    // ===== ЭТАП 4: Построение IR =====
    printf("\n=== Building IR ===\n");
    IRBuilder ir_builder;
    ir_builder_init(&ir_builder, arena, &reporter, &analyzer);
    IRModule* ir_module = ir_builder_build(&ir_builder, ast);

    if (!ir_module || ir_builder_error_count(&ir_builder) > 0) {
        printf("\n=== IR Builder Errors ===\n");
        error_reporter_print(&reporter, true);
        printf("\n❌ IR building failed with %d error(s)\n", 
               ir_builder_error_count(&ir_builder));
        error_reporter_free(&reporter);
        arena_free(arena);
        return 1;
    }
    printf("✅ IR built successfully!\n");

    // ===== ЭТАП 5: Оптимизации =====
    printf("\n=== Running Optimizations ===\n");
    Optimizer optimizer;
    optimizer_init(&optimizer, arena);
    optimizer_optimize(&optimizer, ir_module);

    // ===== ЭТАП 6: Печать IR (оптимизированного) =====
    if (show_ir) {
        printf("\n=== IR Output (after optimizations) ===\n");
        ir_printer_print_module_stdout(ir_module);
        ir_printer_print_stats(ir_module);
    }

    // ===== ЭТАП 7: Генерация кода =====
    printf("\n=== Code Generation ===\n");
    
    FILE* output = fopen(output_file, "w");
    if (!output) {
        fprintf(stderr, "Failed to create %s\n", output_file);
        error_reporter_free(&reporter);
        arena_free(arena);
        return 1;
    }

    CodeGenerator codegen;
    codegen_init(&codegen, target, output, arena);
    codegen_generate(&codegen, ir_module);
    fclose(output);

    const char* target_name = (target == TARGET_C) ? "C" : 
                              (target == TARGET_LLVM) ? "LLVM IR" : "x86 Assembly";
    printf("✅ %s code generated: %s\n", target_name, output_file);

    // ===== Статистика =====
    printf("\n=== Compilation Summary ===\n");
    printf("  Input file: %s (%zu bytes)\n", input_file, source_size);
    printf("  Arena usage: %zu / %zu bytes (%.1f%%)\n",
           arena_used(arena), arena->capacity, arena_usage_percent(arena));
    printf("  Semantic errors: %d\n", analyzer_error_count(&analyzer));
    printf("  IR optimizations applied: %d\n", optimizer_get_stats(&optimizer));
    printf("  Status: SUCCESS\n");

    error_reporter_free(&reporter);
    arena_free(arena);
    return 0;
}
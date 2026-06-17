// src/codegen/codegen.h
#ifndef BEVEL_CODEGEN_H
#define BEVEL_CODEGEN_H

#include "ir/ir.h"
#include "common/arena.h"
#include <stdio.h>

typedef enum {
    TARGET_C,
    TARGET_LLVM,
    TARGET_X86
} CodegenTarget;

typedef struct {
    CodegenTarget target;
    FILE* out;
    Arena* arena;
} CodeGenerator;

// Инициализация генератора
void codegen_init(CodeGenerator* cg, CodegenTarget target, FILE* out, Arena* arena);

// Главная функция генерации (теперь принимает IRModule, а не AST!)
void codegen_generate(CodeGenerator* cg, IRModule* module);

#endif // BEVEL_CODEGEN_H
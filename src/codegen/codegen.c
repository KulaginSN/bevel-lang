// src/codegen/codegen.c
#include "codegen/codegen.h"
#include "codegen/c_backend.h"
#include "codegen/llvm_backend.h"
#include "codegen/x86_backend.h"
#include <stdio.h>

void codegen_init(CodeGenerator* cg, CodegenTarget target, FILE* out, Arena* arena) {
    cg->target = target;
    cg->out = out;
    cg->arena = arena;
}

void codegen_generate(CodeGenerator* cg, IRModule* module) {
    switch (cg->target) {
        case TARGET_C:
            c_backend_generate(cg->out, module, cg->arena);
            break;
        case TARGET_LLVM:
            llvm_backend_generate(cg->out, module, cg->arena);
            break;
        case TARGET_X86:
            x86_backend_generate(cg->out, module, cg->arena);
            break;
        default:
            fprintf(cg->out, "// Error: Unknown target\n");
            break;
    }
}
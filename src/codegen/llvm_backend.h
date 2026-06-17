// src/codegen/llvm_backend.h
#ifndef BEVEL_LLVM_BACKEND_H
#define BEVEL_LLVM_BACKEND_H

#include "ir/ir.h"
#include "common/arena.h"
#include <stdio.h>

void llvm_backend_generate(FILE* out, IRModule* module, Arena* arena);

#endif // BEVEL_LLVM_BACKEND_H
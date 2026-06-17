// src/codegen/x86_backend.h
#ifndef BEVEL_X86_BACKEND_H
#define BEVEL_X86_BACKEND_H

#include "ir/ir.h"
#include "common/arena.h"
#include <stdio.h>

void x86_backend_generate(FILE* out, IRModule* module, Arena* arena);

#endif // BEVEL_X86_BACKEND_H
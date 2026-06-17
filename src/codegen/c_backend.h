// src/codegen/c_backend.h
#ifndef BEVEL_C_BACKEND_H
#define BEVEL_C_BACKEND_H

#include "ir/ir.h"
#include "common/arena.h"
#include <stdio.h>

void c_backend_generate(FILE* out, IRModule* module, Arena* arena);

#endif // BEVEL_C_BACKEND_H
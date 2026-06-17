// src/optimizer/inline.h
#ifndef BEVEL_INLINE_H
#define BEVEL_INLINE_H

#include "ir/ir.h"
#include "optimizer/optimizer.h"

// Function Inlining: встраивает небольшие функции прямо в место вызова
void inline_optimize(Optimizer* opt, IRModule* module);

#endif // BEVEL_INLINE_H
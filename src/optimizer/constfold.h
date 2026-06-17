// src/optimizer/constfold.h
#ifndef BEVEL_CONSTFOLD_H
#define BEVEL_CONSTFOLD_H

#include "ir/ir.h"
#include "optimizer/optimizer.h"

// Constant Folding: вычисляет константные выражения на этапе компиляции
// Пример: %t0 = add 10, 20  →  %t0 = 30
void constfold_optimize(Optimizer* opt, IRFunction* func);

#endif // BEVEL_CONSTFOLD_H
// src/optimizer/dce.h
#ifndef BEVEL_DCE_H
#define BEVEL_DCE_H

#include "ir/ir.h"
#include "optimizer/optimizer.h"

// Dead Code Elimination: удаляет инструкции, результат которых нигде не используется
// и которые не имеют побочных эффектов.
void dce_optimize(Optimizer* opt, IRFunction* func);

#endif // BEVEL_DCE_H
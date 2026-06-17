// src/optimizer/optimizer.h
#ifndef BEVEL_OPTIMIZER_H
#define BEVEL_OPTIMIZER_H

#include "ir/ir.h"
#include "common/arena.h"
#include "optimizer/inline.h"

typedef struct {
    IRModule* module;
    Arena* arena;
    int optimizations_applied;
} Optimizer;

// Инициализация оптимизатора
void optimizer_init(Optimizer* opt, Arena* arena);

// Применить все оптимизации к модулю
void optimizer_optimize(Optimizer* opt, IRModule* module);

// Получить количество применённых оптимизаций
int optimizer_get_stats(Optimizer* opt);

// Индивидуальные оптимизации
void optimizer_constant_folding(Optimizer* opt, IRFunction* func);
void optimizer_dead_code_elimination(Optimizer* opt, IRFunction* func);
void optimizer_function_inlining(Optimizer* opt, IRModule* module);

#endif // BEVEL_OPTIMIZER_H
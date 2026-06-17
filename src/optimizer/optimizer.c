// src/optimizer/optimizer.c
#include "optimizer/optimizer.h"
#include "optimizer/constfold.h"
#include "optimizer/dce.h"
#include "optimizer/inline.h"
#include <stdio.h>

void optimizer_init(Optimizer* opt, Arena* arena) {
    opt->module = NULL;
    opt->arena = arena;
    opt->optimizations_applied = 0;
}

void optimizer_optimize(Optimizer* opt, IRModule* module) {
    opt->module = module;
    
    printf("\n=== Running Optimizations ===\n");
    
    // Применяем оптимизации к каждой функции
    for (IRFunction* func = module->functions; func; func = func->next) {
        int before = opt->optimizations_applied;
        
        // 1. Constant Folding
        optimizer_constant_folding(opt, func);
        
        // 2. Dead Code Elimination
        optimizer_dead_code_elimination(opt, func);
        
        int applied = opt->optimizations_applied - before;
        if (applied > 0) {
            printf("  Function '%.*s': %d optimizations applied\n",
                   (int)func->name.length, func->name.data, applied);
        }
    }
    
    // 3. Function Inlining (работает на уровне модуля)
    optimizer_function_inlining(opt, module);
    
    printf("✅ Total optimizations applied: %d\n", opt->optimizations_applied);
}

int optimizer_get_stats(Optimizer* opt) {
    return opt->optimizations_applied;
}
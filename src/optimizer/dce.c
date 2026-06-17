// src/optimizer/dce.c
#include "optimizer/dce.h"
#include <stdbool.h>

// Проверяет, имеет ли инструкция побочные эффекты (её нельзя удалять)
// src/optimizer/dce.c

// Проверяет, имеет ли инструкция побочные эффекты (её нельзя удалять)
static bool has_side_effects(IRInstruction* instr) {
    IROpcode op = instr->opcode;
    
    // Инструкции с побочными эффектами
    if (op == IR_OP_CALL ||
        op == IR_OP_RETURN ||
        op == IR_OP_STORE ||
        op == IR_OP_FIELD_STORE ||    // <-- НОВОЕ: запись в поле структуры
        op == IR_OP_INDEX_STORE ||    // <-- НОВОЕ: запись в массив/срез
        op == IR_OP_BR ||
        op == IR_OP_BR_COND ||
        op == IR_OP_PHI ||
        op == IR_OP_VAR_DECL) {
        return true;
    }
    
    // Присваивание в переменную (не temp) - это side effect
    if (op == IR_OP_ASSIGN && instr->result) {
        if (instr->result->kind == IR_VALUE_VAR || 
            instr->result->kind == IR_VALUE_GLOBAL) {
            return true;
        }
    }
    
    return false;
}

// Проверяет, является ли значение временной переменной (%tX)
static bool is_temp(IRValue* val) {
    return val && val->kind == IR_VALUE_TEMP;
}

void dce_optimize(Optimizer* opt, IRFunction* func) {
    if (!func) return;

    // =========================================================
    // ЭТАП 1: Сброс флагов и начальная разметка
    // =========================================================
    for (IRBlock* block = func->blocks; block; block = block->next) {
        for (IRInstruction* instr = block->instructions; instr; instr = instr->next) {
            instr->is_used = false;
            
            // Инструкции с побочными эффектами всегда считаются "используемыми"
            if (has_side_effects(instr)) {
                instr->is_used = true;
            }
        }
    }

    // =========================================================
    // ЭТАП 2: Распространение "используемости" (итеративно)
    // =========================================================
    bool changed = true;
    while (changed) {
        changed = false;
        
        for (IRBlock* block = func->blocks; block; block = block->next) {
            for (IRInstruction* instr = block->instructions; instr; instr = instr->next) {
                // Если инструкция ещё не помечена, но имеет результат (временную переменную)
                if (!instr->is_used && instr->result != NULL && is_temp(instr->result)) {
                    
                    bool is_result_used = false;
                    
                    // Ищем, используется ли instr->result в любой другой инструкции
                    for (IRBlock* b2 = func->blocks; b2 && !is_result_used; b2 = b2->next) {
                        for (IRInstruction* i2 = b2->instructions; i2 && !is_result_used; i2 = i2->next) {
                            if (i2->operand1 == instr->result ||
                                i2->operand2 == instr->result ||
                                i2->operand3 == instr->result) {   // <-- НОВОЕ: проверка operand3
                                is_result_used = true;
                            } else {
                                for (int a = 0; a < i2->arg_count; a++) {
                                    if (i2->args[a] == instr->result) {
                                        is_result_used = true;
                                        break;
                                    }
                                }
                            }
                        }
                    }

                    // Если результат где-то используется, помечаем инструкцию как используемую
                    if (is_result_used) {
                        instr->is_used = true;
                        changed = true; // Запускаем ещё одну итерацию, так как могли "оживить" новую инструкцию
                    }
                }
            }
        }
    }

    // =========================================================
    // ЭТАП 3: Очистка (Sweep) - удаление мёртвого кода
    // =========================================================
    for (IRBlock* block = func->blocks; block; block = block->next) {
        IRInstruction** curr = &block->instructions;
        
        while (*curr != NULL) {
            IRInstruction* instr = *curr;
            
            // Удаляем, если: не используется, не имеет побочных эффектов, и что-то вычисляет
            if (!instr->is_used && !has_side_effects(instr) && instr->result != NULL) {
                // Исключаем инструкцию из связного списка
                *curr = instr->next;
                block->instruction_count--;
                opt->optimizations_applied++;
                
                // Не двигаем curr, так как следующий элемент теперь находится по адресу *curr
            } else {
                curr = &instr->next;
            }
        }
        
        // Обновляем указатель на последнюю инструкцию в блоке
        IRInstruction* last = block->instructions;
        while (last && last->next) {
            last = last->next;
        }
        block->last_instruction = last;
    }
}

// Обёртка для соответствия интерфейсу optimizer.h
void optimizer_dead_code_elimination(Optimizer* opt, IRFunction* func) {
    dce_optimize(opt, func);
}
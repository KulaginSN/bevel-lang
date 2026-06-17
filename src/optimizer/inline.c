// src/optimizer/inline.c
#include "optimizer/inline.h"
#include "common/utils.h"
#include <string.h>
#include <stdbool.h>

// ===== Вспомогательные функции =====

// Поиск функции по имени в модуле
static IRFunction* find_function_by_name(IRModule* module, String name) {
    for (IRFunction* f = module->functions; f; f = f->next) {
        if (string_equals(f->name, name)) {
            return f;
        }
    }
    return NULL;
}

// Проверяет, можно ли безопасно встроить функцию
static bool is_inlinable(IRFunction* func) {
    if (!func || !func->blocks) return false;
    
    // 1. Должен быть ровно один базовый блок
    if (func->blocks->next != NULL) return false;
    
    // 2. Последняя инструкция должна быть RETURN
    IRInstruction* last = func->blocks->last_instruction;
    if (!last || last->opcode != IR_OP_RETURN) return false;
    
    // 3. Внутри не должно быть других вызовов функций (упрощение)
    for (IRInstruction* instr = func->blocks->instructions; instr; instr = instr->next) {
        if (instr->opcode == IR_OP_CALL) return false;
    }
    
    return true;
}

// Структура для сопоставления старых и новых значений при клонировании
typedef struct {
    IRValue* old_val;
    IRValue* new_val;
} ValueMap;

// Подстановка значения: если это параметр или старая временная переменная, заменяем на новую
static IRValue* substitute_value(Arena* arena, IRValue* val, 
                                 IRFunction* callee, 
                                 ValueMap* param_map, int param_count,
                                 ValueMap* temp_map, int temp_count,
                                 IRValue* call_result) {

    (void)arena;
    (void)callee;
    (void)call_result;

    if (!val) return NULL;
    
    // 1. Если это параметр функции, подставляем переданный аргумент
    for (int i = 0; i < param_count; i++) {
        if (param_map[i].old_val == val) {
            return param_map[i].new_val;
        }
    }
    
    // 2. Если это временная переменная из встраиваемой функции, подставляем новую
    for (int i = 0; i < temp_count; i++) {
        if (temp_map[i].old_val == val) {
            return temp_map[i].new_val;
        }
    }
    
    // 3. Если это результат RETURN встраиваемой функции, и у вызова есть результат
    // (Это упрощение: мы считаем, что operand1 инструкции RETURN - это то, что нужно вернуть)
    // В более сложной версии мы бы отслеживали это явно.
    
    // 4. Иначе возвращаем как есть (константы, глобальные переменные и т.д.)
    return val;
}

// ===== Основная логика Inlining =====

void inline_optimize(Optimizer* opt, IRModule* module) {
    bool changed = true;
    
    // Используем цикл while, так как встраивание функции A в B может сделать 
    // функцию B подходящей для встраивания в C (или наоборот).
    while (changed) {
        changed = false;
        
        for (IRFunction* caller = module->functions; caller; caller = caller->next) {
            for (IRBlock* block = caller->blocks; block; block = block->next) {
                // Используем указатель на указатель для безопасного удаления/замены в связном списке
                IRInstruction** p_curr = &block->instructions;
                
                while (*p_curr != NULL) {
                    IRInstruction* call_instr = *p_curr;
                    
                    // Ищем инструкции вызова функций
                    if (call_instr->opcode == IR_OP_CALL) {
                        IRValue* callee_val = call_instr->operand1;
                        
                        // Проверяем, что callee - это глобальная функция (а не указатель на функцию)
                        if (callee_val && callee_val->kind == IR_VALUE_GLOBAL) {
                            IRFunction* callee = find_function_by_name(module, callee_val->name);
                            
                            if (callee && is_inlinable(callee)) {
                                // === НАЧИНАЕМ ВСТРАИВАНИЕ ===
                                
                                int param_count = callee->param_count;
                                int arg_count = call_instr->arg_count;
                                
                                // 1. Создаём карту сопоставления параметров -> аргументов
                                ValueMap* param_map = ARENA_ARRAY(opt->arena, ValueMap, param_count);
                                for (int i = 0; i < param_count; i++) {
                                    param_map[i].old_val = callee->params[i];
                                    param_map[i].new_val = (i < arg_count) ? call_instr->args[i] : NULL;
                                }
                                
                                // 2. Подготавливаем карту для временных переменных
                                // Мы будем создавать новые temps для каждой клонируемой инструкции с результатом
                                ValueMap* temp_map = ARENA_ARRAY(opt->arena, ValueMap, 64);
                                int temp_count = 0;
                                
                                // 3. Клонируем инструкции callee (кроме последней RETURN)
                                IRInstruction* new_head = NULL;
                                IRInstruction* new_tail = NULL;
                                IRValue* return_val = NULL;
                                
                                for (IRInstruction* instr = callee->blocks->instructions; instr; instr = instr->next) {
                                    if (instr->opcode == IR_OP_RETURN) {
                                        return_val = instr->operand1; // Значение, которое возвращается
                                        continue; // Не клонируем саму инструкцию return
                                    }
                                    
                                    // Создаём новую инструкцию
                                    IRInstruction* clone = ir_instruction_new(opt->arena, instr->opcode);
                                    
                                    // Если у инструкции есть результат, создаём для него новую временную переменную
                                    if (instr->result) {
                                        clone->result = ir_value_temp(opt->arena, caller->next_temp_id++, instr->result->type);
                                        
                                        // Добавляем в карту сопоставления
                                        if (temp_count < 64) {
                                            temp_map[temp_count].old_val = instr->result;
                                            temp_map[temp_count].new_val = clone->result;
                                            temp_count++;
                                        }
                                    }
                                    
                                    // Подставляем операнды
                                    clone->operand1 = substitute_value(opt->arena, instr->operand1, callee, param_map, param_count, temp_map, temp_count, call_instr->result);
                                    clone->operand2 = substitute_value(opt->arena, instr->operand2, callee, param_map, param_count, temp_map, temp_count, call_instr->result);
                                    
                                    // НОВОЕ: Копируем operand3 для FIELD_STORE и INDEX_STORE
                                    if (instr->operand3) {
                                        clone->operand3 = substitute_value(opt->arena, instr->operand3, callee, param_map, param_count, temp_map, temp_count, call_instr->result);
                                    }
                                    
                                    // Добавляем в новый список инструкций
                                    if (!new_head) new_head = clone;
                                    if (new_tail) new_tail->next = clone;
                                    new_tail = clone;
                                }
                                
                                // 4. Если вызов функции сохранял результат, добавляем инструкцию присваивания
                                if (call_instr->result && return_val) {
                                    IRInstruction* assign = ir_instruction_new(opt->arena, IR_OP_ASSIGN);
                                    assign->result = call_instr->result;
                                    assign->operand1 = substitute_value(opt->arena, return_val, callee, param_map, param_count, temp_map, temp_count, call_instr->result);
                                    
                                    if (!new_head) new_head = assign;
                                    if (new_tail) new_tail->next = assign;
                                    new_tail = assign;
                                }
                                
                                // 5. Встраиваем новый список инструкций вместо call_instr
                                if (new_head) {
                                    new_tail->next = call_instr->next;
                                    *p_curr = new_head; // Заменяем call_instr на new_head
                                    
                                    // Пересчитываем количество инструкций в блоке
                                    block->instruction_count = 0;
                                    for (IRInstruction* i = block->instructions; i; i = i->next) {
                                        block->instruction_count++;
                                    }
                                    
                                    // Обновляем last_instruction
                                    IRInstruction* last = block->instructions;
                                    while (last && last->next) last = last->next;
                                    block->last_instruction = last;
                                    
                                    changed = true;
                                    opt->optimizations_applied++;
                                    
                                    // Не двигаем p_curr, так как мы только что вставили новые инструкции.
                                    // Следующая итерация проверит new_tail->next (бывший call_instr->next)
                                    continue; 
                                } else {
                                    // Если новых инструкций нет (например, функция была пустой), просто удаляем вызов
                                    *p_curr = call_instr->next;
                                    changed = true;
                                    opt->optimizations_applied++;
                                    continue;
                                }
                            }
                        }
                    }
                    
                    // Переходим к следующей инструкции
                    p_curr = &(*p_curr)->next;
                }
            }
        }
    }
}

// Обёртка для соответствия интерфейсу optimizer.h
void optimizer_function_inlining(Optimizer* opt, IRModule* module) {
    inline_optimize(opt, module);
}
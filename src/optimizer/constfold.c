// src/optimizer/constfold.c
#include "optimizer/constfold.h"
#include "common/string.h"  // <-- ДОБАВЛЕНО: необходимо для string_equals
#include <stdbool.h>
#include <stdio.h>

// Карта константных значений переменных
typedef struct {
    IRValue* var;
    IRValue* const_value;
} ConstMapping;

// Проверяет, является ли значение константой
static bool is_constant(IRValue* val) {
    if (!val) return false;
    return val->kind == IR_VALUE_CONST_INT ||
           val->kind == IR_VALUE_CONST_FLOAT ||
           val->kind == IR_VALUE_CONST_BOOL;
}

// Ищет константное значение переменной в карте
static IRValue* find_const_value(ConstMapping* map, int count, IRValue* var) {
    if (!var || var->kind != IR_VALUE_VAR) return NULL;
    
    for (int i = 0; i < count; i++) {
        // ИСПРАВЛЕНО 1: Сравниваем по имени, а не по указателю
        if (string_equals(map[i].var->name, var->name)) {
            return map[i].const_value;
        }
    }
    return NULL;
}

// Добавляет или обновляет константное значение в карте
static void add_const_mapping(ConstMapping* map, int* count, IRValue* var, IRValue* const_val) {
    // Проверяем, есть ли уже такая переменная
    for (int i = 0; i < *count; i++) {
        // ИСПРАВЛЕНО 2: Сравниваем по имени, а не по указателю
        if (string_equals(map[i].var->name, var->name)) {
            map[i].const_value = const_val;
            return;
        }
    }
    
    // Добавляем новую запись
    if (*count < 256) {
        map[*count].var = var;
        map[*count].const_value = const_val;
        (*count)++;
    }
}

// Выполняет операцию над двумя константами
static IRValue* fold_binary(Arena* arena, IROpcode op, IRValue* left, IRValue* right) {
    // Целочисленные операции
    if (left->kind == IR_VALUE_CONST_INT && right->kind == IR_VALUE_CONST_INT) {
        long long l = left->int_val;
        long long r = right->int_val;
        long long result = 0;
        
        switch (op) {
            case IR_OP_ADD: result = l + r; break;
            case IR_OP_SUB: result = l - r; break;
            case IR_OP_MUL: result = l * r; break;
            case IR_OP_DIV: 
                if (r == 0) return NULL;
                result = l / r; 
                break;
            case IR_OP_MOD: 
                if (r == 0) return NULL;
                result = l % r; 
                break;
            case IR_OP_EQ:  return ir_value_const_bool(arena, l == r);
            case IR_OP_NE:  return ir_value_const_bool(arena, l != r);
            case IR_OP_LT:  return ir_value_const_bool(arena, l < r);
            case IR_OP_LE:  return ir_value_const_bool(arena, l <= r);
            case IR_OP_GT:  return ir_value_const_bool(arena, l > r);
            case IR_OP_GE:  return ir_value_const_bool(arena, l >= r);
            default: return NULL;
        }
        
        return ir_value_const_int(arena, result, left->type);
    }
    
    // Вещественные операции
    if (left->kind == IR_VALUE_CONST_FLOAT && right->kind == IR_VALUE_CONST_FLOAT) {
        double l = left->float_val;
        double r = right->float_val;
        double result = 0.0;
        
        switch (op) {
            case IR_OP_ADD: result = l + r; break;
            case IR_OP_SUB: result = l - r; break;
            case IR_OP_MUL: result = l * r; break;
            case IR_OP_DIV: 
                if (r == 0.0) return NULL;
                result = l / r; 
                break;
            case IR_OP_EQ:  return ir_value_const_bool(arena, l == r);
            case IR_OP_NE:  return ir_value_const_bool(arena, l != r);
            case IR_OP_LT:  return ir_value_const_bool(arena, l < r);
            case IR_OP_LE:  return ir_value_const_bool(arena, l <= r);
            case IR_OP_GT:  return ir_value_const_bool(arena, l > r);
            case IR_OP_GE:  return ir_value_const_bool(arena, l >= r);
            default: return NULL;
        }
        
        return ir_value_const_float(arena, result, left->type);
    }
    
    // Логические операции
    if (left->kind == IR_VALUE_CONST_BOOL && right->kind == IR_VALUE_CONST_BOOL) {
        bool l = left->bool_val;
        bool r = right->bool_val;
        
        switch (op) {
            case IR_OP_AND: return ir_value_const_bool(arena, l && r);
            case IR_OP_OR:  return ir_value_const_bool(arena, l || r);
            case IR_OP_EQ:  return ir_value_const_bool(arena, l == r);
            case IR_OP_NE:  return ir_value_const_bool(arena, l != r);
            default: return NULL;
        }
    }
    
    return NULL;
}

void constfold_optimize(Optimizer* opt, IRFunction* func) {
    ConstMapping* const_map = ARENA_ARRAY(opt->arena, ConstMapping, 256);
    int const_count = 0;
    
    // Первый проход: собираем константные значения переменных
    for (IRBlock* block = func->blocks; block; block = block->next) {
        for (IRInstruction* instr = block->instructions; instr; instr = instr->next) {
            // Если это присваивание константы переменной
            if (instr->opcode == IR_OP_ASSIGN && 
                instr->result && instr->result->kind == IR_VALUE_VAR &&
                instr->operand1 && is_constant(instr->operand1)) {
                add_const_mapping(const_map, &const_count, instr->result, instr->operand1);
            }
            // ВАЖНО: Если переменная переприсваивается не-константой, удаляем её из карты
            else if (instr->opcode == IR_OP_ASSIGN && 
                     instr->result && instr->result->kind == IR_VALUE_VAR &&
                     instr->operand1 && !is_constant(instr->operand1)) {
                // Удаляем переменную из карты констант
                for (int i = 0; i < const_count; i++) {
                    // ИСПРАВЛЕНО 3: Сравниваем по имени, а не по указателю!
                    if (string_equals(const_map[i].var->name, instr->result->name)) {
                        // Сдвигаем массив
                        for (int j = i; j < const_count - 1; j++) {
                            const_map[j] = const_map[j + 1];
                        }
                        const_count--;
                        break;
                    }
                }
            }
        }
    }
    
    // Второй проход: заменяем переменные на их константные значения и сворачиваем
    for (IRBlock* block = func->blocks; block; block = block->next) {
        for (IRInstruction* instr = block->instructions; instr; instr = instr->next) {
            // Обрабатываем только бинарные операции
            if (instr->opcode >= IR_OP_ADD && instr->opcode <= IR_OP_GE) {
                IRValue* left = instr->operand1;
                IRValue* right = instr->operand2;
                
                // Пытаемся заменить переменные на константы
                IRValue* left_const = find_const_value(const_map, const_count, left);
                IRValue* right_const = find_const_value(const_map, const_count, right);
                
                if (left_const) left = left_const;
                if (right_const) right = right_const;
                
                // Если оба операнда теперь константы, сворачиваем
                if (is_constant(left) && is_constant(right)) {
                    IRValue* folded = fold_binary(opt->arena, instr->opcode, left, right);
                    
                    if (folded && instr->result) {
                        // Заменяем инструкцию на присваивание константы
                        instr->opcode = IR_OP_ASSIGN;
                        instr->operand1 = folded;
                        instr->operand2 = NULL;
                        opt->optimizations_applied++;
                        
                        // Если результат - временная переменная, добавляем её в карту
                        if (instr->result->kind == IR_VALUE_TEMP) {
                            add_const_mapping(const_map, &const_count, instr->result, folded);
                        }
                    }
                } else {
                    // Если не удалось свернуть, но заменили переменные на константы
                    if (left != instr->operand1 || right != instr->operand2) {
                        instr->operand1 = left;
                        instr->operand2 = right;
                        opt->optimizations_applied++;
                    }
                }
            }
        }
    }
}

void optimizer_constant_folding(Optimizer* opt, IRFunction* func) {
    constfold_optimize(opt, func);
}
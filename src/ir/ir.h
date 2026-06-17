// src/ir/ir.h
#ifndef BEVEL_IR_H
#define BEVEL_IR_H

#include "semantic/types.h"
#include "common/string.h"
#include "common/arena.h"
#include <stdbool.h>

// Типы значений в IR
typedef enum {
    IR_VALUE_CONST_INT,
    IR_VALUE_CONST_FLOAT,
    IR_VALUE_CONST_BOOL,
    IR_VALUE_CONST_STRING,
    IR_VALUE_TEMP,        // Временная переменная (%0, %1, ...)
    IR_VALUE_VAR,         // Локальная переменная
    IR_VALUE_GLOBAL,      // Глобальная переменная/функция
    IR_VALUE_PARAM        // Параметр функции
} IRValueKind;

// Значение в IR
typedef struct IRValue {
    IRValueKind kind;
    Type* type;
    
    union {
        long long int_val;
        double float_val;
        bool bool_val;
        String string_val;
        int temp_id;      // Для TEMP
        String name;      // Для VAR, GLOBAL, PARAM
    };
} IRValue;

// Операции в IR
typedef enum {
    // Арифметические
    IR_OP_ADD, IR_OP_SUB, IR_OP_MUL, IR_OP_DIV, IR_OP_MOD,
    
    // Сравнения
    IR_OP_EQ, IR_OP_NE, IR_OP_LT, IR_OP_LE, IR_OP_GT, IR_OP_GE,
    
    // Логические
    IR_OP_AND, IR_OP_OR, IR_OP_NOT,
    
    // Bitwise
    IR_OP_BIT_AND, IR_OP_BIT_OR, IR_OP_BIT_XOR, IR_OP_SHL, IR_OP_SHR,
    
    // Присваивание
    IR_OP_ASSIGN,
    
    // Вызов функции
    IR_OP_CALL,
    
    // Возврат
    IR_OP_RETURN,
    
    // Переходы
    IR_OP_BR,      // Безусловный переход
    IR_OP_BR_COND, // Условный переход
    
    // Память
    IR_OP_LOAD,    // Загрузка из памяти
    IR_OP_STORE,   // Сохранение в память
    
    // Структуры
    IR_OP_FIELD_ACCESS,
    
    // Преобразование типов
    IR_OP_CAST,
    
    //Phi-функция (для SSA)
    IR_OP_PHI,

    IR_OP_ADDR_OF,
    IR_OP_DEREF,
    IR_OP_ARRAY_LITERAL,  // Создание литерала массива
    IR_OP_INDEX_ACCESS,   // Чтение элемента: arr[i]
    IR_OP_INDEX_STORE,    // Запись в элемент: arr[i] = value
    IR_OP_VAR_DECL,     // Объявление переменной (для C-бэкенда)
    IR_OP_FIELD_STORE,    // Запись в поле: obj.field = value
} IROpcode;

// Инструкция IR (трёхадресный код)
typedef struct IRInstruction {
    IROpcode opcode;
    IRValue* result;      // Результат (может быть NULL для void операций)
    IRValue* operand1;    // Первый операнд
    IRValue* operand2;    // Второй операнд (может быть NULL)
    IRValue* operand3;    // <-- ДОБАВЛЕНО: Третий операнд (для INDEX_STORE, STORE и др.)
    IRValue** args;       // Для CALL: массив аргументов
    int arg_count;
    
    // Для BR_COND: метки
    struct IRBlock* true_block;
    struct IRBlock* false_block;
    
    // Для BR: метка
    struct IRBlock* target_block;
    
    // Для PHI: массив пар (значение, блок)
    struct {
        IRValue* value;
        struct IRBlock* block;
    }* phi_args;
    int phi_count;
    
    int line;             // Для отладки
    struct IRInstruction* next;

    bool is_used;
} IRInstruction;

// Базовый блок (последовательность инструкций без переходов)
typedef struct IRBlock {
    String label;
    IRInstruction* instructions;
    IRInstruction* last_instruction;
    int instruction_count;
    
    // Предшественники и наследники (для CFG)
    struct IRBlock** predecessors;
    int predecessor_count;
    struct IRBlock** successors;
    int successor_count;
    
    struct IRBlock* next;
} IRBlock;

// Функция в IR
typedef struct IRFunction {
    String name;
    Type* return_type;
    IRValue** params;
    int param_count;
    
    IRBlock* entry_block;
    IRBlock* blocks;
    int block_count;
    
    int next_temp_id;     // Для генерации уникальных имён временных переменных
    
    struct IRFunction* next;
} IRFunction;

// Реализация интерфейса структурой
typedef struct {
    Type* struct_type;      // Circle
    Type* interface_type;   // Shape
    String vtable_name;     // "Circle_Shape_vtable"
} InterfaceImpl;

typedef struct IRModule {
    String name;
    IRFunction* functions;
    int function_count;
    
    IRValue** globals;
    int global_count;
    
    Type** struct_types;
    int struct_type_count;
    
    Type** interface_types;
    int interface_type_count;
    
    // НОВОЕ: Список реализаций интерфейсов
    InterfaceImpl* impls;
    int impl_count;
    
    Arena* arena;
} IRModule;

// ===== Создание IR =====
IRModule* ir_module_new(Arena* arena, String name);
IRFunction* ir_function_new(Arena* arena, String name, Type* return_type);
IRBlock* ir_block_new(Arena* arena, String label);
IRInstruction* ir_instruction_new(Arena* arena, IROpcode opcode);

// ===== Создание значений =====
IRValue* ir_value_const_int(Arena* arena, long long val, Type* type);
IRValue* ir_value_const_float(Arena* arena, double val, Type* type);
IRValue* ir_value_const_bool(Arena* arena, bool val);
IRValue* ir_value_const_string(Arena* arena, String val);
IRValue* ir_value_temp(Arena* arena, int id, Type* type);
IRValue* ir_value_var(Arena* arena, String name, Type* type);
IRValue* ir_value_global(Arena* arena, String name, Type* type);
IRValue* ir_value_param(Arena* arena, String name, Type* type);

// ===== Добавление инструкций =====
void ir_block_add_instruction(IRBlock* block, IRInstruction* instr);
IRInstruction* ir_emit_binary(Arena* arena, IRBlock* block, IROpcode op, 
                               IRValue* left, IRValue* right, Type* result_type);
IRInstruction* ir_emit_unary(Arena* arena, IRBlock* block, IROpcode op, 
                              IRValue* operand, Type* result_type);
IRInstruction* ir_emit_assign(Arena* arena, IRBlock* block, 
                               IRValue* dest, IRValue* src);
IRInstruction* ir_emit_call(Arena* arena, IRBlock* block, 
                             IRValue* func, IRValue** args, int arg_count, 
                             Type* result_type);
IRInstruction* ir_emit_return(Arena* arena, IRBlock* block, IRValue* value);
IRInstruction* ir_emit_br(Arena* arena, IRBlock* block, IRBlock* target);
IRInstruction* ir_emit_br_cond(Arena* arena, IRBlock* block, IRValue* cond, 
                                IRBlock* true_block, IRBlock* false_block);
IRInstruction* ir_emit_cast(Arena* arena, IRBlock* block, 
                             IRValue* src, Type* target_type);

// ===== Управление блоками =====
void ir_function_add_block(IRFunction* func, IRBlock* block);

// ===== Вывод IR =====
void ir_print_module(IRModule* module);
void ir_print_function(IRFunction* func);
void ir_print_block(IRBlock* block);
void ir_print_instruction(IRInstruction* instr);
void ir_print_value(IRValue* value);

#endif // BEVEL_IR_H
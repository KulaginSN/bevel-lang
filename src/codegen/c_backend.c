// src/codegen/c_backend.c
#include "codegen/c_backend.h"
#include "semantic/types.h"
#include <stdio.h>
#include <string.h>
#include <stdbool.h>

// Преобразование типа Bevel в валидный тип C
static const char* bevel_type_to_c(Type* type) {
    if (!type) return "void";
    if (type->kind == TYPE_VOID) return "void";
    if (type->kind == TYPE_BOOL) return "int"; // В C нет нативного bool без stdbool, int надежнее для IR
    if (type->kind == TYPE_CHAR) return "char";
    if (type->kind == TYPE_STRING) return "const char*";
    
    if (type->kind == TYPE_INT || type->kind == TYPE_UINT) {
        if (type->integer.bits == 32) return "int32_t";
        if (type->integer.bits == 64) return "int64_t";
        if (type->integer.bits == 8) return "int8_t";
        if (type->integer.bits == 16) return "int16_t";
        return "int32_t";
    }
    
    if (type->kind == TYPE_FLOAT) {
        if (type->floating.bits == 64) return "double";
        if (type->floating.bits == 32) return "float";
        return "double";
    }
    
    if (type->kind == TYPE_POINTER) {
        if (!type->pointer.pointee) return "void*";
        
        // НОВОЕ: Если это указатель на функцию, обрабатываем specially
        if (type->pointer.pointee->kind == TYPE_FUNCTION) {
            // Для указателя на функцию возвращаем сам тип функции
            // (в C функции уже являются указателями)
            return bevel_type_to_c(type->pointer.pointee);
        }
        
        // Рекурсивно получаем C-тип для базового типа
        const char* base_type = bevel_type_to_c(type->pointer.pointee);
        
        // Используем кольцевой буфер (4 слота) для безопасности, 
        // если функция вызывается несколько раз в одном fprintf
        static char ptr_buf[4][256];
        static int buf_idx = 0;
        char* current_buf = ptr_buf[buf_idx];
        buf_idx = (buf_idx + 1) % 4;
        
        snprintf(current_buf, 256, "%s*", base_type);
        return current_buf;
    }
    
    if (type->kind == TYPE_STRUCT || type->kind == TYPE_INTERFACE || type->kind == TYPE_ENUM) {
        if (type->structure.name.length > 0 && type->structure.name.data) {
            static char name_buf[256];
            int len = type->structure.name.length;
            if (len > 255) len = 255;
            memcpy(name_buf, type->structure.name.data, len);
            name_buf[len] = '\0';
            return name_buf;
        }
    }

    if (type->kind == TYPE_FUNCTION) {
        // Указатель на функцию: ret_type (*name)(params)
        // Для использования в объявлениях переменных: ret_type (*)(params)
        static char func_buf[8][512];
        static int func_idx = 0;
        char* current = func_buf[func_idx];
        func_idx = (func_idx + 1) % 8;
        
        const char* ret_type = bevel_type_to_c(type->function.return_type);
        
        // Формируем: ret_type (*)(param1, param2, ...)
        int offset = snprintf(current, 512, "%s (*)(void*", ret_type);
        
        // Добавляем параметры (пропуская self)
        for (int i = 1; i < type->function.param_count; i++) {
            const char* param_type = bevel_type_to_c(type->function.param_types[i]);
            offset += snprintf(current + offset, 512 - offset, ", %s", param_type);
        }
        
        snprintf(current + offset, 512 - offset, ")");
        return current;
    }

    if (type->kind == TYPE_ARRAY) {
        // Возвращаем только базовый тип, размер [N] добавим при объявлении
        return bevel_type_to_c(type->array.element);
    }
    
    if (type->kind == TYPE_SLICE) {
        static char slice_buf[8][128];
        static int slice_idx = 0;
        char* current = slice_buf[slice_idx];
        slice_idx = (slice_idx + 1) % 8;
        
        // Используем короткое имя типа для среза (i32, i64, f32, f64 и т.д.)
        const char* elem_name = "void";
        Type* elem = type->slice.element;
        
        if (elem) {
            switch (elem->kind) {
                case TYPE_INT:
                    if (elem->integer.bits == 8)  elem_name = "i8";
                    else if (elem->integer.bits == 16) elem_name = "i16";
                    else if (elem->integer.bits == 32) elem_name = "i32";
                    else if (elem->integer.bits == 64) elem_name = "i64";
                    break;
                case TYPE_UINT:
                    if (elem->integer.bits == 8)  elem_name = "u8";
                    else if (elem->integer.bits == 16) elem_name = "u16";
                    else if (elem->integer.bits == 32) elem_name = "u32";
                    else if (elem->integer.bits == 64) elem_name = "u64";
                    break;
                case TYPE_FLOAT:
                    if (elem->floating.bits == 32) elem_name = "f32";
                    else if (elem->floating.bits == 64) elem_name = "f64";
                    else if (elem->floating.bits == 80) elem_name = "f80";
                    else if (elem->floating.bits == 128) elem_name = "f128";
                    break;
                case TYPE_BOOL:   elem_name = "bool"; break;
                case TYPE_CHAR:   elem_name = "char"; break;
                case TYPE_STRING: elem_name = "string"; break;
                default: break;
            }
        }
        
        snprintf(current, 128, "Slice_%s", elem_name);
        return current;
    }
    
    return "int32_t";
}


// Печать значения IR как C-выражения
static void c_print_value(FILE* out, IRValue* val) {
    if (!val) {
        fprintf(out, "0");
        return;
    }
    switch (val->kind) {
        case IR_VALUE_CONST_INT:
            fprintf(out, "%lld", val->int_val);
            break;
        case IR_VALUE_CONST_FLOAT:
            fprintf(out, "%f", val->float_val);
            break;
        case IR_VALUE_CONST_BOOL:
            fprintf(out, "%s", val->bool_val ? "1" : "0");
            break;
        case IR_VALUE_TEMP:
            fprintf(out, "t%d", val->temp_id);
            break;
        case IR_VALUE_VAR:
        case IR_VALUE_PARAM:
        case IR_VALUE_GLOBAL:
            if (val->name.data && val->name.length > 0) {
                fprintf(out, "%.*s", (int)val->name.length, val->name.data);
            } else {
                fprintf(out, "unknown_var");
            }
            break;
        default:
            fprintf(out, "0");
            break;
    }
}

// Генерация одной инструкции IR в C
static void c_emit_instruction(FILE* out, IRInstruction* instr) {
    fprintf(out, "    ");

    switch (instr->opcode) {
        case IR_OP_ASSIGN: {
            bool is_array = false;
            bool is_slice_init = false;
            
            if (instr->result->type && instr->result->type->kind == TYPE_ARRAY) {
                is_array = true;
            }
            
            // Проверяем, не является ли это инициализацией среза из массива
            if (instr->result->type && instr->result->type->kind == TYPE_SLICE &&
                instr->operand1->type && instr->operand1->type->kind == TYPE_ARRAY) {
                is_slice_init = true;
            }
            
            if (is_slice_init) {
                // s = (Slice_i32){arr, 5};
                const char* elem_name = "void";
                Type* elem = instr->result->type->slice.element;
                
                if (elem) {
                    switch (elem->kind) {
                        case TYPE_INT:
                            if (elem->integer.bits == 8)  elem_name = "i8";
                            else if (elem->integer.bits == 16) elem_name = "i16";
                            else if (elem->integer.bits == 32) elem_name = "i32";
                            else if (elem->integer.bits == 64) elem_name = "i64";
                            break;
                        case TYPE_UINT:
                            if (elem->integer.bits == 8)  elem_name = "u8";
                            else if (elem->integer.bits == 16) elem_name = "u16";
                            else if (elem->integer.bits == 32) elem_name = "u32";
                            else if (elem->integer.bits == 64) elem_name = "u64";
                            break;
                        case TYPE_FLOAT:
                            if (elem->floating.bits == 32) elem_name = "f32";
                            else if (elem->floating.bits == 64) elem_name = "f64";
                            break;
                        case TYPE_BOOL:   elem_name = "bool"; break;
                        case TYPE_CHAR:   elem_name = "char"; break;
                        case TYPE_STRING: elem_name = "string"; break;
                        default: break;
                    }
                }
                
                c_print_value(out, instr->result);
                fprintf(out, " = (Slice_%s){", elem_name);
                c_print_value(out, instr->operand1);
                fprintf(out, ", %d};\n", instr->operand1->type->array.size);
            } else if (is_array) {
                // Для массивов используем memcpy
                fprintf(out, "memcpy(");
                c_print_value(out, instr->result);
                fprintf(out, ", ");
                c_print_value(out, instr->operand1);
                fprintf(out, ", sizeof(");
                c_print_value(out, instr->result);
                fprintf(out, "));\n");
            } else {
                // Обычное присваивание: result = operand1;
                c_print_value(out, instr->result);
                fprintf(out, " = ");
                c_print_value(out, instr->operand1);
                fprintf(out, ";\n");
            }
            break;
        }
        case IR_OP_ADD:
            c_print_value(out, instr->result);
            fprintf(out, " = ");
            c_print_value(out, instr->operand1);
            fprintf(out, " + ");
            c_print_value(out, instr->operand2);
            fprintf(out, ";\n");
            break;
        case IR_OP_SUB:
            c_print_value(out, instr->result);
            fprintf(out, " = ");
            c_print_value(out, instr->operand1);
            fprintf(out, " - ");
            c_print_value(out, instr->operand2);
            fprintf(out, ";\n");
            break;
        case IR_OP_MUL:
            c_print_value(out, instr->result);
            fprintf(out, " = ");
            c_print_value(out, instr->operand1);
            fprintf(out, " * ");
            c_print_value(out, instr->operand2);
            fprintf(out, ";\n");
            break;
        case IR_OP_DIV:
            c_print_value(out, instr->result);
            fprintf(out, " = ");
            c_print_value(out, instr->operand1);
            fprintf(out, " / ");
            c_print_value(out, instr->operand2);
            fprintf(out, ";\n");
            break;
        case IR_OP_STORE:
            fprintf(out, "*");
            c_print_value(out, instr->operand1);
            fprintf(out, " = ");
            c_print_value(out, instr->operand2);
            fprintf(out, ";\n");
            break;
        case IR_OP_EQ:
            c_print_value(out, instr->result);
            fprintf(out, " = ");
            c_print_value(out, instr->operand1);
            fprintf(out, " == ");
            c_print_value(out, instr->operand2);
            fprintf(out, ";\n");
            break;
        case IR_OP_NE:
            c_print_value(out, instr->result);
            fprintf(out, " = ");
            c_print_value(out, instr->operand1);
            fprintf(out, " != ");
            c_print_value(out, instr->operand2);
            fprintf(out, ";\n");
            break;
        case IR_OP_LT:
            c_print_value(out, instr->result);
            fprintf(out, " = ");
            c_print_value(out, instr->operand1);
            fprintf(out, " < ");
            c_print_value(out, instr->operand2);
            fprintf(out, ";\n");
            break;
        case IR_OP_LE:
            c_print_value(out, instr->result);
            fprintf(out, " = ");
            c_print_value(out, instr->operand1);
            fprintf(out, " <= ");
            c_print_value(out, instr->operand2);
            fprintf(out, ";\n");
            break;
        case IR_OP_GT:
            c_print_value(out, instr->result);
            fprintf(out, " = ");
            c_print_value(out, instr->operand1);
            fprintf(out, " > ");
            c_print_value(out, instr->operand2);
            fprintf(out, ";\n");
            break;
        case IR_OP_GE:
            c_print_value(out, instr->result);
            fprintf(out, " = ");
            c_print_value(out, instr->operand1);
            fprintf(out, " >= ");
            c_print_value(out, instr->operand2);
            fprintf(out, ";\n");
            break;
        case IR_OP_AND:
            c_print_value(out, instr->result);
            fprintf(out, " = ");
            c_print_value(out, instr->operand1);
            fprintf(out, " && ");
            c_print_value(out, instr->operand2);
            fprintf(out, ";\n");
            break;
        case IR_OP_OR:
            c_print_value(out, instr->result);
            fprintf(out, " = ");
            c_print_value(out, instr->operand1);
            fprintf(out, " || ");
            c_print_value(out, instr->operand2);
            fprintf(out, ";\n");
            break;
        case IR_OP_NOT:
            c_print_value(out, instr->result);
            fprintf(out, " = !");
            c_print_value(out, instr->operand1);
            fprintf(out, ";\n");
            break;
        case IR_OP_RETURN:
            if (instr->operand1) {
                fprintf(out, "return ");
                c_print_value(out, instr->operand1);
                fprintf(out, ";\n");
            } else {
                fprintf(out, "return;\n");
            }
            break;
            
        case IR_OP_BR:
            fprintf(out, "goto %.*s;\n", 
                    (int)instr->target_block->label.length, instr->target_block->label.data);
            break;
        case IR_OP_BR_COND:
            fprintf(out, "if (");
            c_print_value(out, instr->operand1);
            fprintf(out, ") goto %.*s; else goto %.*s;\n",
                    (int)instr->true_block->label.length, instr->true_block->label.data,
                    (int)instr->false_block->label.length, instr->false_block->label.data);
            break;

        case IR_OP_FIELD_ACCESS: {
            // result = obj.field ИЛИ result = ptr->field
            c_print_value(out, instr->result);
            fprintf(out, " = ");
            c_print_value(out, instr->operand1);
            
            // Проверяем, является ли operand1 указателем
            if (instr->operand1->type && instr->operand1->type->kind == TYPE_POINTER) {
                fprintf(out, "->");
            } else {
                fprintf(out, ".");
            }
            
            if (instr->operand2 && instr->operand2->kind == IR_VALUE_CONST_STRING) {
                fprintf(out, "%.*s",
                        (int)instr->operand2->string_val.length,
                        instr->operand2->string_val.data);
            } else {
                fprintf(out, "?unknown_field?");
            }
            fprintf(out, ";\n");
            break;
        }

        case IR_OP_CALL: {
            // result = callee(arg1, arg2, ...)
            if (instr->result) {
                c_print_value(out, instr->result);
                fprintf(out, " = ");
            }
            
            // Имя функции из operand1
            // Имя функции из operand1
            if (instr->operand1) {
                if (instr->operand1->kind == IR_VALUE_GLOBAL) {
                    // Функция: operand1->name содержит имя функции
                    fprintf(out, "%.*s",
                            (int)instr->operand1->name.length,
                            instr->operand1->name.data);
                } else if (instr->operand1->kind == IR_VALUE_VAR || 
                           instr->operand1->kind == IR_VALUE_TEMP || 
                           instr->operand1->kind == IR_VALUE_PARAM) {
                    c_print_value(out, instr->operand1);
                } else {
                    fprintf(out, "/* unknown callee kind=%d */", instr->operand1->kind);
                }
            } else {
                fprintf(out, "/* null callee */");
            }
            
            fprintf(out, "(");
            
            // Аргументы
            if (instr->args && instr->arg_count > 0) {
                for (int i = 0; i < instr->arg_count; i++) {
                    if (i > 0) fprintf(out, ", ");
                    c_print_value(out, instr->args[i]);
                }
            }
            
            fprintf(out, ");\n");
            break;
        }
            
        case IR_OP_ADDR_OF:
            c_print_value(out, instr->result);
            fprintf(out, " = &");
            c_print_value(out, instr->operand1);
            fprintf(out, ";\n");
            break;
            
        case IR_OP_DEREF:
            c_print_value(out, instr->result);
            fprintf(out, " = *");
            c_print_value(out, instr->operand1);
            fprintf(out, ";\n");
            break;

        case IR_OP_ARRAY_LITERAL: {
            // result = (int32_t[]){elem0, elem1, ...}
            // Используем compound literal + memcpy
            const char* elem_type = "int32_t";
            if (instr->result->type && instr->result->type->kind == TYPE_ARRAY) {
                elem_type = bevel_type_to_c(instr->result->type->array.element);
            }
            
            fprintf(out, "    memcpy(");
            c_print_value(out, instr->result);
            fprintf(out, ", (%s[]){", elem_type);
            for (int i = 0; i < instr->arg_count; i++) {
                if (i > 0) fprintf(out, ", ");
                c_print_value(out, instr->args[i]);
            }
            fprintf(out, "}, sizeof(");
            c_print_value(out, instr->result);
            fprintf(out, "));\n");
            break;
        }
        
        case IR_OP_INDEX_ACCESS: {
            c_print_value(out, instr->result);
            fprintf(out, " = ");
            
            // НОВОЕ: Для срезов используем .data[i], для массивов — просто [i]
            if (instr->operand1->type && instr->operand1->type->kind == TYPE_SLICE) {
                c_print_value(out, instr->operand1);
                fprintf(out, ".data[");
                c_print_value(out, instr->operand2);
                fprintf(out, "];\n");
            } else {
                c_print_value(out, instr->operand1);
                fprintf(out, "[");
                c_print_value(out, instr->operand2);
                fprintf(out, "];\n");
            }
            break;
        }
        
        case IR_OP_INDEX_STORE: {
            // НОВОЕ: Для срезов используем .data[i], для массивов — просто [i]
            if (instr->operand1->type && instr->operand1->type->kind == TYPE_SLICE) {
                c_print_value(out, instr->operand1);
                fprintf(out, ".data[");
                c_print_value(out, instr->operand2);
                fprintf(out, "] = ");
                c_print_value(out, instr->operand3);
                fprintf(out, ";\n");
            } else {
                c_print_value(out, instr->operand1);
                fprintf(out, "[");
                c_print_value(out, instr->operand2);
                fprintf(out, "] = ");
                c_print_value(out, instr->operand3);
                fprintf(out, ";\n");
            }
            break;
        }
        
        case IR_OP_VAR_DECL:
            // Ничего не генерируем — объявление уже поднято в начало функции
            // через emit_variable_declarations
            break;
        case IR_OP_FIELD_STORE: {
            // obj.field = value ИЛИ ptr->field = value
            c_print_value(out, instr->operand1);
            
            // Проверяем, является ли operand1 указателем
            if (instr->operand1->type && instr->operand1->type->kind == TYPE_POINTER) {
                fprintf(out, "->");
            } else {
                fprintf(out, ".");
            }
            
            if (instr->operand2 && instr->operand2->kind == IR_VALUE_CONST_STRING) {
                fprintf(out, "%.*s", 
                        (int)instr->operand2->string_val.length,
                        instr->operand2->string_val.data);
            } else {
                fprintf(out, "?unknown_field?");
            }
            fprintf(out, " = ");
            c_print_value(out, instr->operand3);
            fprintf(out, ";\n");
            break;
        }
        
        default:
            fprintf(out, "/* unsupported opcode %d */;\n", instr->opcode);
            break;
    }
}
// ШАГ 1: Подъем объявлений переменных с защитой от дубликатов
static void emit_variable_declarations(FILE* out, IRFunction* func) {
    // Простой массив для отслеживания уже объявленных переменных (IRValue*)
    // 256 элементов более чем достаточно для наших тестовых функций
    IRValue* declared[256];
    int declared_count = 0;

    for (IRBlock* block = func->blocks; block; block = block->next) {
        for (IRInstruction* instr = block->instructions; instr; instr = instr->next) {
            if (instr->result && (instr->result->kind == IR_VALUE_TEMP || instr->result->kind == IR_VALUE_VAR)) {
                
                // Проверяем, не объявляли ли мы уже эту переменную
                bool already_declared = false;
                for (int i = 0; i < declared_count; i++) {
                    if (declared[i] == instr->result) {
                        already_declared = true;
                        break;
                    }
                }

                // Если еще не объявляли, печатаем и запоминаем
                if (!already_declared) {
                    if (declared_count < 256) {
                        declared[declared_count++] = instr->result;
                    }

                    // Используем нашу универсальную функцию преобразования типов
                    // Она корректно обрабатывает все типы: int, float, void, string, char, pointer, struct
                    // НОВОЕ: Специальная обработка для указателей на функции
                    if (instr->result->type && instr->result->type->kind == TYPE_FUNCTION) {
                        // Формат: ret_type (*name)(params)
                        const char* ret_type = bevel_type_to_c(instr->result->type->function.return_type);
                        fprintf(out, "    %s (*", ret_type);
                        
                        // Печатаем имя переменной
                        if (instr->result->kind == IR_VALUE_TEMP) {
                            fprintf(out, "t%d", instr->result->temp_id);
                        } else {
                            fprintf(out, "%.*s", (int)instr->result->name.length, instr->result->name.data);
                        }
                        
                        fprintf(out, ")(void*");
                        
                        // Дополнительные параметры (пропуская self)
                        for (int i = 1; i < instr->result->type->function.param_count; i++) {
                            fprintf(out, ", %s", bevel_type_to_c(instr->result->type->function.param_types[i]));
                        }
                        fprintf(out, ");\n");
                    } else {
                        // Обычные типы
                        const char* c_type = bevel_type_to_c(instr->result->type);
                        
                        fprintf(out, "    %s ", c_type);
                        
                        // Печатаем имя переменной
                        if (instr->result->kind == IR_VALUE_TEMP) {
                            fprintf(out, "t%d", instr->result->temp_id);
                        } else {
                            fprintf(out, "%.*s", (int)instr->result->name.length, instr->result->name.data);
                        }
                        
                        // НОВОЕ: Для массивов добавляем размер [N] после имени
                        if (instr->result->type && instr->result->type->kind == TYPE_ARRAY) {
                            fprintf(out, "[%d]", instr->result->type->array.size);
                        }
                        
                        fprintf(out, ";\n");
                    }
                    
                    // НОВОЕ: Для массивов добавляем размер [N] после имени
                    if (instr->result->type && instr->result->type->kind == TYPE_ARRAY) {
                        fprintf(out, "[%d]", instr->result->type->array.size);
                    }
                    
                }
            }
        }
    }
    fprintf(out, "\n");
}

// ШАГ 2: Генерация одной функции с поддержкой множественных блоков
static void c_emit_function(FILE* out, IRFunction* func) {
    const char* ret_type = bevel_type_to_c(func->return_type);
    fprintf(out, "%s %.*s(", ret_type, (int)func->name.length, func->name.data);
    
    for (int i = 0; i < func->param_count; i++) {
        if (i > 0) fprintf(out, ", ");
        const char* p_type = bevel_type_to_c(func->params[i]->type);
        fprintf(out, "%s %.*s", p_type, (int)func->params[i]->name.length, func->params[i]->name.data);
    }
    fprintf(out, ") {\n");
    
    // 1. Объявляем все переменные в начале функции
    emit_variable_declarations(out, func);
    
    // 2. Генерируем код для каждого базового блока
    for (IRBlock* block = func->blocks; block; block = block->next) {
        // Печатаем метку блока
        fprintf(out, "%.*s:\n", (int)block->label.length, block->label.data);
        
        // Печатаем инструкции блока
        for (IRInstruction* instr = block->instructions; instr; instr = instr->next) {
            c_emit_instruction(out, instr);
        }
    }
    
    // 3. Добавляем return 0 для функций, возвращающих int, ТОЛЬКО если его нет в конце
    if (func->return_type && func->return_type->kind == TYPE_INT) {
        // Находим последний блок
        IRBlock* last_block = func->blocks;
        while (last_block && last_block->next) {
            last_block = last_block->next;
        }
        
        // Проверяем, есть ли return в последней инструкции последнего блока
        bool has_return = false;
        if (last_block && last_block->instructions) {
            IRInstruction* last_instr = last_block->instructions;
            while (last_instr && last_instr->next) {
                last_instr = last_instr->next;
            }
            
            if (last_instr && last_instr->opcode == IR_OP_RETURN) {
                has_return = true;
            }
        }
        
        if (!has_return) {
            fprintf(out, "    return 0;\n");
        }
    }
    
    fprintf(out, "}\n\n");
}

// ===== Генерация typedef'ов для интерфейсов =====
static void emit_interface_typedefs(FILE* out, IRModule* module) {
    if (!module->interface_types || module->interface_type_count == 0) return;
    
    fprintf(out, "// Interface type definitions\n");
    
    for (int i = 0; i < module->interface_type_count; i++) {
        Type* iface = module->interface_types[i];
        if (!iface || iface->kind != TYPE_INTERFACE) continue;
        
        // 1. Typedef для vtable: Shape_vtable
        fprintf(out, "typedef struct {\n");
        for (Method* m = iface->interface.methods; m; m = m->next) {
            // f64 (*area)(void* self);
            const char* ret_c = bevel_type_to_c(m->function_type->function.return_type);
            fprintf(out, "    %s (*%.*s)(void*", ret_c, 
                    (int)m->name.length, m->name.data);
            
            // Дополнительные параметры (пропуская self)
            for (int j = 1; j < m->function_type->function.param_count; j++) {
                fprintf(out, ", %s", bevel_type_to_c(m->function_type->function.param_types[j]));
            }
            fprintf(out, ");\n");
        }
        fprintf(out, "} %.*s_vtable;\n\n", 
                (int)iface->interface.name.length, iface->interface.name.data);
        
        // 2. Typedef для самого интерфейса: Shape
        fprintf(out, "typedef struct {\n");
        fprintf(out, "    void* data;\n");
        fprintf(out, "    %.*s_vtable* vtable;\n", 
                (int)iface->interface.name.length, iface->interface.name.data);
        fprintf(out, "} %.*s;\n\n", 
                (int)iface->interface.name.length, iface->interface.name.data);
    }
}

// ===== Генерация глобальных vtable для реализаций интерфейсов =====
static void emit_vtable_globals(FILE* out, IRModule* module) {
    if (!module->impls || module->impl_count == 0) return;
    
    fprintf(out, "// VTable implementations\n");
    
    for (int i = 0; i < module->impl_count; i++) {
        InterfaceImpl* impl = &module->impls[i];
        Type* iface = impl->interface_type;
        Type* strct = impl->struct_type;
        
        // Shape_vtable Circle_Shape_vtable = { .area = (double (*)(void*))area };
        fprintf(out, "%.*s_vtable %.*s = {\n",
                (int)iface->interface.name.length, iface->interface.name.data,
                (int)impl->vtable_name.length, impl->vtable_name.data);
        
        bool first_method = true;
        for (Method* m = iface->interface.methods; m; m = m->next) {
            Method* struct_method = type_find_method(strct, m->name);
            if (struct_method) {
                const char* ret_c = bevel_type_to_c(m->function_type->function.return_type);
                
                // Запятая перед элементом (кроме первого)
                if (!first_method) {
                    fprintf(out, ",\n");
                }
                first_method = false;
                
                fprintf(out, "    .%.*s = (%s (*)(void*))%.*s_%.*s",
                        (int)m->name.length, m->name.data,
                        ret_c,
                        (int)strct->structure.name.length, strct->structure.name.data,
                        (int)m->name.length, m->name.data);
            }
        }
        fprintf(out, "\n");
        fprintf(out, "};\n\n");
    }
}



void c_backend_generate(FILE* out, IRModule* module, Arena* arena) {
    (void)arena;
    fprintf(out, "// Generated by Bevel Compiler (C Backend)\n");
    fprintf(out, "#include <stdint.h>\n");
    fprintf(out, "#include <string.h>\n");
    fprintf(out, "\n");
    
    // Определения типов срезов
    fprintf(out, "// Slice type definitions\n");
    fprintf(out, "typedef struct { int32_t* data; int64_t len; } Slice_i32;\n");
    fprintf(out, "typedef struct { int64_t* data; int64_t len; } Slice_i64;\n");
    fprintf(out, "typedef struct { float* data; int64_t len; } Slice_f32;\n");
    fprintf(out, "typedef struct { double* data; int64_t len; } Slice_f64;\n");
    fprintf(out, "typedef struct { uint32_t* data; int64_t len; } Slice_u32;\n");
    fprintf(out, "typedef struct { uint64_t* data; int64_t len; } Slice_u64;\n");
    fprintf(out, "\n");
    
    // НОВОЕ: Определения типов структур
    // НОВОЕ: Определения типов структур
    if (module->struct_type_count > 0 && module->struct_types != NULL) {
        fprintf(out, "// Struct type definitions\n");
        for (int i = 0; i < module->struct_type_count; i++) {
            Type* t = module->struct_types[i];
            
            // ЗАЩИТА: Проверяем валидность типа
            if (!t || (uintptr_t)t < 0x1000) {
                fprintf(stderr, "[WARN] Skipping invalid struct type at index %d (ptr=%p)\n", i, (void*)t);
                continue;
            }

            // НОВОЕ: Пропускаем оригинальные generic структуры
            // (у них is_generic == true и нет type_args)
            if (t->structure.is_generic && t->structure.type_arg_count == 0) {
                continue;
            }
            
            if (!t->structure.name.data) {
                fprintf(stderr, "[WARN] Skipping struct with no name at index %d\n", i);
                continue;
            }
            
            fprintf(out, "typedef struct {\n");
            for (Field* f = t->structure.fields; f != NULL; f = f->next) {
                // ЗАЩИТА: Проверяем валидность поля
                if (!f || !f->name.data) continue;
                if (!f->type || (uintptr_t)f->type < 0x1000) {
                    fprintf(stderr, "[WARN] Field '%.*s' has invalid type %p, using int\n",
                            (int)f->name.length, f->name.data, (void*)f->type);
                    fprintf(out, "    int %.*s;\n", (int)f->name.length, f->name.data);
                    continue;
                }
                
                const char* field_c_type = bevel_type_to_c(f->type);
                if (!field_c_type) field_c_type = "int";
                
                fprintf(out, "    %s %.*s;\n", 
                        field_c_type,
                        (int)f->name.length, 
                        f->name.data);
            }

            fprintf(out, "} %.*s;\n\n", 
                    (int)t->structure.name.length, 
                    t->structure.name.data);
        }
    }

    // НОВОЕ: Forward declarations для функций (чтобы vtable могла на них ссылаться)
    if (module->functions) {
        fprintf(out, "// Forward declarations\n");
        for (IRFunction* func = module->functions; func; func = func->next) {
            const char* ret_c = bevel_type_to_c(func->return_type);
            fprintf(out, "%s %.*s(", ret_c, 
                    (int)func->name.length, func->name.data);
            for (int i = 0; i < func->param_count; i++) {
                if (i > 0) fprintf(out, ", ");
                fprintf(out, "%s", bevel_type_to_c(func->params[i]->type));
            }
            if (func->param_count == 0) fprintf(out, "void");
            fprintf(out, ");\n");
        }
        fprintf(out, "\n");
    }
    
    // НОВОЕ: Определения типов интерфейсов
    emit_interface_typedefs(out, module);
    emit_vtable_globals(out, module);  // <-- НОВОЕ

    for (IRFunction* func = module->functions; func; func = func->next) {
        // ЗАПАСНОЙ ВАРИАНТ: Пропускаем функции с типом void и параметрами void
        // (это артефакты неинстанцированных generic функций)
        if (func->return_type && func->return_type->kind == TYPE_VOID) {
            bool has_void_params = false;
            for (int i = 0; i < func->param_count; i++) {
                if (func->params[i]->type && func->params[i]->type->kind == TYPE_VOID) {
                    has_void_params = true;
                    break;
                }
            }
            
            if (has_void_params) {
                // Проверяем, есть ли "_" в имени (признак инстанцированной функции, например max_val_i32)
                bool has_underscore = false;
                for (size_t i = 0; i < func->name.length; i++) {
                    if (func->name.data[i] == '_') {
                        has_underscore = true;
                        break;
                    }
                }
                
                // Если это оригинальная generic функция (без "_"), пропускаем её
                if (!has_underscore) {
                    continue;
                }
            }
        }
        
        c_emit_function(out, func);
    }
}
// src/semantic/analyzer.c
#include "semantic/analyzer.h"
#include "common/utils.h"
#include <stdio.h>
#include <stdarg.h> 
#include <string.h>

// ===== Вспомогательные функции =====

static void error(Analyzer* a, int line, int col, const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    char buffer[512];
    vsnprintf(buffer, sizeof(buffer), fmt, args);
    va_end(args);
    
    error_reporter_semantic_error(a->errors, line, col, buffer);
    a->error_count++;
}

static void error_at_node(Analyzer* a, ASTNode* node, const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    char buffer[512];
    vsnprintf(buffer, sizeof(buffer), fmt, args);
    va_end(args);
    
    error_reporter_semantic_error(a->errors, node->line, node->column, buffer);
    a->error_count++;
}

// ===== Forward declarations =====

static Type* analyze_expression(Analyzer* a, ASTNode* node);
static void analyze_statement(Analyzer* a, ASTNode* node);
static void analyze_declaration(Analyzer* a, ASTNode* node);

// ===== Преобразование AST типа в семантический тип =====

static Type* resolve_type(Analyzer* a, ASTNode* type_node) {
    if (!type_node) {
        return type_error_new(a->arena);
    }
    
    // НОВОЕ: Обработка AST_TYPE_AS_EXPR (тип как выражение в generic вызовах)
    if (type_node->type == AST_TYPE_AS_EXPR) {
        TypeAsExprNode* tae = (TypeAsExprNode*)type_node;
        return resolve_type(a, tae->type_node);
    }
    
    if (type_node->type != AST_TYPE) {
        return type_error_new(a->arena);
    }
    
    TypeNode* tn = (TypeNode*)type_node;
    
    // Обработка указателей (рекурсивно)
    if (tn->is_pointer) {
        TypeNode base_tn = *tn;
        base_tn.is_pointer = false;
        Type* base_type = resolve_type(a, (ASTNode*)&base_tn);
        if (base_type->kind == TYPE_ERROR) return type_error_new(a->arena);
        return type_pointer_new(a->arena, base_type);
    }
    
    // Встроенные типы
    switch (tn->token_type) {
        case TOKEN_VOID:   return type_void();
        case TOKEN_BOOL_TYPE: return type_bool();
        case TOKEN_CHAR_TYPE: return type_char();
        case TOKEN_STRING_TYPE: return type_string();
        case TOKEN_I8:  return type_int(8, true);
        case TOKEN_I16: return type_int(16, true);
        case TOKEN_I32: return type_int(32, true);
        case TOKEN_I64: return type_int(64, true);
        case TOKEN_U8:  return type_int(8, false);
        case TOKEN_U16: return type_int(16, false);
        case TOKEN_U32: return type_int(32, false);
        case TOKEN_U64: return type_int(64, false);
        case TOKEN_F32: case TOKEN_SINGLE: return type_float(32);
        case TOKEN_F64: case TOKEN_DOUBLE: return type_float(64);
        case TOKEN_F80: case TOKEN_EXTENDED: return type_float(80);
        case TOKEN_F128: case TOKEN_QUAD: return type_float(128);
        case TOKEN_INT_TYPE: return type_int(32, true);
        
        case TOKEN_IDENTIFIER: {
            // НОВОЕ: Сначала проверяем generic параметры текущей области (T, E, ...)
            for (struct GenericParamNode* gp = a->current_generic_params; gp != NULL; gp = gp->next) {
                if (string_equals(gp->name, tn->name)) {
                    return type_generic_param_new(a->arena, tn->name, NULL);
                }
            }
            
            // Ищем базовый тип
            Type* user_type = symbol_lookup_type(&a->symbols, tn->name);
            if (!user_type) {
                error(a, tn->base.line, tn->base.column, 
                      "Unknown type '%.*s'", (int)tn->name.length, tn->name.data);
                return type_error_new(a->arena);
            }
            
            // НОВОЕ: Обработка инстанциации дженерик-структуры Result(f64, string)
            if (tn->type_arg_count > 0 && user_type->kind == TYPE_STRUCT) {
                // Проверяем, что базовая структура — generic
                if (!user_type->structure.is_generic) {
                    error(a, tn->base.line, tn->base.column,
                          "Type '%.*s' is not generic but used with type arguments",
                          (int)tn->name.length, tn->name.data);
                    return type_error_new(a->arena);
                }
                
                // Преобразуем ASTNode** type_args в Type** resolved_args
                Type** resolved_args = ARENA_ARRAY(a->arena, Type*, tn->type_arg_count);
                for (int i = 0; i < tn->type_arg_count; i++) {
                    resolved_args[i] = resolve_type(a, tn->type_args[i]);
                    if (resolved_args[i]->kind == TYPE_ERROR) {
                        return type_error_new(a->arena);
                    }
                }
                
                // Формируем манглированное имя: Result_f64_string
                String mangled_name = user_type->structure.name;
                for (int i = 0; i < tn->type_arg_count; i++) {
                    mangled_name = string_concat(a->arena, mangled_name, STRING_FROM_LITERAL("_"));
                    
                    Type* arg_type = resolved_args[i];
                    String arg_name = STRING_EMPTY;
                    
                    // Получаем имя типа для манглинга
                    if (arg_type->kind == TYPE_INT) {
                        char buf[16];
                        snprintf(buf, sizeof(buf), "%s%d", 
                                arg_type->integer.is_signed ? "i" : "u", 
                                arg_type->integer.bits);
                        arg_name = string_from_cstr(buf);
                    } else if (arg_type->kind == TYPE_FLOAT) {
                        char buf[16];
                        snprintf(buf, sizeof(buf), "f%d", arg_type->floating.bits);
                        arg_name = string_from_cstr(buf);
                    } else if (arg_type->kind == TYPE_BOOL) {
                        arg_name = string_from_cstr("bool");
                    } else if (arg_type->kind == TYPE_STRING) {
                        arg_name = string_from_cstr("string");
                    } else if (arg_type->kind == TYPE_STRUCT) {
                        arg_name = arg_type->structure.name;
                    } else {
                        arg_name = string_from_cstr("unknown");
                    }
                    
                    mangled_name = string_concat(a->arena, mangled_name, arg_name);
                }
                
                // Проверяем, не создали ли мы уже этот инстанцированный тип
                Type* inst_type = symbol_lookup_type(&a->symbols, mangled_name);
                if (!inst_type) {
                    // Создаём новую инстанцированную структуру
                    inst_type = type_struct_new(a->arena, mangled_name);
                    inst_type->structure.is_defined = true;
                    inst_type->structure.type_args = resolved_args;
                    inst_type->structure.type_arg_count = tn->type_arg_count;
                    
                    // Копируем поля, подставляя конкретные типы
                    for (Field* f = user_type->structure.fields; f != NULL; f = f->next) {
                        Type* field_type = f->type;
                        
                        // Если поле — generic параметр, подставляем соответствующий аргумент
                        if (field_type->kind == TYPE_GENERIC_PARAM) {
                            int param_index = -1;
                            int idx = 0;
                            for (struct GenericParamNode* gp = user_type->structure.generic_params;
                                 gp != NULL; gp = gp->next, idx++) {
                                if (string_equals(gp->name, field_type->generic_param.name)) {
                                    param_index = idx;
                                    break;
                                }
                            }
                            
                            if (param_index >= 0 && param_index < tn->type_arg_count) {
                                field_type = resolved_args[param_index];
                            }
                        }
                        
                        type_add_field(a->arena, inst_type, f->name, field_type);
                    }
                    
                    // Регистрируем инстанцированный тип в таблице символов
                    // ИСПРАВЛЕНО: используем type_node вместо node
                    symbol_define_type(&a->symbols, mangled_name, inst_type, type_node);

                symbol_define_type_global(&a->symbols, mangled_name, inst_type, type_node);
                }
                
                return tn->is_pointer ? type_pointer_new(a->arena, inst_type) : inst_type;
            }
            
            return tn->is_pointer ? type_pointer_new(a->arena, user_type) : user_type;
        }
        
        default:
            error(a, tn->base.line, tn->base.column, "Invalid type");
            return type_error_new(a->arena);
    }
}

// ===== Анализ выражений =====

static Type* analyze_expression(Analyzer* a, ASTNode* node) {
    if (!node) return type_error_new(a->arena);
    
    switch (node->type) {
        case AST_INT_LITERAL:
            return type_int(32, true);  // По умолчанию i32
        
        case AST_FLOAT_LITERAL:
            return type_float(64);  // По умолчанию f64
        
        case AST_STRING_LITERAL:
            return type_string();
        
        case AST_CHAR_LITERAL:
            return type_char();
        
        case AST_BOOL_LITERAL:
            return type_bool();
        
        case AST_IDENTIFIER: {
            IdentifierNode* id = (IdentifierNode*)node;
            Symbol* sym = symbol_lookup(&a->symbols, id->name);
            if (!sym) {
                error_at_node(a, node, "Undefined variable '%.*s'",
                              (int)id->name.length, id->name.data);
                return type_error_new(a->arena);
            }
            sym->is_used = true;
            
            // НОВОЕ: Если это тип (enum/struct), возвращаем его тип
            // Это позволяет использовать Status.Ok, Vector3.x и т.д.
            if (sym->kind == SYMBOL_ENUM || 
                sym->kind == SYMBOL_STRUCT || 
                sym->kind == SYMBOL_INTERFACE) {
                return sym->type;
            }
            
            return sym->type;
        }
        case AST_ASSIGN: {
            AssignNode* assign = (AssignNode*)node;
            
            Type* target_type = analyze_expression(a, assign->target);
            Type* value_type = analyze_expression(a, assign->value);
            
            if (target_type->kind == TYPE_ERROR || value_type->kind == TYPE_ERROR) {
                return type_error_new(a->arena);
            }
            
            // НОВОЕ: Разрешаем присваивание массива срезу
            if (target_type->kind == TYPE_SLICE && value_type->kind == TYPE_ARRAY) {
                // Проверяем совместимость типов элементов
                if (!types_equal(target_type->slice.element, value_type->array.element)) {
                    error_at_node(a, node, "Cannot assign array to slice: element types don't match.");
                    return type_error_new(a->arena);
                }
                return target_type;
            }
            
            // Существующая проверка совместимости типов
            if (!types_equal(target_type, value_type)) {
                // ... существующая логика ошибок ...
            }
            
            return target_type;
        }
        
        case AST_SELF_EXPR: {
            if (!a->current_struct_type) {
                error_at_node(a, node, "'self' can only be used in methods");
                return type_error_new(a->arena);
            }
            return type_pointer_new(a->arena, a->current_struct_type);
        }
        
        case AST_BINARY_EXPR: {
            BinaryExprNode* bin = (BinaryExprNode*)node;
            Type* left = analyze_expression(a, bin->left);
            Type* right = analyze_expression(a, bin->right);
            
            if (left->kind == TYPE_ERROR || right->kind == TYPE_ERROR) {
                return type_error_new(a->arena);
            }
            
            // Проверка совместимости типов для операции
            bool valid = false;
            Type* result = NULL;
            
            switch (bin->op) {
                case TOKEN_PLUS:
                case TOKEN_MINUS:
                case TOKEN_STAR:
                case TOKEN_SLASH:
                case TOKEN_PERCENT:
                    // Арифметические операции
                    if ((left->kind == TYPE_INT || left->kind == TYPE_UINT || left->kind == TYPE_FLOAT) &&
                        (right->kind == TYPE_INT || right->kind == TYPE_UINT || right->kind == TYPE_FLOAT)) {
                        
                        // Определяем результирующий тип (больший из двух)
                        if (left->kind == TYPE_FLOAT || right->kind == TYPE_FLOAT) {
                            // Если один из них float, результат float
                            int left_bits = (left->kind == TYPE_FLOAT) ? left->floating.bits : 0;
                            int right_bits = (right->kind == TYPE_FLOAT) ? right->floating.bits : 64; // int → f64
                            int result_bits = (left_bits > right_bits) ? left_bits : right_bits;
                            result = type_float(result_bits);
                        } else {
                            // Оба целые, результат — больший тип
                            int left_bits = left->integer.bits;
                            int right_bits = right->integer.bits;
                            bool left_signed = left->integer.is_signed;
                            bool right_signed = right->integer.is_signed;
                            
                            int result_bits = (left_bits > right_bits) ? left_bits : right_bits;
                            bool result_signed = left_signed || right_signed;
                            result = type_int(result_bits, result_signed);
                        }
                        valid = true;
                    }
                    break;
                    
                case TOKEN_EQEQ:
                case TOKEN_NOTEQ:
                case TOKEN_LT:
                case TOKEN_LTE:
                case TOKEN_GT:
                case TOKEN_GTE:
                    // Операции сравнения
                    if (types_equal(left, right) || 
                        types_implicitly_convertible(left, right) ||
                        types_implicitly_convertible(right, left)) {
                        valid = true;
                        result = type_bool();
                    }
                    break;
                    
                case TOKEN_AND:
                case TOKEN_OR:
                    // Логические операции
                    if (left->kind == TYPE_BOOL && right->kind == TYPE_BOOL) {
                        valid = true;
                        result = type_bool();
                    }
                    break;
                    
                default:
                    break;
            }
            
            if (!valid) {
                error_at_node(a, node, "Invalid operands for operator %s: %s and %s",
                            token_type_name(bin->op),
                            type_to_string(a->arena, left),
                            type_to_string(a->arena, right));
                return type_error_new(a->arena);
            }
            
            return result;
        }
        
        case AST_UNARY_EXPR: {
            UnaryExprNode* un = (UnaryExprNode*)node;
            Type* operand = analyze_expression(a, un->operand);

             // НОВОЕ: Отладка
            fprintf(stderr, "[DEBUG UNARY] op=%d (TOKEN_NOT=%d, TOKEN_MINUS=%d), operand kind=%d\n",
                    un->op, TOKEN_NOT, TOKEN_MINUS, operand->kind);
            
            
            // Подавляем каскадные ошибки
            if (operand->kind == TYPE_ERROR) {
                return type_error_new(a->arena);
            }
            
            if (un->op == TOKEN_MINUS) {
                if (operand->kind == TYPE_INT || operand->kind == TYPE_UINT || 
                    operand->kind == TYPE_FLOAT) {
                    return operand;
                }
            } else if (un->op == TOKEN_NOT) {
                // Разрешаем ! для bool и int
                if (operand->kind == TYPE_BOOL || operand->kind == TYPE_INT) {
                    return type_bool();
                }
            }
            
            error_at_node(a, node, "Invalid operand for unary operator (got %s)",
                          type_to_string(a->arena, operand));
            return type_error_new(a->arena);
        }

        case AST_CALL_EXPR: {
            CallExprNode* call = (CallExprNode*)node;
            
            // НОВОЕ: Проверяем, не является ли это generic вызовом
            // (первый аргумент — это тип: max_val(i32, 5, 10))
            bool is_generic_call = false;
            Type* concrete_type = NULL;
            
            if (call->arg_count > 0 && call->args[0] != NULL) {
                ASTNode* first_arg = call->args[0];
                if (first_arg->type == AST_TYPE || first_arg->type == AST_TYPE_AS_EXPR) {
                    is_generic_call = true;
                    concrete_type = resolve_type(a, first_arg);
                    
                    if (concrete_type->kind == TYPE_ERROR) {
                        return type_error_new(a->arena);
                    }
                }
            }
            
            Type* callee_type = analyze_expression(a, call->callee);
            
            if (!callee_type || callee_type->kind == TYPE_ERROR) {
                return type_error_new(a->arena);
            }
            
            if (callee_type->kind != TYPE_FUNCTION) {
                error_at_node(a, node, "Calling non-function type");
                return type_error_new(a->arena);
            }

            // НОВОЕ: Неявная инстанциация generic функций (вывод типов из аргументов)
            bool is_implicit_generic = false;
            FunctionDeclNode* gen_decl = NULL;
            if (call->callee && call->callee->type == AST_IDENTIFIER) {
                IdentifierNode* id = (IdentifierNode*)call->callee;
                Symbol* sym = symbol_lookup(&a->symbols, id->name);
                if (sym && sym->declaration && sym->declaration->type == AST_FUNCTION_DECL) {
                    gen_decl = (FunctionDeclNode*)sym->declaration;
                    if (gen_decl->is_generic && call->arg_count > 0) {
                        is_implicit_generic = true;
                    }
                }
            }

            if (is_implicit_generic) {
                // 1. Выводим конкретный тип для первого generic параметра
                Type* concrete_type = NULL;
                String gp_name = gen_decl->generic_params->name;

                for (int i = 0; i < call->arg_count && i < gen_decl->param_count; i++) {
                    Type* arg_t = analyze_expression(a, call->args[i]);
                    Type* param_t = callee_type->function.param_types[i];
                    if (param_t->kind == TYPE_GENERIC_PARAM && string_equals(param_t->generic_param.name, gp_name)) {
                        concrete_type = arg_t;
                        break;
                    }
                }

                if (!concrete_type || concrete_type->kind == TYPE_ERROR) {
                    error_at_node(a, node, "Cannot infer generic type for '%.*s'", 
                                 (int)gen_decl->name.length, gen_decl->name.data);
                    return type_error_new(a->arena);
                }

                // 2. Проверяем аргументы с подстановкой T -> concrete_type
                for (int i = 0; i < call->arg_count && i < gen_decl->param_count; i++) {
                    Type* arg_t = analyze_expression(a, call->args[i]);
                    Type* param_t = callee_type->function.param_types[i];
                    Type* expected = (param_t->kind == TYPE_GENERIC_PARAM && 
                                      string_equals(param_t->generic_param.name, gp_name)) 
                                     ? concrete_type : param_t;

                    if (!types_assignable(expected, arg_t)) {
                        error_at_node(a, node, "Argument %d: expected %s, got %s", 
                                     i + 1, 
                                     type_to_string(a->arena, expected), 
                                     type_to_string(a->arena, arg_t));
                    }
                }

                // 3. Возвращаем подставленный тип возврата
                Type* ret = callee_type->function.return_type;
                if (ret->kind == TYPE_GENERIC_PARAM && string_equals(ret->generic_param.name, gp_name)) {
                    return concrete_type;
                }
                return ret;
            }
            
            // Проверяем, не является ли это вызовом метода (obj.method(args))
            bool is_method_call = false;
            if (call->callee && call->callee->type == AST_FIELD_ACCESS) {
                FieldAccessNode* fa = (FieldAccessNode*)call->callee;
                
                Type* obj_type = analyze_expression(a, fa->object);
                
                if (!obj_type || obj_type->kind == TYPE_ERROR) {
                    return type_error_new(a->arena);
                }
                
                if (obj_type->kind == TYPE_POINTER) {
                    obj_type = obj_type->pointer.pointee;
                }
                
                if (obj_type->kind == TYPE_STRUCT || obj_type->kind == TYPE_INTERFACE) {
                    Method* method = type_find_method(obj_type, fa->field);
                    if (method) {
                        is_method_call = true;
                    }
                }
                
                // НОВОЕ: Обработка вызовов методов на generic параметрах (через where clauses)
                if (obj_type->kind == TYPE_GENERIC_PARAM) {
                    // Ищем where clause для этого generic параметра
                    for (int i = 0; i < a->current_where_clause_count; i++) {
                        if (string_equals(a->current_where_clauses[i].type_param, 
                                         obj_type->generic_param.name)) {
                            // Нашли where clause — это вызов метода
                            is_method_call = true;
                            break;
                        }
                    }
                }
            }

            // Проверка количества аргументов
            int expected_args = callee_type->function.param_count;
            if (is_method_call) {
                expected_args--;  // self не считается аргументом
            }
            
            if (is_generic_call) {
                // Для generic вызова: первый аргумент — тип, остальные — значения
                // Ожидаем: 1 (тип) + expected_args (значения)
                int expected_total = 1 + expected_args;
                if (call->arg_count != expected_total) {
                    error_at_node(a, node, "Generic call: expected %d arguments (1 type + %d values), got %d",
                                 expected_total, expected_args, call->arg_count);
                    return type_error_new(a->arena);
                }
                
                // TODO: Инстанциация функции с concrete_type
                // Пока просто пропускаем проверку типов аргументов
                // (полная мономорфизация будет реализована позже)
                
                // Возвращаем тип возврата функции (с подстановкой T → concrete_type)
                // Пока используем оригинальный тип возврата
                Type* return_type = callee_type->function.return_type;
                
                // Если тип возврата — generic параметр, подставляем concrete_type
                if (return_type->kind == TYPE_GENERIC_PARAM) {
                    return concrete_type;
                }
                
                return return_type;
                
            } else {
                // Обычный вызов
                if (call->arg_count != expected_args) {
                    error_at_node(a, node, "Expected %d arguments, got %d",
                                 expected_args, call->arg_count);
                    return type_error_new(a->arena);
                }
                
                // Проверка типов аргументов
                int param_offset = is_method_call ? 1 : 0;
                for (int i = 0; i < call->arg_count; i++) {
                    Type* arg_type = analyze_expression(a, call->args[i]);
                    Type* param_type = callee_type->function.param_types[i + param_offset];
                    
                    if (!types_assignable(param_type, arg_type)) {
                        error_at_node(a, node, "Argument %d: expected %s, got %s",
                                     i + 1,
                                     type_to_string(a->arena, param_type),
                                     type_to_string(a->arena, arg_type));
                    }
                }
                
                return callee_type->function.return_type;
            }
        }

        case AST_FIELD_ACCESS: {
            FieldAccessNode* fa = (FieldAccessNode*)node;
            
            // ПЕРВЫМ ДЕЛОМ: Проверяем, не является ли это обращением к значению enum
            if (fa->object && fa->object->type == AST_IDENTIFIER) {
                IdentifierNode* id = (IdentifierNode*)fa->object;
                Symbol* sym = symbol_lookup(&a->symbols, id->name);
                
                if (sym && sym->kind == SYMBOL_ENUM && sym->type && 
                    sym->type->kind == TYPE_ENUM) {
                    // Это значение enum: Status.Ok
                    Type* enum_type = sym->type;
                    
                    // Ищем значение в списке enum
                    for (int i = 0; i < enum_type->enumeration.value_count; i++) {
                        if (string_equals(enum_type->enumeration.value_names[i], fa->field)) {
                            // Найдено! Возвращаем базовый тип enum
                            return enum_type->enumeration.base_type;
                        }
                    }
                    
                    error_at_node(a, node, "Enum '%.*s' has no value '%.*s'.",
                                 (int)enum_type->enumeration.name.length,
                                 enum_type->enumeration.name.data,
                                 (int)fa->field.length, fa->field.data);
                    return type_error_new(a->arena);
                }
            }

            Type* obj_type = analyze_expression(a, fa->object);
            
            if (obj_type->kind == TYPE_ERROR) return type_error_new(a->arena);
            
            // НОВОЕ: Специальная обработка .len для срезов и массивов
            if (string_equals(fa->field, STRING_FROM_LITERAL("len"))) {
                if (obj_type->kind == TYPE_SLICE || obj_type->kind == TYPE_ARRAY) {
                    return type_int(64, true);  // s.len и arr.len возвращают i64
                }
            }
            
            // Если это указатель, разыменовываем
            if (obj_type->kind == TYPE_POINTER) {
                obj_type = obj_type->pointer.pointee;
            }

            // НОВОЕ: Обработка доступа к методам интерфейса (s.area())
            if (obj_type->kind == TYPE_INTERFACE) {
                Method* method = type_find_method(obj_type, fa->field);
                if (method) {
                    return method->function_type;
                }
                error_at_node(a, node, "Interface '%.*s' has no method '%.*s'",
                             (int)obj_type->interface.name.length,
                             obj_type->interface.name.data,
                             (int)fa->field.length, fa->field.data);
                return type_error_new(a->arena);
            }

            // НОВОЕ: Обработка доступа к методам generic параметров через where clauses
            if (obj_type->kind == TYPE_GENERIC_PARAM) {
                Type* interface_type = NULL;
                
                for (int i = 0; i < a->current_where_clause_count; i++) {
                    if (string_equals(a->current_where_clauses[i].type_param, 
                                     obj_type->generic_param.name)) {
                        String iface_name = a->current_where_clauses[i].interface_name;
                        interface_type = symbol_lookup_type(&a->symbols, iface_name);
                        
                        if (!interface_type || interface_type->kind != TYPE_INTERFACE) {
                            error_at_node(a, node, "Where clause references undefined interface '%.*s'",
                                         (int)iface_name.length, iface_name.data);
                            return type_error_new(a->arena);
                        }
                        break;
                    }
                }
                
                if (!interface_type) {
                    error_at_node(a, node, "Generic parameter '%.*s' has no where clause for method access",
                                 (int)obj_type->generic_param.name.length,
                                 obj_type->generic_param.name.data);
                    return type_error_new(a->arena);
                }
                
                Method* method = type_find_method(interface_type, fa->field);
                if (!method) {
                    error_at_node(a, node, "Interface '%.*s' has no method '%.*s'",
                                 (int)interface_type->interface.name.length,
                                 interface_type->interface.name.data,
                                 (int)fa->field.length, fa->field.data);
                    return type_error_new(a->arena);
                }
                
                return method->function_type;
            }
            
            if (obj_type->kind != TYPE_STRUCT) {
                error_at_node(a, node, "Field access on non-struct type");
                return type_error_new(a->arena);
            }

            // НОВОЕ: Сначала ищем метод (для вызовов типа obj.method())
            Method* method = type_find_method(obj_type, fa->field);
            if (method) {
                // Это вызов метода! Возвращаем тип функции метода
                return method->function_type;
            }
            
            Field* field = type_find_field(obj_type, fa->field);
            if (!field) {
                error_at_node(a, node, "Struct '%.*s' has no field '%.*s'",
                             (int)obj_type->structure.name.length,
                             obj_type->structure.name.data,
                             (int)fa->field.length, fa->field.data);
                return type_error_new(a->arena);
            }
            
            return field->type;
        }

        case AST_ADDR_OF: {
            AddrOfNode* addr = (AddrOfNode*)node;
            Type* operand_type = analyze_expression(a, addr->operand);
            if (operand_type->kind == TYPE_ERROR) return type_error_new(a->arena);
            
            // Оператор & превращает тип T в тип *T
            return type_pointer_new(a->arena, operand_type);
        }
        
        case AST_DEREF: {
            DerefNode* deref = (DerefNode*)node;
            Type* operand_type = analyze_expression(a, deref->operand);
            if (operand_type->kind == TYPE_ERROR) return type_error_new(a->arena);
            
            // Оператор * требует, чтобы операнд был указателем
            if (operand_type->kind != TYPE_POINTER) {
                error_at_node(a, node, "Cannot dereference non-pointer type '%s'", 
                              type_to_string(a->arena, operand_type));
                return type_error_new(a->arena);
            }
            
            // Разыменование указателя *T дает тип T
            return operand_type->pointer.pointee;
        }

        case AST_ARRAY_LITERAL: {
            ArrayLiteralNode* arr = (ArrayLiteralNode*)node;
            
            if (arr->element_count == 0) {
                error_at_node(a, node, "Empty array literal requires explicit type.");
                return type_error_new(a->arena);
            }
            
            // Определяем тип элементов по первому элементу
            Type* element_type = analyze_expression(a, arr->elements[0]);
            if (element_type->kind == TYPE_ERROR) return type_error_new(a->arena);
            
            // Проверяем, что все элементы одного типа
            for (int i = 1; i < arr->element_count; i++) {
                Type* t = analyze_expression(a, arr->elements[i]);
                if (!type_equals(t, element_type)) {
                    error_at_node(a, node, "All array elements must have the same type.");
                    return type_error_new(a->arena);
                }
            }
            
            // Возвращаем тип массива с известным размером
            return type_array_new(a->arena, element_type, arr->element_count);
        }
        
        case AST_INDEX_ACCESS: {
            IndexAccessNode* idx = (IndexAccessNode*)node;
            
            // Анализируем массив/срез
            Type* array_type = analyze_expression(a, idx->array);
            if (array_type->kind == TYPE_ERROR) return type_error_new(a->arena);
            
            // Анализируем индекс
            Type* index_type = analyze_expression(a, idx->index);
            if (index_type->kind == TYPE_ERROR) return type_error_new(a->arena);
            
            // Проверяем, что индекс - целое число
            if (index_type->kind != TYPE_INT && index_type->kind != TYPE_UINT) {
                error_at_node(a, node, "Array index must be an integer.");
                return type_error_new(a->arena);
            }
            
            // Проверяем, что левая часть - массив или срез
            if (array_type->kind == TYPE_ARRAY) {
                return array_type->array.element;
            } else if (array_type->kind == TYPE_SLICE) {
                return array_type->slice.element;
            } else if (array_type->kind == TYPE_POINTER) {
                // Указатель тоже можно индексировать: ptr[i] == *(ptr + i)
                return array_type->pointer.pointee;
            } else {
                error_at_node(a, node, "Cannot index non-array type.");
                return type_error_new(a->arena);
            }
        }

        case AST_ENUM_DECL: {
            EnumDeclNode* e = (EnumDeclNode*)node;
            
            // Разрешаем базовый тип
            Type* base_type = resolve_type(a, e->base_type);
            
            // Подготавливаем массивы значений
            String* value_names = NULL;
            long long* value_values = NULL;
            if (e->value_count > 0) {
                value_names = ARENA_ARRAY(a->arena, String, e->value_count);
                value_values = ARENA_ARRAY(a->arena, long long, e->value_count);
                for (int i = 0; i < e->value_count; i++) {
                    value_names[i] = e->values[i].name;
                    value_values[i] = e->values[i].value;
                }
            }
            
            Type* enum_type = type_enum_new(a->arena, e->name, base_type,
                                           value_names, value_values, e->value_count);
            
            // Регистрируем тип в таблице символов
            if (!symbol_define_type(&a->symbols, e->name, enum_type, node)) {
                error_at_node(a, node, "Enum '%.*s' already defined.",
                             (int)e->name.length, e->name.data);
                return type_error_new(a->arena);
            }
            
            return enum_type;
        }

        default:
            error_at_node(a, node, "Unsupported expression type");
            return type_error_new(a->arena);
    }
}

// ===== Анализ операторов =====

static void analyze_statement(Analyzer* a, ASTNode* node) {
    if (!node) return;
    
    switch (node->type) {
        case AST_BLOCK: {
            BlockNode* block = (BlockNode*)node;
            scope_enter(&a->symbols);
            for (int i = 0; i < block->statement_count; i++) {
                analyze_statement(a, block->statements[i]);
            }
            scope_leave(&a->symbols);
            break;
        }
        
        case AST_VAR_DECL: {
            VarDeclNode* var = (VarDeclNode*)node;
            Type* declared_type = resolve_type(a, var->type);
            
            if (var->initializer) {
                Type* init_type = analyze_expression(a, var->initializer);
                if (!types_assignable(declared_type, init_type)) {
                    error_at_node(a, node, "Cannot initialize %s with %s",
                                 type_to_string(a->arena, declared_type),
                                 type_to_string(a->arena, init_type));
                }
            }
            
            if (!symbol_define(&a->symbols, var->name, SYMBOL_VARIABLE, 
                              declared_type, node)) {
                error_at_node(a, node, "Variable '%.*s' already declared in this scope",
                             (int)var->name.length, var->name.data);
            }
            break;
        }
        
        case AST_RETURN: {
            ReturnNode* ret = (ReturnNode*)node;
            
            if (!a->current_return_type) {
                error_at_node(a, node, "Return statement outside of function");
                break;
            }
            
            if (ret->value) {
                Type* val_type = analyze_expression(a, ret->value);
                if (!types_assignable(a->current_return_type, val_type)) {
                    error_at_node(a, node, "Cannot return %s from function expecting %s",
                                 type_to_string(a->arena, val_type),
                                 type_to_string(a->arena, a->current_return_type));
                }
            } else {
                if (a->current_return_type->kind != TYPE_VOID) {
                    error_at_node(a, node, "Non-void function must return a value");
                }
            }
            break;
        }
        
        case AST_IF: {
            IfNode* if_node = (IfNode*)node;
            Type* cond_type = analyze_expression(a, if_node->condition);
            
            if (cond_type->kind != TYPE_BOOL) {
                error_at_node(a, node, "If condition must be bool, got %s",
                             type_to_string(a->arena, cond_type));
            }
            
            analyze_statement(a, if_node->then_branch);
            if (if_node->else_branch) {
                analyze_statement(a, if_node->else_branch);
            }
            break;
        }
        
        case AST_FOR: {
            ForNode* f = (ForNode*)node;
            if (f->init) analyze_statement(a, f->init);
            if (f->condition) {
                Type* cond_type = analyze_expression(a, f->condition);
                if (cond_type->kind != TYPE_BOOL && cond_type->kind != TYPE_ERROR) {
                    error_at_node(a, node, "For condition must be 'bool', got '%s'", 
                                  type_to_string(a->arena, cond_type));
                }
            }
            if (f->increment) analyze_expression(a, f->increment);
            a->in_loop_depth++;
            analyze_statement(a, f->body);
            a->in_loop_depth--;
            return;
        }
        
        case AST_WHILE: {
            WhileNode* w = (WhileNode*)node;
            Type* cond_type = analyze_expression(a, w->condition);
            if (cond_type->kind != TYPE_BOOL && cond_type->kind != TYPE_ERROR) {
                error_at_node(a, node, "While condition must be 'bool', got '%s'", 
                              type_to_string(a->arena, cond_type));
            }
            a->in_loop_depth++;  // Входим в цикл
            analyze_statement(a, w->body);
            a->in_loop_depth--;  // Выходим из цикла
            return;  // void функция - просто return
        }
        case AST_BREAK:
            if (a->in_loop_depth <= 0) {
                error_at_node(a, node, "'break' can only be used inside a loop");
            }
            return;
        case AST_CONTINUE:
            if (a->in_loop_depth <= 0) {
                error_at_node(a, node, "'continue' can only be used inside a loop");
            }
            return;
        
        default:
            // Выражение как statement
            analyze_expression(a, node);
            break;
    }
}

// ===== Анализ объявлений =====

static void analyze_declaration(Analyzer* a, ASTNode* node) {
    if (!node) return;
    
    switch (node->type) {
        case AST_STRUCT_DECL: {
            StructDeclNode* s = (StructDeclNode*)node;
            Type* struct_type = type_struct_new(a->arena, s->name);
            
            // НОВОЕ: Сохраняем информацию о дженериках
            if (s->is_generic) {
                struct_type->structure.is_generic = true;
                struct_type->structure.generic_params = s->generic_params;
                struct_type->structure.generic_param_count = s->generic_param_count;
            }
            
            // Регистрируем тип до анализа полей (для рекурсивных типов)
            if (!symbol_define_type(&a->symbols, s->name, struct_type, node)) {
                error_at_node(a, node, "Type '%.*s' already defined",
                             (int)s->name.length, s->name.data);
                break;
            }
            
            // НОВОЕ: Устанавливаем контекст дженериков для анализа полей
            struct GenericParamNode* old_gp = a->current_generic_params;
            int old_gc = a->current_generic_param_count;
            
            if (s->is_generic) {
                a->current_generic_params = s->generic_params;
                a->current_generic_param_count = s->generic_param_count;
            }
            
            // Добавляем поля
            for (int i = 0; i < s->field_count; i++) {
                Type* field_type = resolve_type(a, s->fields[i].type);
                type_add_field(a->arena, struct_type, s->fields[i].name, field_type);
            }
            
            // НОВОЕ: Восстанавливаем контекст
            a->current_generic_params = old_gp;
            a->current_generic_param_count = old_gc;
            
            struct_type->structure.is_defined = true;
            break;
        }
        
        case AST_INTERFACE_DECL: {
            InterfaceDeclNode* iface = (InterfaceDeclNode*)node;
            Type* iface_type = type_interface_new(a->arena, iface->name);
            
            if (!symbol_define_type(&a->symbols, iface->name, iface_type, node)) {
                error_at_node(a, node, "Type '%.*s' already defined",
                             (int)iface->name.length, iface->name.data);
                break;
            }
            
            // Добавляем методы интерфейса
            // Добавляем методы интерфейса с правильными сигнатурами
            for (int i = 0; i < iface->method_count; i++) {
                Type* ret_type = resolve_type(a, iface->methods[i].return_type);
                
                // Парсер уже добавил self как первый параметр
                int param_count = iface->methods[i].param_count;
                Type** param_types = ARENA_ARRAY(a->arena, Type*, param_count);
                String* param_names = ARENA_ARRAY(a->arena, String, param_count);
                
                for (int j = 0; j < param_count; j++) {
                    param_types[j] = resolve_type(a, iface->methods[i].params[j].type);
                    param_names[j] = iface->methods[i].params[j].name;
                    
                    // Если это self, делаем его указателем на интерфейс
                    if (string_equals(iface->methods[i].params[j].name, 
                                     STRING_FROM_LITERAL("self"))) {
                        param_types[j] = type_pointer_new(a->arena, iface_type);
                    }
                }
                
                Type* func_type = type_function_new(a->arena, ret_type, param_types, 
                                                    param_names, param_count);
                type_add_method(a->arena, iface_type, iface->methods[i].name, func_type);
            }
            break;
        }
        
        case AST_METHODS_BLOCK: {
            MethodsBlockNode* mb = (MethodsBlockNode*)node;
            
            // Ищем тип, к которому привязаны методы
            Type* target_type = symbol_lookup_type(&a->symbols, mb->type_name);
            if (!target_type) {
                error_at_node(a, node, "Methods for undefined type '%.*s'",
                             (int)mb->type_name.length, mb->type_name.data);
                break;
            }
            
            // Анализируем каждый метод
            for (int i = 0; i < mb->method_count; i++) {
                FunctionDeclNode* method = &mb->methods[i];
                
                Type* ret_type = resolve_type(a, method->return_type);
                
                // Парсер уже добавил self как первый параметр!
                // Просто разрешаем типы параметров и заменяем тип self на указатель
                Type** param_types = ARENA_ARRAY(a->arena, Type*, method->param_count);
                String* param_names = ARENA_ARRAY(a->arena, String, method->param_count);
                
                for (int j = 0; j < method->param_count; j++) {
                    param_types[j] = resolve_type(a, method->params[j].type);
                    param_names[j] = method->params[j].name;
                    
                    // НОВОЕ: Если это параметр self, делаем его указателем
                    if (string_equals(method->params[j].name, STRING_FROM_LITERAL("self"))) {
                        param_types[j] = type_pointer_new(a->arena, target_type);
                    }
                }
                
                Type* func_type = type_function_new(a->arena, ret_type, param_types, param_names, method->param_count);
                
                // Регистрируем метод в типе
                type_add_method(a->arena, target_type, method->name, func_type);
                
                // Анализируем тело метода
                scope_enter(&a->symbols);
                Type* old_return = a->current_return_type;
                Type* old_struct = a->current_struct_type;
                a->current_return_type = ret_type;
                a->current_struct_type = target_type;
                
                // НОВОЕ: Регистрируем параметры метода как переменные
                for (int j = 0; j < method->param_count; j++) {
                    if (!symbol_define(&a->symbols, param_names[j], SYMBOL_VARIABLE,
                                      param_types[j], (ASTNode*)method)) {
                        error_at_node(a, (ASTNode*)method, "Parameter '%.*s' already declared",
                                     (int)param_names[j].length, param_names[j].data);
                    }
                }
                
                analyze_statement(a, method->body);
                
                a->current_return_type = old_return;
                a->current_struct_type = old_struct;
                scope_leave(&a->symbols);
            }
            break;
        }
        
        case AST_FUNCTION_DECL: {
            FunctionDeclNode* func = (FunctionDeclNode*)node;
            
            // Сохраняем старый контекст generic параметров
            struct GenericParamNode* old_generic_params = a->current_generic_params;
            int old_generic_count = a->current_generic_param_count;
            
            // Сохраняем старый контекст where clauses
            struct WhereClauseNode* old_where = a->current_where_clauses;
            int old_where_count = a->current_where_clause_count;
            
            // Устанавливаем новый контекст, если функция generic
            if (func->is_generic) {
                a->current_generic_params = func->generic_params;
                a->current_generic_param_count = func->generic_param_count;
                a->current_where_clauses = func->where_clauses;
                a->current_where_clause_count = func->where_clause_count;
            }
            
            Type* ret_type = resolve_type(a, func->return_type);
            
            // Собираем типы и имена параметров
            Type** param_types = NULL;
            String* param_names = NULL;
            if (func->param_count > 0) {
                param_types = ARENA_ARRAY(a->arena, Type*, func->param_count);
                param_names = ARENA_ARRAY(a->arena, String, func->param_count);
                for (int i = 0; i < func->param_count; i++) {
                    param_types[i] = resolve_type(a, func->params[i].type);
                    param_names[i] = func->params[i].name;
                }
            }
            
            Type* func_type = type_function_new(a->arena, ret_type, param_types, param_names, func->param_count);
            
            // Регистрируем функцию
            if (!symbol_define(&a->symbols, func->name, SYMBOL_FUNCTION, 
                              func_type, node)) {
                error_at_node(a, node, "Function '%.*s' already declared",
                             (int)func->name.length, func->name.data);
                // Восстанавливаем контекст перед выходом
                a->current_generic_params = old_generic_params;
                a->current_generic_param_count = old_generic_count;
                break;
            }
            
            // Анализируем тело функции
            scope_enter(&a->symbols);
            Type* old_return = a->current_return_type;
            a->current_return_type = ret_type;
            
            // Регистрируем параметры как переменные в scope функции
            for (int i = 0; i < func->param_count; i++) {
                if (!symbol_define(&a->symbols, func->params[i].name, SYMBOL_VARIABLE,
                                  param_types[i], node)) {
                    error_at_node(a, node, "Parameter '%.*s' already declared",
                                 (int)func->params[i].name.length,
                                 func->params[i].name.data);
                }
            }
            
            analyze_statement(a, func->body);
            
            a->current_return_type = old_return;
            scope_leave(&a->symbols);
            
            // Восстанавливаем контекст generic параметров
            a->current_generic_params = old_generic_params;
            a->current_generic_param_count = old_generic_count;
            
            // ДОБАВЬТЕ ЭТИ ДВЕ СТРОКИ:
            a->current_where_clauses = old_where;
            a->current_where_clause_count = old_where_count;
            break;
        }
        
        case AST_IMPORT:
            // Пока игнорируем импорты
            break;
        
        default:
            // Это может быть statement верхнего уровня
            analyze_statement(a, node);
            break;
    }
}

// ===== Главный интерфейс =====
void analyzer_init(Analyzer* a, Arena* arena, ErrorReporter* errors) {
    a->arena = arena;
    a->errors = errors;
    a->error_count = 0;
    a->in_loop_depth = 0;
    a->current_return_type = NULL;
    a->current_struct_type = NULL;
    a->current_generic_params = NULL;
    a->current_generic_param_count = 0;
    
    // ДОБАВЬТЕ ЭТИ ДВЕ СТРОКИ:
    a->current_where_clauses = NULL;
    a->current_where_clause_count = 0;
    
    symbol_table_init(&a->symbols, arena);
    types_init(arena);
}

bool analyzer_analyze(Analyzer* a, ASTNode* ast) {
    if (!ast) return false;   // <-- ИСПРАВЛЕНО: было return;
    
    // Предполагаем, что корень — это BlockNode со списком деклараций
    if (ast->type != AST_BLOCK) {
        // Если это не блок, просто анализируем как единственную декларацию
        analyze_declaration(a, ast);
        return a->error_count == 0;
    }
    
    BlockNode* block = (BlockNode*)ast;
    
    // ===== ПРОХОД 1: Регистрируем все типы (struct, enum, interface) =====
    for (int i = 0; i < block->statement_count; i++) {
        ASTNode* node = block->statements[i];
        if (!node) continue;
        
        if (node->type == AST_STRUCT_DECL || 
            node->type == AST_ENUM_DECL || 
            node->type == AST_INTERFACE_DECL) {
            analyze_declaration(a, node);
        }
    }
    
    // ===== ПРОХОД 2: Анализируем функции и остальные декларации =====
    for (int i = 0; i < block->statement_count; i++) {
        ASTNode* node = block->statements[i];
        if (!node) continue;
        
        // Пропускаем типы — они уже обработаны в первом проходе
        if (node->type == AST_STRUCT_DECL || 
            node->type == AST_ENUM_DECL || 
            node->type == AST_INTERFACE_DECL) {
            continue;
        }
        
        analyze_declaration(a, node);
    }
    
    return a->error_count == 0;
}

int analyzer_error_count(Analyzer* a) {
    return a->error_count;
}
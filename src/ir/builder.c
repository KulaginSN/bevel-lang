// src/ir/builder.c
#include "ir/builder.h"
#include "common/utils.h"
#include <stdio.h>
#include <stdarg.h>
#include <string.h>

// ===== Вспомогательные функции =====

static void builder_error(IRBuilder* b, int line, int col, const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    char buffer[512];
    vsnprintf(buffer, sizeof(buffer), fmt, args);
    va_end(args);
    
    error_reporter_semantic_error(b->errors, line, col, buffer);
    b->error_count++;
}

static IRValue* create_temp(IRBuilder* b, Type* type) {
    int id = b->current_function->next_temp_id++;
    return ir_value_temp(b->arena, id, type);
}

static String generate_block_label(IRBuilder* b, const char* prefix) {
    static int block_counter = 0;
    char* cstr = format_string(b->arena, "%s%d", prefix, block_counter++);
    return string_new(cstr, strlen(cstr));
}

// ===== Управление областями видимости =====

static void ir_scope_enter(IRBuilder* b) {
    IRScope* scope = ARENA_ALLOC(b->arena, IRScope);
    scope->symbol_count = 0;
    scope->parent = b->current_scope;
    b->current_scope = scope;
}

static void ir_scope_leave(IRBuilder* b) {
    if (b->current_scope && b->current_scope->parent) {
        b->current_scope = b->current_scope->parent;
    }
}

static void ir_scope_define(IRBuilder* b, String name, IRValue* value) {
    
    if (!b->current_scope || b->current_scope->symbol_count >= 256) {
        return;
    }
    b->current_scope->symbols[b->current_scope->symbol_count].name = name;
    b->current_scope->symbols[b->current_scope->symbol_count].value = value;
    b->current_scope->symbol_count++;
}

static IRValue* ir_scope_lookup(IRBuilder* b, String name) {
    for (IRScope* s = b->current_scope; s; s = s->parent) {
        for (int i = 0; i < s->symbol_count; i++) {
            if (string_equals(s->symbols[i].name, name)) {
                return s->symbols[i].value;
            }
        }
    }
    return NULL;
}

// ===== Forward declarations =====

static IRValue* build_expression(IRBuilder* b, ASTNode* node);
static void build_statement(IRBuilder* b, ASTNode* node);
static void build_declaration(IRBuilder* b, ASTNode* node);
static void build_function(IRBuilder* b, FunctionDeclNode* func, bool is_method, Type* struct_type);

// ===== Преобразование AST типа в семантический тип =====

static Type* resolve_ast_type(IRBuilder* b, ASTNode* type_node) {
    if (!type_node) return type_void();
    
    // Обработка указателей
    if (type_node->type == AST_POINTER_TYPE) {
        PointerTypeNode* pt = (PointerTypeNode*)type_node;
        Type* base_type = resolve_ast_type(b, pt->base_type);
        if (base_type->kind == TYPE_ERROR) return type_error_new(b->arena);
        return type_pointer_new(b->arena, base_type);
    }
    
    // Обработка массивов и срезов
    if (type_node->type == AST_ARRAY_TYPE) {
        ArrayTypeNode* at = (ArrayTypeNode*)type_node;
        Type* element_type = resolve_ast_type(b, at->element_type);
        
        if (at->is_slice) {
            return type_slice_new(b->arena, element_type);
        } else {
            int size = 0;
            if (at->size && at->size->type == AST_INT_LITERAL) {
                IntLiteralNode* lit = (IntLiteralNode*)at->size;
                size = (int)lit->value;
            } else {
                builder_error(b, type_node->line, type_node->column, 
                            "Array size must be a constant integer.");
                size = 0;
            }
            return type_array_new(b->arena, element_type, size);
        }
    }
    
    if (type_node->type != AST_TYPE) return type_void();
    
    TypeNode* tn = (TypeNode*)type_node;
    
    switch (tn->token_type) {
        case TOKEN_VOID:       return type_void();
        case TOKEN_BOOL_TYPE:  return type_bool();
        case TOKEN_CHAR_TYPE:  return type_char();
        case TOKEN_STRING_TYPE: return type_string();
        case TOKEN_I8:   return type_int(8, true);
        case TOKEN_I16:  return type_int(16, true);
        case TOKEN_I32:  return type_int(32, true);
        case TOKEN_I64:  return type_int(64, true);
        case TOKEN_U8:   return type_int(8, false);
        case TOKEN_U16:  return type_int(16, false);
        case TOKEN_U32:  return type_int(32, false);
        case TOKEN_U64:  return type_int(64, false);
        case TOKEN_F32: case TOKEN_SINGLE:   return type_float(32);
        case TOKEN_F64: case TOKEN_DOUBLE:   return type_float(64);
        case TOKEN_F80: case TOKEN_EXTENDED: return type_float(80);
        case TOKEN_F128: case TOKEN_QUAD:    return type_float(128);
        case TOKEN_INT_TYPE: return type_int(32, true);
        
        case TOKEN_IDENTIFIER: {
            // Проверяем, есть ли type_args (инстанциация generic типа)
            if (tn->type_arg_count > 0) {
                // Формируем манглированное имя: Result_f64_string
                String mangled_name = tn->name;
                for (int i = 0; i < tn->type_arg_count; i++) {
                    mangled_name = string_concat(b->arena, mangled_name, STRING_FROM_LITERAL("_"));
                    
                    ASTNode* arg = tn->type_args[i];
                    String arg_name = STRING_FROM_LITERAL("unknown");
                    
                    if (arg && arg->type == AST_TYPE) {
                        TypeNode* arg_tn = (TypeNode*)arg;
                        arg_name = arg_tn->name;
                    }
                    
                    mangled_name = string_concat(b->arena, mangled_name, arg_name);
                }
                
                // Ищем инстанцированный тип по манглированному имени
                Type* inst_type = symbol_lookup_type(&b->analyzer->symbols, mangled_name);
                if (inst_type) {
                    return tn->is_pointer ? type_pointer_new(b->arena, inst_type) : inst_type;
                }
                
                // Если не нашли, fallback на обычный поиск
            }
            
            // Обычный поиск типа
            Type* user_type = symbol_lookup_type(&b->analyzer->symbols, tn->name);
            if (!user_type) return type_void();
            return tn->is_pointer ? type_pointer_new(b->arena, user_type) : user_type;
        }
        
        default: return type_void();
    }
}

// ===== Преобразование AST оператора в IR opcode =====

static IROpcode ast_op_to_ir(TokenType op) {
    switch (op) {
        case TOKEN_PLUS: return IR_OP_ADD;
        case TOKEN_MINUS: return IR_OP_SUB;
        case TOKEN_STAR: return IR_OP_MUL;
        case TOKEN_SLASH: return IR_OP_DIV;
        case TOKEN_PERCENT: return IR_OP_MOD;
        case TOKEN_EQEQ: return IR_OP_EQ;
        case TOKEN_NOTEQ: return IR_OP_NE;
        case TOKEN_LT: return IR_OP_LT;
        case TOKEN_LTE: return IR_OP_LE;
        case TOKEN_GT: return IR_OP_GT;
        case TOKEN_GTE: return IR_OP_GE;
        case TOKEN_AND: return IR_OP_AND;
        case TOKEN_OR: return IR_OP_OR;
        default: return IR_OP_ADD;
    }
}

// ===== Построение выражений =====

static IRValue* build_expression(IRBuilder* b, ASTNode* node) {
    if (!node) return NULL;
    
    switch (node->type) {
        case AST_INT_LITERAL: {
            IntLiteralNode* lit = (IntLiteralNode*)node;
            return ir_value_const_int(b->arena, lit->value, type_int(32, true));
        }
        
        case AST_FLOAT_LITERAL: {
            FloatLiteralNode* lit = (FloatLiteralNode*)node;
            return ir_value_const_float(b->arena, lit->value, type_float(64));
        }
        
        case AST_STRING_LITERAL: {
            StringLiteralNode* lit = (StringLiteralNode*)node;
            return ir_value_const_string(b->arena, lit->value);
        }
        
        case AST_BOOL_LITERAL: {
            BoolLiteralNode* lit = (BoolLiteralNode*)node;
            return ir_value_const_bool(b->arena, lit->value);
        }
        
        case AST_IDENTIFIER: {
            IdentifierNode* id = (IdentifierNode*)node;
            
            // ОТЛАДКА: смотрим, что ищем
            
            IRValue* local = ir_scope_lookup(b, id->name);
            if (local) {
                return local;
            }
            
            Symbol* sym = symbol_lookup(&b->analyzer->symbols, id->name);
            if (!sym) {
                builder_error(b, node->line, node->column, 
                            "Undefined variable '%.*s' in IR builder",
                            (int)id->name.length, id->name.data);
                return ir_value_const_int(b->arena, 0, type_int(32, true));
            }
            
            if (sym->kind == SYMBOL_VARIABLE) {
                return ir_value_var(b->arena, id->name, sym->type);
            } else if (sym->kind == SYMBOL_FUNCTION) {
                return ir_value_global(b->arena, id->name, sym->type);
            }
            
            return ir_value_var(b->arena, id->name, sym->type);
        }
        case AST_ASSIGN: {
            AssignNode* assign = (AssignNode*)node;
            IRValue* target = build_expression(b, assign->target);
            IRValue* value = build_expression(b, assign->value);
            
            // СЛУЧАЙ 1: Присваивание простой переменной
            if (target && target->kind == IR_VALUE_VAR) {
                ir_emit_assign(b->arena, b->current_block, target, value);
                return target;
            }
            
            // СЛУЧАЙ 2: Запись через разыменование *ptr = value
            if (assign->target->type == AST_DEREF) {
                DerefNode* deref = (DerefNode*)assign->target;
                IRValue* ptr = build_expression(b, deref->operand);
                IRInstruction* instr = ir_instruction_new(b->arena, IR_OP_STORE);
                instr->operand1 = ptr;
                instr->operand2 = value;
                ir_block_add_instruction(b->current_block, instr);
                return value;
            }
            
            // НОВОЕ СЛУЧАЙ 3: Запись в элемент массива arr[i] = value
            if (assign->target->type == AST_INDEX_ACCESS) {
                IndexAccessNode* idx = (IndexAccessNode*)assign->target;
                IRValue* array = build_expression(b, idx->array);
                IRValue* index = build_expression(b, idx->index);
                
                IRInstruction* instr = ir_instruction_new(b->arena, IR_OP_INDEX_STORE);
                instr->operand1 = array;
                instr->operand2 = index;
                instr->operand3 = value;  // <-- Используем новое поле
                ir_block_add_instruction(b->current_block, instr);
                return value;
            }

            if (assign->target->type == AST_FIELD_ACCESS) {
                FieldAccessNode* fa = (FieldAccessNode*)assign->target;
                IRValue* object = build_expression(b, fa->object);
                IRValue* value = build_expression(b, assign->value);
                
                fprintf(stderr, "[DEBUG IR FIELD_STORE] object kind=%d, field='%.*s', value=%p\n",
                        object ? object->kind : -1,
                        (int)fa->field.length, fa->field.data,
                        (void*)value);
                
                IRInstruction* instr = ir_instruction_new(b->arena, IR_OP_FIELD_STORE);
                instr->operand1 = object;
                instr->operand2 = ir_value_const_string(b->arena, fa->field);
                instr->operand3 = value;
                
                fprintf(stderr, "[DEBUG IR FIELD_STORE] After set: operand3=%p\n", (void*)instr->operand3);
                
                ir_block_add_instruction(b->current_block, instr);
                return value;
            }
            
            builder_error(b, node->line, node->column, "Unsupported assignment target");
            return value;
        }
        case AST_BINARY_EXPR: {
            BinaryExprNode* bin = (BinaryExprNode*)node;
            IRValue* left = build_expression(b, bin->left);
            IRValue* right = build_expression(b, bin->right);
            
            IROpcode op = ast_op_to_ir(bin->op);
            
            Type* result_type = left->type;
            if (right->type && right->type->kind == TYPE_FLOAT && 
                left->type && left->type->kind != TYPE_FLOAT) {
                result_type = right->type;
            }
            
            IRValue* temp = create_temp(b, result_type);
            IRInstruction* instr = ir_emit_binary(b->arena, b->current_block, op, left, right, result_type);
            instr->result = temp;
            
            return temp;
        }
        
        case AST_UNARY_EXPR: {
            UnaryExprNode* un = (UnaryExprNode*)node;
            IRValue* operand = build_expression(b, un->operand);
            
            IROpcode op = (un->op == TOKEN_MINUS) ? IR_OP_SUB : IR_OP_NOT;
            IRValue* temp = create_temp(b, operand->type);
            
            if (op == IR_OP_SUB) {
                IRValue* zero = ir_value_const_int(b->arena, 0, operand->type);
                IRInstruction* instr = ir_emit_binary(b->arena, b->current_block, IR_OP_SUB, zero, operand, operand->type);
                instr->result = temp;
            } else {
                IRInstruction* instr = ir_emit_unary(b->arena, b->current_block, IR_OP_NOT, operand, operand->type);
                instr->result = temp;
            }
            
            return temp;
        }
        
        case AST_CALL_EXPR: {
            CallExprNode* call = (CallExprNode*)node;
            
            // Проверяем вызов метода интерфейса
            bool is_interface_method_call = false;
            IRValue* iface_data = NULL;
            IRValue* iface_vtable = NULL;
            String iface_method_name = STRING_EMPTY;
            Method* iface_method = NULL;
            
            if (call->callee && call->callee->type == AST_FIELD_ACCESS) {
                FieldAccessNode* fa = (FieldAccessNode*)call->callee;
                
                if (fa->object && fa->object->type == AST_IDENTIFIER) {
                    IdentifierNode* id = (IdentifierNode*)fa->object;
                    IRValue* obj_val = ir_scope_lookup(b, id->name);
                    
                    if (obj_val && obj_val->type && obj_val->type->kind == TYPE_INTERFACE) {
                        Method* method = type_find_method(obj_val->type, fa->field);
                        if (method) {
                            is_interface_method_call = true;
                            iface_method_name = fa->field;
                            iface_method = method;
                            
                            String data_name = string_concat(b->arena, id->name, 
                                                            STRING_FROM_LITERAL("_data"));
                            String vtable_name = string_concat(b->arena, id->name, 
                                                              STRING_FROM_LITERAL("_vtable"));
                            
                            iface_data = ir_scope_lookup(b, data_name);
                            iface_vtable = ir_scope_lookup(b, vtable_name);
                        }
                    }
                }
            }
            
            if (is_interface_method_call) {
                IRValue* func_ptr = create_temp(b, iface_method->function_type);
                
                IRInstruction* load_instr = ir_instruction_new(b->arena, IR_OP_FIELD_ACCESS);
                load_instr->operand1 = iface_vtable;
                load_instr->operand2 = ir_value_const_string(b->arena, iface_method_name);
                load_instr->result = func_ptr;
                ir_block_add_instruction(b->current_block, load_instr);
                
                int total_args = call->arg_count + 1;
                IRValue** args = ARENA_ARRAY(b->arena, IRValue*, total_args);
                args[0] = iface_data;
                for (int i = 0; i < call->arg_count; i++) {
                    args[i + 1] = build_expression(b, call->args[i]);
                }
                
                Type* return_type = iface_method->function_type->function.return_type;
                IRValue* temp = NULL;
                if (return_type->kind != TYPE_VOID) {
                    temp = create_temp(b, return_type);
                }
                
                IRInstruction* instr = ir_emit_call(b->arena, b->current_block, 
                                                   func_ptr, args, total_args, return_type);
                instr->result = temp;
                return temp;
            }
            
            // Проверяем вызов метода структуры
            bool is_method_call = false;
            IRValue* self_value = NULL;
            String method_name = STRING_EMPTY;
            Type* method_type = NULL;
            Type* method_obj_type = NULL;
            
            if (call->callee && call->callee->type == AST_FIELD_ACCESS) {
                FieldAccessNode* fa = (FieldAccessNode*)call->callee;
                
                IRValue* obj_value = build_expression(b, fa->object);
                Type* obj_type = obj_value ? obj_value->type : NULL;
                
                if (obj_type && obj_type->kind == TYPE_POINTER) {
                    obj_type = obj_type->pointer.pointee;
                }
                
                if (obj_type && obj_type->kind == TYPE_STRUCT) {
                    Method* method = type_find_method(obj_type, fa->field);
                    
                    if (method) {
                        fprintf(stderr, "[DEBUG IR BUILD] Found method '%.*s' on struct '%.*s'\n",
                                (int)fa->field.length, fa->field.data,
                                (int)obj_type->structure.name.length, obj_type->structure.name.data);
                        
                        is_method_call = true;
                        method_name = fa->field;
                        method_type = method->function_type;
                        method_obj_type = obj_type;
                        
                        Type* ptr_type = type_pointer_new(b->arena, obj_type);
                        self_value = create_temp(b, ptr_type);
                        
                        IRInstruction* addr_instr = ir_instruction_new(b->arena, IR_OP_ADDR_OF);
                        addr_instr->operand1 = obj_value;
                        addr_instr->result = self_value;
                        ir_block_add_instruction(b->current_block, addr_instr);
                    }
                }
            }
            
            // Определяем функцию для вызова
            IRValue* callee = NULL;
            IRValue** args = NULL;
            int total_args = call->arg_count;
            
            if (is_method_call) {
                // Ищем функцию метода по манглированному имени
                String mangled_name;
                if (method_obj_type && method_obj_type->kind == TYPE_STRUCT) {
                    mangled_name = string_concat3(b->arena,
                                                 method_obj_type->structure.name,
                                                 STRING_FROM_LITERAL("_"),
                                                 method_name);
                } else {
                    mangled_name = method_name;
                }
                
                fprintf(stderr, "[DEBUG IR BUILD] Searching for IR function '%.*s'...\n",
                        (int)mangled_name.length, mangled_name.data);
                
                callee = ARENA_ALLOC(b->arena, IRValue);
                callee->kind = IR_VALUE_GLOBAL;
                callee->name = mangled_name;
                callee->type = method_type;
                total_args++;
                
                args = ARENA_ARRAY(b->arena, IRValue*, total_args);
                args[0] = self_value;
                for (int i = 0; i < call->arg_count; i++) {
                    args[i + 1] = build_expression(b, call->args[i]);
                }
            } else {
                callee = build_expression(b, call->callee);
                
                if (call->arg_count > 0) {
                    args = ARENA_ARRAY(b->arena, IRValue*, call->arg_count);
                    for (int i = 0; i < call->arg_count; i++) {
                        args[i] = build_expression(b, call->args[i]);
                    }
                }
            }
            
            Type* return_type = type_void();
            if (callee && callee->type && callee->type->kind == TYPE_FUNCTION) {
                return_type = callee->type->function.return_type;
            } else if (is_method_call && method_type) {
                return_type = method_type->function.return_type;
            }
            
            IRValue* temp = NULL;
            if (return_type->kind != TYPE_VOID) {
                temp = create_temp(b, return_type);
            }
            
            IRInstruction* instr = ir_emit_call(b->arena, b->current_block, callee, args, total_args, return_type);
            instr->result = temp;
            
            return temp;
        }
        
        case AST_FIELD_ACCESS: {
            FieldAccessNode* fa = (FieldAccessNode*)node;
            
            // НОВОЕ: Проверяем, не является ли это значением enum
            if (fa->object && fa->object->type == AST_IDENTIFIER) {
                IdentifierNode* id = (IdentifierNode*)fa->object;
                Symbol* sym = symbol_lookup(&b->analyzer->symbols, id->name);
                
                if (sym && sym->kind == SYMBOL_ENUM && sym->type && 
                    sym->type->kind == TYPE_ENUM) {
                    // Это значение enum: Status.Ok
                    Type* enum_type = sym->type;
                    
                    // Ищем значение и возвращаем константу
                    for (int i = 0; i < enum_type->enumeration.value_count; i++) {
                        if (string_equals(enum_type->enumeration.value_names[i], fa->field)) {
                            return ir_value_const_int(b->arena, 
                                                     enum_type->enumeration.value_values[i],
                                                     enum_type->enumeration.base_type);
                        }
                    }
                    
                    builder_error(b, fa->base.line, fa->base.column,
                                "Enum value not found.");
                    return ir_value_const_int(b->arena, 0, enum_type->enumeration.base_type);
                }
            }
            IRValue* object = build_expression(b, fa->object);
            
            // Специальная обработка для .len у срезов и массивов
            if (string_equals(fa->field, STRING_FROM_LITERAL("len"))) {
                if (object->type && (object->type->kind == TYPE_SLICE || 
                                     object->type->kind == TYPE_ARRAY)) {
                    Type* result_type = type_int(64, true);
                    IRValue* result = create_temp(b, result_type);
                    
                    IRInstruction* instr = ir_instruction_new(b->arena, IR_OP_FIELD_ACCESS);
                    instr->result = result;
                    instr->operand1 = object;
                    instr->operand2 = ir_value_const_string(b->arena, fa->field);
                    ir_block_add_instruction(b->current_block, instr);
                    
                    return result;
                }
            }
            
            // Для других полей (например, self.x) — существующая логика
            Type* field_type = type_int(32, true);
            IRValue* temp = create_temp(b, field_type);
            
            IRInstruction* instr = ir_instruction_new(b->arena, IR_OP_FIELD_ACCESS);
            instr->result = temp;
            instr->operand1 = object;
            instr->operand2 = ir_value_const_string(b->arena, fa->field);
            ir_block_add_instruction(b->current_block, instr);
            
            return temp;
        }
        
        case AST_SELF_EXPR: {
            if (b->current_function && b->current_function->param_count > 0) {
                return b->current_function->params[0];
            }
            builder_error(b, node->line, node->column, "'self' used outside of method");
            return ir_value_const_int(b->arena, 0, type_int(32, true));
        }

        case AST_ADDR_OF: {
            AddrOfNode* addr = (AddrOfNode*)node;
            IRValue* operand = build_expression(b, addr->operand);
            
            
            Type* ptr_type = type_pointer_new(b->arena, operand->type);
            
            IRValue* result = create_temp(b, ptr_type);
            
            IRInstruction* instr = ir_emit_unary(b->arena, b->current_block, IR_OP_ADDR_OF, operand, ptr_type);
            instr->result = result;
            
            return result;
        }
        
        case AST_DEREF: {
            DerefNode* deref = (DerefNode*)node;
            IRValue* ptr = build_expression(b, deref->operand);
            
            // Тип результата - это тип, на который указывает указатель
            Type* result_type = ptr->type->pointer.pointee;
            
            // Используем create_temp для создания временной переменной
            IRValue* result = create_temp(b, result_type);
            
            // Эмитим инструкцию разыменования
            IRInstruction* instr = ir_emit_unary(b->arena, b->current_block, IR_OP_DEREF, ptr, result_type);
            instr->result = result;
            
            return result;
        }

        case AST_ARRAY_LITERAL: {
            ArrayLiteralNode* arr = (ArrayLiteralNode*)node;
            
            // Определяем тип элементов
            Type* element_type = type_int(32, true);
            if (arr->element_count > 0) {
                IRValue* first = build_expression(b, arr->elements[0]);
                element_type = first->type;
            }
            
            Type* array_type = type_array_new(b->arena, element_type, arr->element_count);
            IRValue* result = create_temp(b, array_type);
            
            IRInstruction* instr = ir_instruction_new(b->arena, IR_OP_ARRAY_LITERAL);
            instr->result = result;
            instr->args = ARENA_ARRAY(b->arena, IRValue*, arr->element_count);
            instr->arg_count = arr->element_count;
            
            for (int i = 0; i < arr->element_count; i++) {
                instr->args[i] = build_expression(b, arr->elements[i]);
            }
            
            ir_block_add_instruction(b->current_block, instr);
            return result;
        }
        
        case AST_INDEX_ACCESS: {
            IndexAccessNode* idx = (IndexAccessNode*)node;
            IRValue* array = build_expression(b, idx->array);
            IRValue* index = build_expression(b, idx->index);
            
            // Определяем тип элемента
            Type* elem_type = type_int(32, true);
            if (array->type) {
                if (array->type->kind == TYPE_ARRAY) {
                    elem_type = array->type->array.element;
                } else if (array->type->kind == TYPE_SLICE) {
                    elem_type = array->type->slice.element;
                } else if (array->type->kind == TYPE_POINTER) {
                    elem_type = array->type->pointer.pointee;
                }
            }
            
            IRValue* result = create_temp(b, elem_type);
            
            IRInstruction* instr = ir_instruction_new(b->arena, IR_OP_INDEX_ACCESS);
            instr->result = result;
            instr->operand1 = array;
            instr->operand2 = index;
            ir_block_add_instruction(b->current_block, instr);
            
            return result;
        }
        
        default:
            builder_error(b, node->line, node->column, "Unsupported expression in IR builder");
            return ir_value_const_int(b->arena, 0, type_int(32, true));
    }
}

// ===== Построение операторов =====

static void build_statement(IRBuilder* b, ASTNode* node) {
    if (!node) return;
    
    switch (node->type) {
        case AST_BLOCK: {
            BlockNode* block = (BlockNode*)node;
            ir_scope_enter(b);
            for (int i = 0; i < block->statement_count; i++) {
                build_statement(b, block->statements[i]);
            }
            ir_scope_leave(b);
            break;
        }
        
        case AST_VAR_DECL: {
            VarDeclNode* var = (VarDeclNode*)node;
            
            Type* var_type = resolve_ast_type(b, var->type);
            
            IRValue* dest = NULL;
            if (var_type->kind != TYPE_INTERFACE) {
                // Создаем IRValue для переменной с правильным типом
                dest = ir_value_var(b->arena, var->name, var_type);
                ir_scope_define(b, var->name, dest);
                
                // Эмитим IR_OP_VAR_DECL, чтобы C-бэкенд увидел переменную
                IRInstruction* decl_instr = ir_instruction_new(b->arena, IR_OP_VAR_DECL);
                decl_instr->result = dest;
                ir_block_add_instruction(b->current_block, decl_instr);
            }
            
            // Если есть инициализатор, генерируем присваивание
            // Если есть инициализатор, генерируем присваивание
            if (var->initializer) {
                IRValue* src = build_expression(b, var->initializer);
                
                // НОВОЕ: Проверяем, не является ли это upcast'ом struct → interface
                if (var_type->kind == TYPE_INTERFACE && 
                    src && src->type && src->type->kind == TYPE_STRUCT) {
                    // Регистрируем саму переменную s (чтобы lookup находил её)
                    dest = ir_value_var(b->arena, var->name, var_type);
                    ir_scope_define(b, var->name, dest);

                    // 1. Регистрируем реализацию в модуле (если ещё не зарегистрирована)
                    String vtable_name = string_concat3(b->arena, 
                                                        src->type->structure.name,
                                                        STRING_FROM_LITERAL("_"),
                                                        var_type->interface.name);
                    vtable_name = string_concat(b->arena, vtable_name, 
                                               STRING_FROM_LITERAL("_vtable"));
                    
                    // Проверяем, есть ли уже такая реализация
                    bool found = false;
                    if (b->module->impls) {
                        for (int i = 0; i < b->module->impl_count; i++) {
                            if (string_equals(b->module->impls[i].vtable_name, vtable_name)) {
                                found = true;
                                break;
                            }
                        }
                    }
                    
                    if (!found) {
                        if (!b->module->impls) {
                            b->module->impls = ARENA_ARRAY(b->arena, InterfaceImpl, 16);
                        }
                        InterfaceImpl* impl = &b->module->impls[b->module->impl_count++];
                        impl->struct_type = src->type;
                        impl->interface_type = var_type;
                        impl->vtable_name = vtable_name;
                    }
                    
                    // 2. Создаём две переменные: s.data и s.vtable
                    Type* void_ptr_type = type_pointer_new(b->arena, type_void());
                    
                    // s.data = &src
                    String data_name = string_concat(b->arena, var->name, 
                                                    STRING_FROM_LITERAL("_data"));
                    IRValue* data_var = ir_value_var(b->arena, data_name, void_ptr_type);
                    ir_scope_define(b, data_name, data_var);
                    
                                        // Создаём ADDR_OF для src
                    Type* ptr_type = type_pointer_new(b->arena, src->type);
                    IRValue* addr_temp = create_temp(b, ptr_type);
                    
                    IRInstruction* addr_instr = ir_instruction_new(b->arena, IR_OP_ADDR_OF);
                    addr_instr->operand1 = src;
                    addr_instr->result = addr_temp;
                    ir_block_add_instruction(b->current_block, addr_instr);
                    
                    // Присваиваем: s_data = &c
                    ir_emit_assign(b->arena, b->current_block, data_var, addr_temp);
                    
                    // Эмитим VAR_DECL для s.data
                    IRInstruction* data_decl = ir_instruction_new(b->arena, IR_OP_VAR_DECL);
                    data_decl->result = data_var;
                    ir_block_add_instruction(b->current_block, data_decl);
                    
                    // s.vtable = &vtable_global
                    String vtable_var_name = string_concat(b->arena, var->name, 
                                                          STRING_FROM_LITERAL("_vtable"));
                    // Тип: Shape_vtable*
                    String vtable_type_name = string_concat(b->arena, 
                                                           var_type->interface.name,
                                                           STRING_FROM_LITERAL("_vtable"));
                    Type* vtable_type = type_struct_new(b->arena, vtable_type_name);
                    Type* vtable_ptr_type = type_pointer_new(b->arena, vtable_type);
                    
                    IRValue* vtable_var = ir_value_var(b->arena, vtable_var_name, vtable_ptr_type);
                    ir_scope_define(b, vtable_var_name, vtable_var);
                    
                    // Создаём глобальную переменную для vtable
                    IRValue* vtable_global = ir_value_global(b->arena, vtable_name, vtable_type);
                    
                    // Создаём временную переменную для адреса vtable
                    IRValue* vtable_addr_temp = create_temp(b, vtable_ptr_type);
                    
                    IRInstruction* vtable_addr = ir_instruction_new(b->arena, IR_OP_ADDR_OF);
                    vtable_addr->operand1 = vtable_global;
                    vtable_addr->result = vtable_addr_temp;
                    ir_block_add_instruction(b->current_block, vtable_addr);
                    
                    // Присваиваем: s_vtable = &Circle_Shape_vtable
                    ir_emit_assign(b->arena, b->current_block, vtable_var, vtable_addr_temp);
                    
                    // Эмитим VAR_DECL для s.vtable
                    IRInstruction* vtable_decl = ir_instruction_new(b->arena, IR_OP_VAR_DECL);
                    vtable_decl->result = vtable_var;
                    ir_block_add_instruction(b->current_block, vtable_decl);
                    
                } else {
                    // Обычное присваивание
                    ir_emit_assign(b->arena, b->current_block, dest, src);
                }
            }
            break;
        }
        
        case AST_RETURN: {
            ReturnNode* ret = (ReturnNode*)node;
            IRValue* value = NULL;
            if (ret->value) {
                value = build_expression(b, ret->value);
            }
            ir_emit_return(b->arena, b->current_block, value);
            break;
        }

        case AST_WHILE: {
            WhileNode* w = (WhileNode*)node;
            
            String cond_label = generate_block_label(b, "while_cond");
            String body_label = generate_block_label(b, "while_body");
            String end_label = generate_block_label(b, "while_end");
            
            IRBlock* cond_block = ir_block_new(b->arena, cond_label);
            IRBlock* body_block = ir_block_new(b->arena, body_label);
            IRBlock* end_block = ir_block_new(b->arena, end_label);
            
            // 1. Прыгаем к проверке условия
            ir_emit_br(b->arena, b->current_block, cond_block);
            ir_function_add_block(b->current_function, cond_block);
            b->current_block = cond_block;
            
            // 2. Вычисляем условие
            IRValue* cond = build_expression(b, w->condition);
            ir_emit_br_cond(b->arena, b->current_block, cond, body_block, end_block);
            
            // 3. Пушим контекст цикла в стек (continue -> cond_block, break -> end_block)
            if (b->loop_depth < 32) {
                b->loop_stack[b->loop_depth].continue_block = cond_block;
                b->loop_stack[b->loop_depth].break_block = end_block;
                b->loop_depth++;
            }
            
            // 4. Тело цикла
            ir_function_add_block(b->current_function, body_block);
            b->current_block = body_block;
            build_statement(b, w->body);
            
            // 5. Обратная дуга к условию (если не было break в конце тела)
            if (b->current_block->last_instruction == NULL ||
                (b->current_block->last_instruction->opcode != IR_OP_RETURN &&
                 b->current_block->last_instruction->opcode != IR_OP_BR &&
                 b->current_block->last_instruction->opcode != IR_OP_BR_COND)) {
                ir_emit_br(b->arena, b->current_block, cond_block);
            }
            
            // 6. Попаем контекст цикла
            if (b->loop_depth > 0) {
                b->loop_depth--;
            }
            
            // 7. Выход из цикла
            ir_function_add_block(b->current_function, end_block);
            b->current_block = end_block;
            break;
        }
        
        case AST_BREAK: {
            if (b->loop_depth <= 0) {
                builder_error(b, node->line, node->column, "'break' outside of loop");
                break;
            }
            // Прыгаем в конец текущего цикла
            IRBlock* end_block = b->loop_stack[b->loop_depth - 1].break_block;
            ir_emit_br(b->arena, b->current_block, end_block);
            
            // Создаём недостижимый блок для последующего кода (если break не последний в блоке)
            String dead_label = generate_block_label(b, "after_break");
            IRBlock* dead_block = ir_block_new(b->arena, dead_label);
            ir_function_add_block(b->current_function, dead_block);
            b->current_block = dead_block;
            break;
        }
        
        case AST_CONTINUE: {
            if (b->loop_depth <= 0) {
                builder_error(b, node->line, node->column, "'continue' outside of loop");
                break;
            }
            // Прыгаем к проверке условия текущего цикла
            IRBlock* cond_block = b->loop_stack[b->loop_depth - 1].continue_block;
            ir_emit_br(b->arena, b->current_block, cond_block);
            
            // Создаём недостижимый блок для последующего кода
            String dead_label = generate_block_label(b, "after_continue");
            IRBlock* dead_block = ir_block_new(b->arena, dead_label);
            ir_function_add_block(b->current_function, dead_block);
            b->current_block = dead_block;
            break;
        }
        
        case AST_IF: {
            IfNode* if_node = (IfNode*)node;
            IRValue* cond = build_expression(b, if_node->condition);
            
            String true_label = generate_block_label(b, "if_true");
            String false_label = generate_block_label(b, "if_false");
            String end_label = generate_block_label(b, "if_end");
            
            IRBlock* true_block = ir_block_new(b->arena, true_label);
            IRBlock* false_block = ir_block_new(b->arena, false_label);
            IRBlock* end_block = ir_block_new(b->arena, end_label);
            
            ir_emit_br_cond(b->arena, b->current_block, cond, true_block, false_block);
            
            ir_function_add_block(b->current_function, true_block);
            b->current_block = true_block;
            build_statement(b, if_node->then_branch);
            ir_emit_br(b->arena, b->current_block, end_block);
            
            ir_function_add_block(b->current_function, false_block);
            b->current_block = false_block;
            if (if_node->else_branch) {
                build_statement(b, if_node->else_branch);
            }
            ir_emit_br(b->arena, b->current_block, end_block);
            
            ir_function_add_block(b->current_function, end_block);
            b->current_block = end_block;
            break;
        }

        case AST_FOR: {
            ForNode* f = (ForNode*)node;
            
            String cond_label = generate_block_label(b, "for_cond");
            String body_label = generate_block_label(b, "for_body");
            String incr_label = generate_block_label(b, "for_incr");
            String end_label = generate_block_label(b, "for_end");
            
            IRBlock* cond_block = ir_block_new(b->arena, cond_label);
            IRBlock* body_block = ir_block_new(b->arena, body_label);
            IRBlock* incr_block = ir_block_new(b->arena, incr_label);
            IRBlock* end_block = ir_block_new(b->arena, end_label);
            
            // 1. Инициализация (в текущем блоке)
            if (f->init) {
                build_statement(b, f->init);
            }
            
            // 2. Переход к условию
            ir_emit_br(b->arena, b->current_block, cond_block);
            ir_function_add_block(b->current_function, cond_block);
            b->current_block = cond_block;
            
            // 3. Условие
            if (f->condition) {
                IRValue* cond = build_expression(b, f->condition);
                ir_emit_br_cond(b->arena, b->current_block, cond, body_block, end_block);
            } else {
                ir_emit_br(b->arena, b->current_block, body_block);
            }
            
            // 4. Пушим контекст цикла (continue -> incr_block, break -> end_block)
            if (b->loop_depth < 32) {
                b->loop_stack[b->loop_depth].continue_block = incr_block;
                b->loop_stack[b->loop_depth].break_block = end_block;
                b->loop_depth++;
            }
            
            // 5. Тело цикла
            ir_function_add_block(b->current_function, body_block);
            b->current_block = body_block;
            build_statement(b, f->body);
            
            // 6. Переход к инкременту
            if (b->current_block->last_instruction == NULL ||
                (b->current_block->last_instruction->opcode != IR_OP_RETURN &&
                 b->current_block->last_instruction->opcode != IR_OP_BR &&
                 b->current_block->last_instruction->opcode != IR_OP_BR_COND)) {
                ir_emit_br(b->arena, b->current_block, incr_block);
            }
            
            // 7. Инкремент
            ir_function_add_block(b->current_function, incr_block);
            b->current_block = incr_block;
            if (f->increment) {
                build_expression(b, f->increment);
            }
            ir_emit_br(b->arena, b->current_block, cond_block);
            
            // 8. Попаем контекст цикла
            if (b->loop_depth > 0) {
                b->loop_depth--;
            }
            
            // 9. Выход из цикла
            ir_function_add_block(b->current_function, end_block);
            b->current_block = end_block;
            break;
        }
        
        default:
            build_expression(b, node);
            break;
    }
}

// ===== Построение объявлений =====

static void build_function(IRBuilder* b, FunctionDeclNode* func, bool is_method, Type* struct_type) {
    fprintf(stderr, "[DEBUG IR BUILD] Building function: %.*s (is_method=%d)\n",
                            (int)func->name.length, func->name.data, is_method);
    // НОВОЕ: Name mangling для методов: Circle_area, Rectangle_area
    String func_name;
    if (is_method && struct_type) {
        func_name = string_concat3(b->arena, 
                                   struct_type->structure.name,
                                   STRING_FROM_LITERAL("_"),
                                   func->name);
    } else {
        func_name = func->name;
    }
    
    IRFunction* ir_func = ir_function_new(b->arena, func_name, NULL);
    
    // ИСПРАВЛЕНО: Берём тип возврата прямо из AST
    ir_func->return_type = resolve_ast_type(b, func->return_type);
    

    // Парсер уже добавил self в func->params для методов!
    // Просто обрабатываем все параметры из func->params
    if (func->param_count > 0) {
        ir_func->params = ARENA_ARRAY(b->arena, IRValue*, func->param_count);
        
        for (int i = 0; i < func->param_count; i++) {
            Type* param_type = resolve_ast_type(b, func->params[i].type);
            
            // Если это self в методе, делаем его указателем
            if (is_method && string_equals(func->params[i].name, STRING_FROM_LITERAL("self"))) {
                param_type = type_pointer_new(b->arena, struct_type);
            }
            
            IRValue* param = ir_value_param(b->arena, func->params[i].name, param_type);
            ir_func->params[i] = param;
        }
        
        ir_func->param_count = func->param_count;
    } else {
        ir_func->params = NULL;
        ir_func->param_count = 0;
    }
    
    char* entry_cstr = format_string(b->arena, "%.*s_entry", (int)func->name.length, func->name.data);
    String entry_label = string_new(entry_cstr, strlen(entry_cstr));
    IRBlock* entry_block = ir_block_new(b->arena, entry_label);
    ir_function_add_block(ir_func, entry_block);
    
    IRFunction* old_func = b->current_function;
    IRBlock* old_block = b->current_block;
    b->current_function = ir_func;
    b->current_block = entry_block;
    
    ir_scope_enter(b);
    
    for (int i = 0; i < ir_func->param_count; i++) {
        ir_scope_define(b, ir_func->params[i]->name, ir_func->params[i]);
    }
    
    build_statement(b, func->body);
    
    ir_scope_leave(b);
    
    if (!b->current_block->last_instruction || 
        b->current_block->last_instruction->opcode != IR_OP_RETURN) {
        ir_emit_return(b->arena, b->current_block, NULL);
    }
    
    b->current_function = old_func;
    b->current_block = old_block;
    
    // Проверяем, не добавили ли уже функцию с таким именем
    bool already_added = false;
    for (IRFunction* f = b->module->functions; f; f = f->next) {
        if (string_equals(f->name, func_name)) {
            already_added = true;
            fprintf(stderr, "[DEBUG IR BUILD] WARNING: Function '%.*s' already added, skipping duplicate\n",
                    (int)func_name.length, func_name.data);
            break;
        }
    }
    
    if (!already_added) {
        if (!b->module->functions) {
            b->module->functions = ir_func;
        } else {
            IRFunction* last = b->module->functions;
            while (last->next) last = last->next;
            last->next = ir_func;
        }
        b->module->function_count++;
    }
}

static void build_declaration(IRBuilder* b, ASTNode* node) {
    if (!node) return;
    
    switch (node->type) {
        case AST_FUNCTION_DECL: {
            FunctionDeclNode* func = (FunctionDeclNode*)node;
            build_function(b, func, false, NULL);
            break;
        }
        
        case AST_METHODS_BLOCK: {
            MethodsBlockNode* mb = (MethodsBlockNode*)node;
            Type* struct_type = symbol_lookup_type(&b->analyzer->symbols, mb->type_name);
            
            for (int i = 0; i < mb->method_count; i++) {
                build_function(b, &mb->methods[i], true, struct_type);
            }
            break;
        }
        
        case AST_STRUCT_DECL:
        case AST_ENUM_DECL:
        case AST_INTERFACE_DECL:
        case AST_IMPORT:
            break;
        
        default:
            break;
    }
}

// ===== Главный интерфейс =====

void ir_builder_init(IRBuilder* builder, Arena* arena, ErrorReporter* errors, Analyzer* analyzer) {
    builder->arena = arena;
    builder->errors = errors;
    builder->analyzer = analyzer;
    builder->module = ir_module_new(arena, STRING_FROM_LITERAL("main"));
    builder->current_function = NULL;
    builder->current_block = NULL;
    builder->error_count = 0;
    builder->current_scope = NULL;
    builder->loop_depth = 0;
}

// Рекурсивный обход AST для сбора структур
// Обход корневого блока для сбора структур
// Рекурсивный обход AST для сбора структур
static void collect_structs(IRBuilder* builder, BlockNode* block) {
    (void)block; // Больше не используем AST напрямую
    
    fprintf(stderr, "[DEBUG IR] Scanning symbol table for instantiated structs...\n");
    
    Scope* scope = builder->analyzer->symbols.current_scope;
    int scope_depth = 0;
    while (scope != NULL) {
        for (int i = 0; i < scope->capacity; i++) {
            Symbol* sym = scope->table[i];
            while (sym != NULL) {
                if (sym->kind == SYMBOL_STRUCT && sym->type && sym->type->kind == TYPE_STRUCT) {
                    // Пропускаем оригинальные generic структуры (без type_args)
                    if (sym->type->structure.is_generic && sym->type->structure.type_arg_count == 0) {
                        sym = sym->next;
                        continue;
                    }
                    
                    if (!builder->module->struct_types) {
                        builder->module->struct_types = ARENA_ARRAY(builder->arena, Type*, 16);
                    }
                    
                    // Проверяем, не добавили ли уже
                    bool found = false;
                    for (int j = 0; j < builder->module->struct_type_count; j++) {
                        if (string_equals(builder->module->struct_types[j]->structure.name,
                                        sym->type->structure.name)) {
                            found = true;
                            break;
                        }
                    }
                    
                    if (!found) {
                        fprintf(stderr, "[DEBUG IR] Adding struct: %.*s\n",
                                (int)sym->type->structure.name.length,
                                sym->type->structure.name.data);
                        builder->module->struct_types[builder->module->struct_type_count++] = sym->type;
                    }
                }
                sym = sym->next;
            }

            // НОВОЕ: Сбор интерфейсов из таблицы символов
            for (int i = 0; i < scope->capacity; i++) {
                Symbol* sym = scope->table[i];
                while (sym != NULL) {
                    if (sym->kind == SYMBOL_INTERFACE && sym->type && sym->type->kind == TYPE_INTERFACE) {
                        if (!builder->module->interface_types) {
                            builder->module->interface_types = ARENA_ARRAY(builder->arena, Type*, 16);
                        }
                        
                        // Проверяем дубликаты
                        bool found = false;
                        for (int j = 0; j < builder->module->interface_type_count; j++) {
                            if (string_equals(builder->module->interface_types[j]->interface.name,
                                            sym->type->interface.name)) {
                                found = true;
                                break;
                            }
                        }
                        
                        if (!found) {
                            fprintf(stderr, "[DEBUG IR] Adding interface: %.*s\n",
                                    (int)sym->type->interface.name.length,
                                    sym->type->interface.name.data);
                            builder->module->interface_types[builder->module->interface_type_count++] = sym->type;
                        }
                    }
                    sym = sym->next;
                }
            }           

        }
        scope = scope->parent;
        scope_depth++;
    }
}


IRModule* ir_builder_build(IRBuilder* builder, ASTNode* ast) {
    if (!ast || ast->type != AST_BLOCK) {
        builder_error(builder, 0, 0, "Invalid AST root for IR builder");
        return NULL;
    }
    
    BlockNode* block = (BlockNode*)ast;
    
    // НОВОЕ: Сначала собираем все структуры (чтобы типы были доступны)
    collect_structs(builder, block);
    
    // Затем строим IR для функций
    for (int i = 0; i < block->statement_count; i++) {
        ASTNode* node = block->statements[i];
        if (node && (node->type == AST_FUNCTION_DECL || node->type == AST_METHODS_BLOCK)) {
            build_declaration(builder, node);
        }
        //         // НОВОЕ: Обработка интерфейсов
        // if (node->type == AST_INTERFACE_DECL) {
        //     InterfaceDeclNode* iface = (InterfaceDeclNode*)node;
            
        //     // Получаем тип интерфейса из Analyzer
        //     Type* iface_type = symbol_lookup_type(&builder->analyzer->symbols, iface->name);
        //     if (!iface_type) {
        //         fprintf(stderr, "[WARN] Interface '%.*s' not found in symbols\n",
        //                 (int)iface->name.length, iface->name.data);
        //         continue;
        //     }
            
        //     // Добавляем в модуль
        //     if (!builder->module->interface_types) {
        //         builder->module->interface_types = ARENA_ARRAY(builder->arena, Type*, 16);
        //     }
            
        //     builder->module->interface_types[builder->module->interface_type_count++] = iface_type;
        // }
            // Затем строим IR для функций
        for (int i = 0; i < block->statement_count; i++) {
            ASTNode* node = block->statements[i];
            if (node && (node->type == AST_FUNCTION_DECL || node->type == AST_METHODS_BLOCK)) {
                build_declaration(builder, node);
            }
        }
    }
    
    fprintf(stderr, "[DEBUG IR BUILD] Total functions built: %d\n", builder->module->function_count);
    for (IRFunction* f = builder->module->functions; f; f = f->next) {
        fprintf(stderr, "[DEBUG IR BUILD]   Function: %.*s\n", 
                (int)f->name.length, f->name.data);
    }
    
    return builder->module;
}

int ir_builder_error_count(IRBuilder* builder) {
    return builder->error_count;
}
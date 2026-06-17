// src/ir/ir.c
#include "ir/ir.h"
#include "common/utils.h"
#include <stdio.h>
#include <string.h>

// ===== Создание IR =====

IRModule* ir_module_new(Arena* arena, String name) {
    IRModule* module = ARENA_ALLOC(arena, IRModule);
    module->name = name;
    module->functions = NULL;
    module->function_count = 0;
    module->globals = NULL;
    module->global_count = 0;
    module->struct_types = NULL;       // <-- ДОБАВЛЕНО
    module->struct_type_count = 0;     // <-- ДОБАВЛЕНО
    module->arena = arena;
    return module;
}

IRFunction* ir_function_new(Arena* arena, String name, Type* return_type) {
    IRFunction* func = ARENA_ALLOC(arena, IRFunction);
    func->name = arena_string(arena, name);
    func->return_type = return_type;
    func->params = NULL;
    func->param_count = 0;
    func->entry_block = NULL;
    func->blocks = NULL;
    func->block_count = 0;
    func->next_temp_id = 0;
    func->next = NULL;
    return func;
}

IRBlock* ir_block_new(Arena* arena, String label) {
    IRBlock* block = ARENA_ALLOC(arena, IRBlock);
    block->label = arena_string(arena, label);
    block->instructions = NULL;
    block->last_instruction = NULL;
    block->instruction_count = 0;
    block->predecessors = NULL;
    block->predecessor_count = 0;
    block->successors = NULL;
    block->successor_count = 0;
    block->next = NULL;
    return block;
}

IRInstruction* ir_instruction_new(Arena* arena, IROpcode opcode) {
    IRInstruction* instr = ARENA_ALLOC(arena, IRInstruction);
    instr->opcode = opcode;
    instr->result = NULL;
    instr->operand1 = NULL;
    instr->operand2 = NULL;
    instr->operand3 = NULL;
    instr->args = NULL;
    instr->arg_count = 0;
    instr->true_block = NULL;
    instr->false_block = NULL;
    instr->target_block = NULL;
    instr->phi_args = NULL;
    instr->phi_count = 0;
    instr->line = 0;
    instr->next = NULL;

        // НОВОЕ: Отладка для FIELD_STORE
    if (opcode == IR_OP_FIELD_STORE) {
        fprintf(stderr, "[DEBUG IR_NEW] Created FIELD_STORE instruction at %p\n", (void*)instr);
    }

    return instr;
}

// ===== Создание значений =====

IRValue* ir_value_const_int(Arena* arena, long long val, Type* type) {
    IRValue* v = ARENA_ALLOC(arena, IRValue);
    v->kind = IR_VALUE_CONST_INT;
    v->type = type;
    v->int_val = val;
    return v;
}

IRValue* ir_value_const_float(Arena* arena, double val, Type* type) {
    IRValue* v = ARENA_ALLOC(arena, IRValue);
    v->kind = IR_VALUE_CONST_FLOAT;
    v->type = type;
    v->float_val = val;
    return v;
}

IRValue* ir_value_const_bool(Arena* arena, bool val) {
    IRValue* v = ARENA_ALLOC(arena, IRValue);
    v->kind = IR_VALUE_CONST_BOOL;
    v->type = type_bool();
    v->bool_val = val;
    return v;
}

IRValue* ir_value_const_string(Arena* arena, String val) {
    IRValue* v = ARENA_ALLOC(arena, IRValue);
    v->kind = IR_VALUE_CONST_STRING;
    v->type = type_string();
    v->string_val = arena_string(arena, val);
    return v;
}

IRValue* ir_value_temp(Arena* arena, int id, Type* type) {
    IRValue* v = ARENA_ALLOC(arena, IRValue);
    v->kind = IR_VALUE_TEMP;
    v->type = type;
    v->temp_id = id;
    return v;
}

IRValue* ir_value_var(Arena* arena, String name, Type* type) {
    IRValue* v = ARENA_ALLOC(arena, IRValue);
    v->kind = IR_VALUE_VAR;
    v->type = type;
    v->name = arena_string(arena, name);
    return v;
}

IRValue* ir_value_global(Arena* arena, String name, Type* type) {
    IRValue* v = ARENA_ALLOC(arena, IRValue);
    v->kind = IR_VALUE_GLOBAL;
    v->type = type;
    v->name = arena_string(arena, name);
    return v;
}

IRValue* ir_value_param(Arena* arena, String name, Type* type) {
    IRValue* v = ARENA_ALLOC(arena, IRValue);
    v->kind = IR_VALUE_PARAM;
    v->type = type;
    v->name = arena_string(arena, name);
    return v;
}

// ===== Добавление инструкций =====

void ir_block_add_instruction(IRBlock* block, IRInstruction* instr) {
    if (!block->instructions) {
        block->instructions = instr;
    } else {
        block->last_instruction->next = instr;
    }
    block->last_instruction = instr;
    block->instruction_count++;
}

IRInstruction* ir_emit_binary(Arena* arena, IRBlock* block, IROpcode op, 
                               IRValue* left, IRValue* right, Type* result_type) {
    (void)result_type;
    IRInstruction* instr = ir_instruction_new(arena, op);
    instr->operand1 = left;
    instr->operand2 = right;
    // Временная переменная для результата будет создана позже при необходимости
    ir_block_add_instruction(block, instr);
    return instr;
}

IRInstruction* ir_emit_unary(Arena* arena, IRBlock* block, IROpcode op, 
                              IRValue* operand, Type* result_type) {
    (void)result_type;
    IRInstruction* instr = ir_instruction_new(arena, op);
    instr->operand1 = operand;
    ir_block_add_instruction(block, instr);
    return instr;
}

IRInstruction* ir_emit_assign(Arena* arena, IRBlock* block, 
                               IRValue* dest, IRValue* src) {
    IRInstruction* instr = ir_instruction_new(arena, IR_OP_ASSIGN);
    instr->result = dest;
    instr->operand1 = src;
    ir_block_add_instruction(block, instr);
    return instr;
}

IRInstruction* ir_emit_call(Arena* arena, IRBlock* block, 
                             IRValue* func, IRValue** args, int arg_count, 
                             Type* result_type) {
    (void)result_type;
    IRInstruction* instr = ir_instruction_new(arena, IR_OP_CALL);
    instr->operand1 = func;
    instr->args = args;
    instr->arg_count = arg_count;
    ir_block_add_instruction(block, instr);
    return instr;
}

IRInstruction* ir_emit_return(Arena* arena, IRBlock* block, IRValue* value) {
    IRInstruction* instr = ir_instruction_new(arena, IR_OP_RETURN);
    instr->operand1 = value;
    ir_block_add_instruction(block, instr);
    return instr;
}

IRInstruction* ir_emit_br(Arena* arena, IRBlock* block, IRBlock* target) {
    IRInstruction* instr = ir_instruction_new(arena, IR_OP_BR);
    instr->target_block = target;
    ir_block_add_instruction(block, instr);
    return instr;
}

IRInstruction* ir_emit_br_cond(Arena* arena, IRBlock* block, IRValue* cond, 
                                IRBlock* true_block, IRBlock* false_block) {
    IRInstruction* instr = ir_instruction_new(arena, IR_OP_BR_COND);
    instr->operand1 = cond;
    instr->true_block = true_block;
    instr->false_block = false_block;
    ir_block_add_instruction(block, instr);
    return instr;
}

IRInstruction* ir_emit_cast(Arena* arena, IRBlock* block, 
                             IRValue* src, Type* target_type) {
    (void)target_type;
    IRInstruction* instr = ir_instruction_new(arena, IR_OP_CAST);
    instr->operand1 = src;
    ir_block_add_instruction(block, instr);
    return instr;
}

// ===== Управление блоками =====

void ir_function_add_block(IRFunction* func, IRBlock* block) {
    if (!func->blocks) {
        func->blocks = block;
        func->entry_block = block;
    } else {
        IRBlock* last = func->blocks;
        while (last->next) last = last->next;
        last->next = block;
    }
    func->block_count++;
}

// ===== Вывод IR =====

static const char* opcode_name(IROpcode op) {
    switch (op) {
        case IR_OP_ADD: return "add";
        case IR_OP_SUB: return "sub";
        case IR_OP_MUL: return "mul";
        case IR_OP_DIV: return "div";
        case IR_OP_MOD: return "mod";
        case IR_OP_EQ: return "eq";
        case IR_OP_NE: return "ne";
        case IR_OP_LT: return "lt";
        case IR_OP_LE: return "le";
        case IR_OP_GT: return "gt";
        case IR_OP_GE: return "ge";
        case IR_OP_AND: return "and";
        case IR_OP_OR: return "or";
        case IR_OP_NOT: return "not";
        case IR_OP_BIT_AND: return "bit_and";
        case IR_OP_BIT_OR: return "bit_or";
        case IR_OP_BIT_XOR: return "bit_xor";
        case IR_OP_SHL: return "shl";
        case IR_OP_SHR: return "shr";
        case IR_OP_ASSIGN: return "assign";
        case IR_OP_CALL: return "call";
        case IR_OP_RETURN: return "return";
        case IR_OP_BR: return "br";
        case IR_OP_BR_COND: return "br_cond";
        case IR_OP_LOAD: return "load";
        case IR_OP_STORE: return "store";
        case IR_OP_FIELD_ACCESS: return "field";
        case IR_OP_CAST: return "cast";
        case IR_OP_PHI: return "phi";
        default: return "unknown";
    }
}

void ir_print_value(IRValue* value) {
    if (!value) {
        printf("null");
        return;
    }
    
    switch (value->kind) {
        case IR_VALUE_CONST_INT:
            printf("%lld", value->int_val);
            break;
        case IR_VALUE_CONST_FLOAT:
            printf("%f", value->float_val);
            break;
        case IR_VALUE_CONST_BOOL:
            printf("%s", value->bool_val ? "true" : "false");
            break;
        case IR_VALUE_CONST_STRING:
            printf("\"%.*s\"", (int)value->string_val.length, value->string_val.data);
            break;
        case IR_VALUE_TEMP:
            printf("%%t%d", value->temp_id);
            break;
        case IR_VALUE_VAR:
        case IR_VALUE_GLOBAL:
        case IR_VALUE_PARAM:
            printf("%.*s", (int)value->name.length, value->name.data);
            break;
    }
}

void ir_print_instruction(IRInstruction* instr) {
    printf("  ");
    
    if (instr->result) {
        ir_print_value(instr->result);
        printf(" = ");
    }
    
    printf("%s", opcode_name(instr->opcode));
    
    if (instr->operand1) {
        printf(" ");
        ir_print_value(instr->operand1);
    }
    
    if (instr->operand2) {
        printf(", ");
        ir_print_value(instr->operand2);
    }
    
    if (instr->opcode == IR_OP_CALL && instr->arg_count > 0) {
        printf("(");
        for (int i = 0; i < instr->arg_count; i++) {
            if (i > 0) printf(", ");
            ir_print_value(instr->args[i]);
        }
        printf(")");
    }
    
    if (instr->opcode == IR_OP_BR && instr->target_block) {
        printf(" %.*s", (int)instr->target_block->label.length, 
               instr->target_block->label.data);
    }
    
    if (instr->opcode == IR_OP_BR_COND) {
        printf(" ? %.*s : %.*s",
               (int)instr->true_block->label.length, instr->true_block->label.data,
               (int)instr->false_block->label.length, instr->false_block->label.data);
    }
    
    printf("\n");
}

void ir_print_block(IRBlock* block) {
    printf("%.*s:\n", (int)block->label.length, block->label.data);
    for (IRInstruction* instr = block->instructions; instr; instr = instr->next) {
        ir_print_instruction(instr);
    }
    printf("\n");
}

void ir_print_function(IRFunction* func) {
    if (!func) return;
    
    const char* ret_type_str = "void";
    if (func->return_type) {
        ret_type_str = type_to_string(NULL, func->return_type);
    }
    
    printf("define %s %.*s(", 
           ret_type_str,
           (int)func->name.length, func->name.data);
    
    for (int i = 0; i < func->param_count; i++) {
        if (i > 0) printf(", ");
        
        const char* param_type_str = "?";
        if (func->params[i] && func->params[i]->type) {
            param_type_str = type_to_string(NULL, func->params[i]->type);
        }
        
        if (func->params[i] && func->params[i]->name.data) {
            printf("%s %.*s", 
                   param_type_str,
                   (int)func->params[i]->name.length, func->params[i]->name.data);
        } else {
            printf("%s <unnamed>", param_type_str);
        }
    }
    printf(") {\n");
    
    for (IRBlock* block = func->blocks; block; block = block->next) {
        ir_print_block(block);
    }
    
    printf("}\n\n");
}

void ir_print_module(IRModule* module) {
    printf("; Module: %.*s\n\n", (int)module->name.length, module->name.data);
    
    for (IRFunction* func = module->functions; func; func = func->next) {
        ir_print_function(func);
    }
}
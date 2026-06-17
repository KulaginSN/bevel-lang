// src/ir/printer.c
#include "ir/printer.h"
#include <stdio.h>

// Обёртки над функциями из ir.c для вывода в произвольный FILE*

void ir_printer_print_value(FILE* out, IRValue* value) {
    if (!value) {
        fprintf(out, "null");
        return;
    }
    
    switch (value->kind) {
        case IR_VALUE_CONST_INT:
            fprintf(out, "%lld", value->int_val);
            break;
        case IR_VALUE_CONST_FLOAT:
            fprintf(out, "%f", value->float_val);
            break;
        case IR_VALUE_CONST_BOOL:
            fprintf(out, "%s", value->bool_val ? "true" : "false");
            break;
        case IR_VALUE_CONST_STRING:
            fprintf(out, "\"%.*s\"", (int)value->string_val.length, value->string_val.data);
            break;
        case IR_VALUE_TEMP:
            fprintf(out, "%%t%d", value->temp_id);
            break;
        case IR_VALUE_VAR:
        case IR_VALUE_GLOBAL:
        case IR_VALUE_PARAM:
            fprintf(out, "%.*s", (int)value->name.length, value->name.data);
            break;
    }
}

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
        case IR_OP_ASSIGN: return "assign";
        case IR_OP_CALL: return "call";
        case IR_OP_RETURN: return "return";
        case IR_OP_BR: return "br";
        case IR_OP_BR_COND: return "br_cond";
        case IR_OP_CAST: return "cast";
        case IR_OP_FIELD_ACCESS: return "field";
        case IR_OP_VAR_DECL: return "var_decl";
        case IR_OP_FIELD_STORE: return "field_store";
        default: return "unknown";
    }
}

void ir_printer_print_instruction(FILE* out, IRInstruction* instr) {
    fprintf(out, "  ");
    
    // НОВОЕ: Специальная печать для VAR_DECL
    if (instr->opcode == IR_OP_VAR_DECL) {
        fprintf(out, "var_decl ");
        if (instr->result) {
            ir_printer_print_value(out, instr->result);
        }
        fprintf(out, "\n");
        return;  // Ранний выход, не печатаем остальное
    }
    
    if (instr->result) {
        ir_printer_print_value(out, instr->result);
        fprintf(out, " = ");
    }
    
    fprintf(out, "%s", opcode_name(instr->opcode));
    
    if (instr->operand1) {
        fprintf(out, " ");
        ir_printer_print_value(out, instr->operand1);
    }
    
    if (instr->operand2) {
        fprintf(out, ", ");
        ir_printer_print_value(out, instr->operand2);
    }

    // НОВОЕ: Печатаем operand3 для FIELD_STORE и INDEX_STORE
    if (instr->operand3) {
        fprintf(out, ", ");
        ir_printer_print_value(out, instr->operand3);
    }
    
    if (instr->opcode == IR_OP_CALL && instr->arg_count > 0) {
        fprintf(out, "(");
        for (int i = 0; i < instr->arg_count; i++) {
            if (i > 0) fprintf(out, ", ");
            ir_printer_print_value(out, instr->args[i]);
        }
        fprintf(out, ")");
    }
    
    if (instr->opcode == IR_OP_BR && instr->target_block) {
        fprintf(out, " %.*s", (int)instr->target_block->label.length, 
                instr->target_block->label.data);
    }
    
    if (instr->opcode == IR_OP_BR_COND) {
        fprintf(out, " ? %.*s : %.*s",
                (int)instr->true_block->label.length, instr->true_block->label.data,
                (int)instr->false_block->label.length, instr->false_block->label.data);
    }
    
    fprintf(out, "\n");
}

void ir_printer_print_block(FILE* out, IRBlock* block) {
    fprintf(out, "%.*s:\n", (int)block->label.length, block->label.data);
    for (IRInstruction* instr = block->instructions; instr; instr = instr->next) {
        ir_printer_print_instruction(out, instr);
    }
    fprintf(out, "\n");
}

void ir_printer_print_function(FILE* out, IRFunction* func) {
    if (!func) return;
    
    const char* ret_type_str = "void";
    if (func->return_type) {
        ret_type_str = type_to_string(NULL, func->return_type);
    }
    
    fprintf(out, "define %s %.*s(", 
            ret_type_str,
            (int)func->name.length, func->name.data);
    
    for (int i = 0; i < func->param_count; i++) {
        if (i > 0) fprintf(out, ", ");
        
        const char* param_type_str = "?";
        if (func->params[i] && func->params[i]->type) {
            param_type_str = type_to_string(NULL, func->params[i]->type);
        }
        
        // const char* param_name_str = "?";
        if (func->params[i] && func->params[i]->name.data) {
            fprintf(out, "%s %.*s", 
                    param_type_str,
                    (int)func->params[i]->name.length, func->params[i]->name.data);
        } else {
            fprintf(out, "%s <unnamed>", param_type_str);
        }
    }
    fprintf(out, ") {\n");
    
    for (IRBlock* block = func->blocks; block; block = block->next) {
        ir_printer_print_block(out, block);
    }
    
    fprintf(out, "}\n\n");
}

void ir_printer_print_module(FILE* out, IRModule* module) {
    fprintf(out, "; Module: %.*s\n\n", (int)module->name.length, module->name.data);
    
    for (IRFunction* func = module->functions; func; func = func->next) {
        ir_printer_print_function(out, func);
    }
}

void ir_printer_print_module_stdout(IRModule* module) {
    ir_printer_print_module(stdout, module);
}

void ir_printer_print_stats(IRModule* module) {
    int total_functions = 0;
    int total_blocks = 0;
    int total_instructions = 0;
    
    for (IRFunction* func = module->functions; func; func = func->next) {
        total_functions++;
        for (IRBlock* block = func->blocks; block; block = block->next) {
            total_blocks++;
            total_instructions += block->instruction_count;
        }
    }
    
    printf("IR Statistics:\n");
    printf("  Functions: %d\n", total_functions);
    printf("  Blocks: %d\n", total_blocks);
    printf("  Instructions: %d\n", total_instructions);
}
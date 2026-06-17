// src/ir/printer.h
#ifndef BEVEL_IR_PRINTER_H
#define BEVEL_IR_PRINTER_H

#include "ir/ir.h"
#include <stdio.h>

// Печать модуля в файл
void ir_printer_print_module(FILE* out, IRModule* module);

// Печать модуля в stdout
void ir_printer_print_module_stdout(IRModule* module);

// Печать функции
void ir_printer_print_function(FILE* out, IRFunction* func);

// Печать блока
void ir_printer_print_block(FILE* out, IRBlock* block);

// Печать инструкции
void ir_printer_print_instruction(FILE* out, IRInstruction* instr);

// Печать значения
void ir_printer_print_value(FILE* out, IRValue* value);

// Печать статистики
void ir_printer_print_stats(IRModule* module);

#endif // BEVEL_IR_PRINTER_H
// src/parser/printer.h
#ifndef BEVEL_PRINTER_H
#define BEVEL_PRINTER_H

#include "parser/ast.h"
#include <stdbool.h>

// Печатает AST в stdout с отступами для читаемости
void ast_print(ASTNode* node, int depth);

// Печатает AST с цветовой подсветкой (ANSI escape codes)
void ast_print_colored(ASTNode* node, int depth);

// Печатает один узел (для отладки)
void ast_print_node(ASTNode* node, bool colored);

#endif // BEVEL_PRINTER_H
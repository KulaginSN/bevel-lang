// src/parser/printer.c
#include "parser/printer.h"
#include "lexer/token.h"
#include <stdio.h>

// ANSI цвета
#define C_RESET   "\033[0m"
#define C_RED     "\033[31m"
#define C_GREEN   "\033[32m"
#define C_YELLOW  "\033[33m"
#define C_BLUE    "\033[34m"
#define C_MAGENTA "\033[35m"
#define C_CYAN    "\033[36m"
#define C_GRAY    "\033[90m"
#define C_BOLD    "\033[1m"

static bool g_use_colors = false;

static const char* col(const char* c) {
    return g_use_colors ? c : "";
}

static void indent(int depth) {
    for (int i = 0; i < depth; i++) {
        printf("%s│%s   ", col(C_GRAY), col(C_RESET));
    }
}

static void print_prefix(int depth, const char* node_color, const char* node_name) {
    indent(depth);
    printf("%s├─%s %s%s%s", col(C_GRAY), col(C_RESET), 
           col(node_color), node_name, col(C_RESET));
}

static void print_str(String s) {
    if (s.data && s.length > 0) {
        printf("%.*s", (int)s.length, s.data);
    }
}

// Рекурсивный обход AST с печатью
static void print_node(ASTNode* node, int depth) {
    if (!node) {
        print_prefix(depth, C_GRAY, "<null>\n");
        return;
    }
    
    switch (node->type) {
        case AST_INT_LITERAL: {
            IntLiteralNode* n = (IntLiteralNode*)node;
            print_prefix(depth, C_YELLOW, "IntLiteral");
            printf("(%s%lld%s)\n", col(C_CYAN), n->value, col(C_RESET));
            break;
        }
        case AST_FLOAT_LITERAL: {
            FloatLiteralNode* n = (FloatLiteralNode*)node;
            print_prefix(depth, C_YELLOW, "FloatLiteral");
            printf("(%s%f%s)\n", col(C_CYAN), n->value, col(C_RESET));
            break;
        }
        case AST_ASSIGN: {
            AssignNode* assign = (AssignNode*)node;
            print_prefix(depth, C_CYAN, "Assign\n");
            print_node(assign->target, depth + 1);
            print_node(assign->value, depth + 1);
            break;
        }
        case AST_STRING_LITERAL: {
            StringLiteralNode* n = (StringLiteralNode*)node;
            print_prefix(depth, C_YELLOW, "StringLiteral");
            printf("(%s\"", col(C_GREEN));
            print_str(n->value);
            printf("\"%s)\n", col(C_RESET));
            break;
        }
        case AST_CHAR_LITERAL: {
            CharLiteralNode* n = (CharLiteralNode*)node;
            print_prefix(depth, C_YELLOW, "CharLiteral");
            printf("(%s'%c'%s)\n", col(C_GREEN), n->value, col(C_RESET));
            break;
        }
        case AST_BOOL_LITERAL: {
            BoolLiteralNode* n = (BoolLiteralNode*)node;
            print_prefix(depth, C_YELLOW, "BoolLiteral");
            printf("(%s%s%s)\n", col(C_CYAN), n->value ? "true" : "false", col(C_RESET));
            break;
        }
        case AST_IDENTIFIER: {
            IdentifierNode* n = (IdentifierNode*)node;
            print_prefix(depth, C_CYAN, "Identifier");
            printf("("); print_str(n->name); printf(")\n");
            break;
        }
        case AST_SELF_EXPR:
            print_prefix(depth, C_MAGENTA, "SelfExpr\n");
            break;
        case AST_BINARY_EXPR: {
            BinaryExprNode* n = (BinaryExprNode*)node;
            print_prefix(depth, C_MAGENTA, "BinaryExpr");
            printf("(%s%s%s)\n", col(C_BOLD), token_type_name(n->op), col(C_RESET));
            print_node(n->left, depth + 1);
            print_node(n->right, depth + 1);
            break;
        }
        case AST_UNARY_EXPR: {
            UnaryExprNode* n = (UnaryExprNode*)node;
            print_prefix(depth, C_MAGENTA, "UnaryExpr");
            printf("(%s%s%s)\n", col(C_BOLD), token_type_name(n->op), col(C_RESET));
            print_node(n->operand, depth + 1);
            break;
        }
        case AST_CALL_EXPR: {
            CallExprNode* n = (CallExprNode*)node;
            print_prefix(depth, C_MAGENTA, "CallExpr");
            printf("(%d args)\n", n->arg_count);
            print_node(n->callee, depth + 1);
            for (int i = 0; i < n->arg_count; i++) {
                print_node(n->args[i], depth + 1);
            }
            break;
        }
        case AST_INDEX_EXPR: {
            IndexExprNode* n = (IndexExprNode*)node;
            print_prefix(depth, C_MAGENTA, "IndexExpr\n");
            print_node(n->object, depth + 1);
            print_node(n->index, depth + 1);
            break;
        }
        case AST_FIELD_ACCESS: {
            FieldAccessNode* n = (FieldAccessNode*)node;
            print_prefix(depth, C_MAGENTA, "FieldAccess");
            printf("(."); print_str(n->field); printf(")\n");
            print_node(n->object, depth + 1);
            break;
        }
        case AST_VAR_DECL: {
            VarDeclNode* n = (VarDeclNode*)node;
            print_prefix(depth, C_BLUE, "VarDecl");
            printf("("); print_str(n->name); printf(")\n");
            print_node(n->type, depth + 1);
            if (n->initializer) print_node(n->initializer, depth + 1);
            break;
        }
        case AST_RETURN: {
            ReturnNode* n = (ReturnNode*)node;
            print_prefix(depth, C_RED, "Return\n");
            if (n->value) print_node(n->value, depth + 1);
            break;
        }
        case AST_IF: {
            IfNode* if_node = (IfNode*)node;
            print_prefix(depth, C_YELLOW, "If\n");
            print_node(if_node->condition, depth + 1);
            print_prefix(depth + 1, C_GREEN, "Then:\n");
            print_node(if_node->then_branch, depth + 2);
            if (if_node->else_branch) {
                print_prefix(depth + 1, C_RED, "Else:\n");
                print_node(if_node->else_branch, depth + 2);
            }
            break;
        }
        case AST_FOR: {
            ForNode* f = (ForNode*)node;
            print_prefix(depth, C_YELLOW, "For\n");
            if (f->init) {
                print_prefix(depth + 1, C_CYAN, "Init:\n");
                print_node(f->init, depth + 2);
            }
            if (f->condition) {
                print_prefix(depth + 1, C_CYAN, "Condition:\n");
                print_node(f->condition, depth + 2);
            }
            if (f->increment) {
                print_prefix(depth + 1, C_CYAN, "Increment:\n");
                print_node(f->increment, depth + 2);
            }
            print_prefix(depth + 1, C_GREEN, "Body:\n");
            print_node(f->body, depth + 2);
            break;
        }
        case AST_WHILE: {
            WhileNode* w = (WhileNode*)node;
            print_prefix(depth, C_YELLOW, "While\n");
            print_prefix(depth + 1, C_CYAN, "Condition:\n");
            print_node(w->condition, depth + 2);
            print_prefix(depth + 1, C_GREEN, "Body:\n");
            print_node(w->body, depth + 2);
            break;
        }
        case AST_BREAK:
            print_prefix(depth, C_RED, "Break\n");
            break;
        case AST_CONTINUE:
            print_prefix(depth, C_RED, "Continue\n");
            break;
        case AST_SWITCH: {
            SwitchNode* n = (SwitchNode*)node;
            print_prefix(depth, C_RED, "Switch\n");
            print_node(n->value, depth + 1);
            for (int i = 0; i < n->case_count; i++) {
                print_node(n->cases[i], depth + 1);
            }
            break;
        }
        case AST_CASE: {
            CaseNode* n = (CaseNode*)node;
            print_prefix(depth, C_RED, n->value ? "Case" : "Default\n");
            if (n->value) print_node(n->value, depth + 1);
            print_node(n->body, depth + 1);
            break;
        }
        case AST_BLOCK: {
            BlockNode* n = (BlockNode*)node;
            print_prefix(depth, C_GRAY, "Block");
            printf("(%d stmts)\n", n->statement_count);
            for (int i = 0; i < n->statement_count; i++) {
                print_node(n->statements[i], depth + 1);
            }
            break;
        }
        case AST_FUNCTION_DECL: {
            FunctionDeclNode* n = (FunctionDeclNode*)node;
            print_prefix(depth, C_GREEN, "FunctionDecl");
            printf("("); print_str(n->name); 
            printf(", %d params%s)\n", n->param_count, 
                   n->is_generic ? ", generic" : "");
            print_node(n->return_type, depth + 1);
            print_node(n->body, depth + 1);
            break;
        }
        case AST_STRUCT_DECL: {
            StructDeclNode* n = (StructDeclNode*)node;
            print_prefix(depth, C_GREEN, "StructDecl");
            printf("("); print_str(n->name); 
            printf(", %d fields)\n", n->field_count);
            for (int i = 0; i < n->field_count; i++) {
                indent(depth + 1);
                printf("%s├─%s Field(", col(C_GRAY), col(C_RESET));
                print_str(n->fields[i].name);
                printf(")\n");
                print_node(n->fields[i].type, depth + 2);
            }
            break;
        }
        case AST_ENUM_DECL: {
            EnumDeclNode* n = (EnumDeclNode*)node;
            print_prefix(depth, C_GREEN, "EnumDecl");
            printf("("); print_str(n->name); 
            printf(", %d values)\n", n->value_count);
            break;
        }
        case AST_INTERFACE_DECL: {
            InterfaceDeclNode* n = (InterfaceDeclNode*)node;
            print_prefix(depth, C_GREEN, "InterfaceDecl");
            printf("("); print_str(n->name); 
            printf(", %d methods)\n", n->method_count);
            break;
        }
        case AST_METHODS_BLOCK: {
            MethodsBlockNode* n = (MethodsBlockNode*)node;
            print_prefix(depth, C_GREEN, "MethodsBlock");
            printf("(for=\""); print_str(n->type_name); 
            printf("\", %d methods)\n", n->method_count);
            for (int i = 0; i < n->method_count; i++) {
                print_node((ASTNode*)&n->methods[i], depth + 1);
            }
            break;
        }
        case AST_IMPORT: {
            ImportNode* n = (ImportNode*)node;
            print_prefix(depth, C_GRAY, "Import");
            printf("(path=\""); print_str(n->path); 
            printf("\", as=\""); print_str(n->alias); printf("\")\n");
            break;
        }
        case AST_TYPE: {
            TypeNode* n = (TypeNode*)node;
            print_prefix(depth, C_BLUE, "Type");
            printf("("); print_str(n->name); 
            if (n->is_pointer) printf("*");
            printf(")\n");
            break;
        }
        default:
            print_prefix(depth, C_RED, "Unknown");
            printf("(type=%d)\n", node->type);
            break;
    }
}

void ast_print(ASTNode* node, int depth) {
    g_use_colors = false;
    printf("%s=== AST ===%s\n", col(C_BOLD), col(C_RESET));
    print_node(node, depth);
}

void ast_print_colored(ASTNode* node, int depth) {
    g_use_colors = true;
    printf("%s=== AST ===%s\n", col(C_BOLD), col(C_RESET));
    print_node(node, depth);
}

void ast_print_node(ASTNode* node, bool colored) {
    g_use_colors = colored;
    print_node(node, 0);
}
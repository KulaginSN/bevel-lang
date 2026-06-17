// src/parser/ast.c
#include "parser/ast.h"
#include <string.h>

// ===== Литералы =====

IntLiteralNode* ast_int_literal(Arena* arena, long long value, int line, int col) {
    IntLiteralNode* node = ARENA_ALLOC(arena, IntLiteralNode);
    node->base.type = AST_INT_LITERAL;
    node->base.line = line;
    node->base.column = col;
    node->value = value;
    return node;
}

FloatLiteralNode* ast_float_literal(Arena* arena, double value, int line, int col) {
    FloatLiteralNode* node = ARENA_ALLOC(arena, FloatLiteralNode);
    node->base.type = AST_FLOAT_LITERAL;
    node->base.line = line;
    node->base.column = col;
    node->value = value;
    return node;
}

StringLiteralNode* ast_string_literal(Arena* arena, String value, int line, int col) {
    StringLiteralNode* node = ARENA_ALLOC(arena, StringLiteralNode);
    node->base.type = AST_STRING_LITERAL;
    node->base.line = line;
    node->base.column = col;
    node->value = arena_string(arena, value);
    return node;
}

CharLiteralNode* ast_char_literal(Arena* arena, char value, int line, int col) {
    CharLiteralNode* node = ARENA_ALLOC(arena, CharLiteralNode);
    node->base.type = AST_CHAR_LITERAL;
    node->base.line = line;
    node->base.column = col;
    node->value = value;
    return node;
}

ASTNode* ast_array_literal(Arena* arena, ASTNode** elements, int element_count, int line, int column) {
    ArrayLiteralNode* node = ARENA_ALLOC(arena, ArrayLiteralNode);
    node->base.type = AST_ARRAY_LITERAL;
    node->base.line = line;
    node->base.column = column;
    node->elements = elements;
    node->element_count = element_count;
    return (ASTNode*)node;
}

BoolLiteralNode* ast_bool_literal(Arena* arena, bool value, int line, int col) {
    BoolLiteralNode* node = ARENA_ALLOC(arena, BoolLiteralNode);
    node->base.type = AST_BOOL_LITERAL;
    node->base.line = line;
    node->base.column = col;
    node->value = value;
    return node;
}

// ===== Выражения =====

IdentifierNode* ast_identifier(Arena* arena, String name, int line, int col) {
    IdentifierNode* node = ARENA_ALLOC(arena, IdentifierNode);
    node->base.type = AST_IDENTIFIER;
    node->base.line = line;
    node->base.column = col;
    node->name = arena_string(arena, name);
    return node;
}

BinaryExprNode* ast_binary_expr(Arena* arena, TokenType op, ASTNode* left, ASTNode* right, int line, int col) {
    BinaryExprNode* node = ARENA_ALLOC(arena, BinaryExprNode);
    node->base.type = AST_BINARY_EXPR;
    node->base.line = line;
    node->base.column = col;
    node->op = op;
    node->left = left;
    node->right = right;
    return node;
}

UnaryExprNode* ast_unary_expr(Arena* arena, TokenType op, ASTNode* operand, int line, int col) {
    UnaryExprNode* node = ARENA_ALLOC(arena, UnaryExprNode);
    node->base.type = AST_UNARY_EXPR;
    node->base.line = line;
    node->base.column = col;
    node->op = op;
    node->operand = operand;
    return node;
}

CallExprNode* ast_call_expr(Arena* arena, ASTNode* callee, ASTNode** args, int arg_count, int line, int col) {
    CallExprNode* node = ARENA_ALLOC(arena, CallExprNode);
    node->base.type = AST_CALL_EXPR;
    node->base.line = line;
    node->base.column = col;
    node->callee = callee;
    node->args = args;
    node->arg_count = arg_count;
    return node;
}

IndexExprNode* ast_index_expr(Arena* arena, ASTNode* object, ASTNode* index, int line, int col) {
    IndexExprNode* node = ARENA_ALLOC(arena, IndexExprNode);
    node->base.type = AST_INDEX_EXPR;
    node->base.line = line;
    node->base.column = col;
    node->object = object;
    node->index = index;
    return node;
}

FieldAccessNode* ast_field_access(Arena* arena, ASTNode* object, String field, int line, int col) {
    FieldAccessNode* node = ARENA_ALLOC(arena, FieldAccessNode);
    node->base.type = AST_FIELD_ACCESS;
    node->base.line = line;
    node->base.column = col;
    node->object = object;
    node->field = arena_string(arena, field);
    return node;
}

SelfExprNode* ast_self_expr(Arena* arena, int line, int col) {
    SelfExprNode* node = ARENA_ALLOC(arena, SelfExprNode);
    node->base.type = AST_SELF_EXPR;
    node->base.line = line;
    node->base.column = col;
    return node;
}

// ===== Операторы =====
ASTNode* ast_assign(Arena* arena, ASTNode* target, ASTNode* value, int line, int column) {
    AssignNode* node = ARENA_ALLOC(arena, AssignNode);
    node->base.type = AST_ASSIGN;
    node->base.line = line;
    node->base.column = column;
    node->target = target;
    node->value = value;
    return (ASTNode*)node;
}

ASTNode* ast_pointer_type(Arena* arena, ASTNode* base_type, int line, int column) {
    PointerTypeNode* node = ARENA_ALLOC(arena, PointerTypeNode);
    node->base.type = AST_POINTER_TYPE;
    node->base.line = line;
    node->base.column = column;
    node->base_type = base_type;
    return (ASTNode*)node;
}

VarDeclNode* ast_var_decl(Arena* arena, ASTNode* type, String name, ASTNode* initializer, int line, int col) {
    VarDeclNode* node = ARENA_ALLOC(arena, VarDeclNode);
    node->base.type = AST_VAR_DECL;
    node->base.line = line;
    node->base.column = col;
    node->type = type;
    node->name = arena_string(arena, name);
    node->initializer = initializer;
    return node;
}

ReturnNode* ast_return(Arena* arena, ASTNode* value, int line, int col) {
    ReturnNode* node = ARENA_ALLOC(arena, ReturnNode);
    node->base.type = AST_RETURN;
    node->base.line = line;
    node->base.column = col;
    node->value = value;
    return node;
}

ASTNode* ast_addr_of(Arena* arena, ASTNode* operand, int line, int column) {
    AddrOfNode* node = ARENA_ALLOC(arena, AddrOfNode);
    node->base.type = AST_ADDR_OF;
    node->base.line = line;
    node->base.column = column;
    node->operand = operand;
    return (ASTNode*)node;
}

ASTNode* ast_deref(Arena* arena, ASTNode* operand, int line, int column) {
    DerefNode* node = ARENA_ALLOC(arena, DerefNode);
    node->base.type = AST_DEREF;
    node->base.line = line;
    node->base.column = column;
    node->operand = operand;
    return (ASTNode*)node;
}

// ===== Управляющие конструкции =====

IfNode* ast_if(Arena* arena, ASTNode* condition, ASTNode* then_branch, ASTNode* else_branch, int line, int col) {
    IfNode* node = ARENA_ALLOC(arena, IfNode);
    node->base.type = AST_IF;
    node->base.line = line;
    node->base.column = col;
    node->condition = condition;
    node->then_branch = then_branch;
    node->else_branch = else_branch;
    return node;
}

ForNode* ast_for(Arena* arena, ASTNode* init, ASTNode* condition, ASTNode* increment, ASTNode* body, int line, int col) {
    ForNode* node = ARENA_ALLOC(arena, ForNode);
    node->base.type = AST_FOR;
    node->base.line = line;
    node->base.column = col;
    node->init = init;
    node->condition = condition;
    node->increment = increment;
    node->body = body;
    return node;
}

WhileNode* ast_while(Arena* arena, ASTNode* condition, ASTNode* body, int line, int col) {
    WhileNode* node = ARENA_ALLOC(arena, WhileNode);
    node->base.type = AST_WHILE;
    node->base.line = line;
    node->base.column = col;
    node->condition = condition;
    node->body = body;
    return node;
}

SwitchNode* ast_switch(Arena* arena, ASTNode* value, ASTNode** cases, int case_count, int line, int col) {
    SwitchNode* node = ARENA_ALLOC(arena, SwitchNode);
    node->base.type = AST_SWITCH;
    node->base.line = line;
    node->base.column = col;
    node->value = value;
    node->cases = cases;
    node->case_count = case_count;
    return node;
}

CaseNode* ast_case(Arena* arena, ASTNode* value, ASTNode* body, int line, int col) {
    CaseNode* node = ARENA_ALLOC(arena, CaseNode);
    node->base.type = AST_CASE;
    node->base.line = line;
    node->base.column = col;
    node->value = value;
    node->body = body;
    return node;
}

BlockNode* ast_block(Arena* arena, ASTNode** statements, int statement_count, int line, int col) {
    BlockNode* node = ARENA_ALLOC(arena, BlockNode);
    node->base.type = AST_BLOCK;
    node->base.line = line;
    node->base.column = col;
    node->statements = statements;
    node->statement_count = statement_count;
    return node;
}

ASTNode* ast_break(Arena* arena, int line, int column) {
    BreakNode* node = ARENA_ALLOC(arena, BreakNode);
    node->base.type = AST_BREAK;
    node->base.line = line;
    node->base.column = column;
    return (ASTNode*)node;
}

ASTNode* ast_continue(Arena* arena, int line, int column) {
    ContinueNode* node = ARENA_ALLOC(arena, ContinueNode);
    node->base.type = AST_CONTINUE;
    node->base.line = line;
    node->base.column = column;
    return (ASTNode*)node;
}

// ===== Объявления =====

FunctionDeclNode* ast_function_decl(Arena* arena, String name, ASTNode* return_type,
                                     ParameterNode* params, int param_count,
                                     ASTNode* body, int line, int col) {
    FunctionDeclNode* node = ARENA_ALLOC(arena, FunctionDeclNode);
    node->base.type = AST_FUNCTION_DECL;
    node->base.line = line;
    node->base.column = col;
    node->name = arena_string(arena, name);
    node->return_type = return_type;
    node->params = params;
    node->param_count = param_count;
    node->body = body;
    node->is_generic = false;
    node->generic_params = NULL;
    node->generic_param_count = 0;
    node->where_clauses = NULL;
    node->where_clause_count = 0;
    return node;
}

StructDeclNode* ast_struct_decl(Arena* arena, String name, FieldNode* fields, int field_count,
                                bool is_generic, GenericParamNode* generic_params, int generic_param_count,
                                WhereClauseNode* where_clauses, int where_clause_count,  // ← ДОБАВИТЬ
                                int line, int col) {
    StructDeclNode* node = ARENA_ALLOC(arena, StructDeclNode);
    node->base.type = AST_STRUCT_DECL;
    node->base.line = line;
    node->base.column = col;
    node->base.next = NULL;
    node->name = name;
    node->fields = fields;
    node->field_count = field_count;
    node->is_generic = is_generic;
    node->generic_params = generic_params;
    node->generic_param_count = generic_param_count;
    node->where_clauses = where_clauses;      // ← ДОБАВИТЬ
    node->where_clause_count = where_clause_count;  // ← ДОБАВИТЬ
    return node;
}

EnumDeclNode* ast_enum_decl(Arena* arena, String name, ASTNode* base_type,
                             EnumValueNode* values, int value_count, int line, int col) {
    EnumDeclNode* node = ARENA_ALLOC(arena, EnumDeclNode);
    node->base.type = AST_ENUM_DECL;
    node->base.line = line;
    node->base.column = col;
    node->name = arena_string(arena, name);
    node->base_type = base_type;
    node->values = values;
    node->value_count = value_count;
    return node;
}

InterfaceDeclNode* ast_interface_decl(Arena* arena, String name,
                                       InterfaceMethodNode* methods, int method_count, int line, int col) {
    InterfaceDeclNode* node = ARENA_ALLOC(arena, InterfaceDeclNode);
    node->base.type = AST_INTERFACE_DECL;
    node->base.line = line;
    node->base.column = col;
    node->name = arena_string(arena, name);
    node->methods = methods;
    node->method_count = method_count;
    return node;
}

MethodsBlockNode* ast_methods_block(Arena* arena, String type_name,
                                     FunctionDeclNode* methods, int method_count, int line, int col) {
    MethodsBlockNode* node = ARENA_ALLOC(arena, MethodsBlockNode);
    node->base.type = AST_METHODS_BLOCK;
    node->base.line = line;
    node->base.column = col;
    node->type_name = arena_string(arena, type_name);
    node->methods = methods;
    node->method_count = method_count;
    return node;
}

ImportNode* ast_import(Arena* arena, String path, String alias, int line, int col) {
    ImportNode* node = ARENA_ALLOC(arena, ImportNode);
    node->base.type = AST_IMPORT;
    node->base.line = line;
    node->base.column = col;
    node->path = arena_string(arena, path);
    node->alias = arena_string(arena, alias);
    return node;
}

// ===== Типы =====

TypeNode* ast_type(Arena* arena, TokenType token_type, String name, bool is_pointer, 
                   ASTNode** type_args, int type_arg_count, int line, int col) {
    TypeNode* node = ARENA_ALLOC(arena, TypeNode);
    node->base.type = AST_TYPE;
    node->base.line = line;
    node->base.column = col;
    node->token_type = token_type;
    node->name = arena_string(arena, name);
    node->is_pointer = is_pointer;
    node->type_args = type_args;
    node->type_arg_count = type_arg_count;
    return node;
}

ASTNode* ast_array_type(Arena* arena, ASTNode* element_type, ASTNode* size, bool is_slice, int line, int column) {
    ArrayTypeNode* node = ARENA_ALLOC(arena, ArrayTypeNode);
    node->base.type = AST_ARRAY_TYPE;
    node->base.line = line;
    node->base.column = column;
    node->element_type = element_type;
    node->size = size;
    node->is_slice = is_slice;
    return (ASTNode*)node;
}

ASTNode* ast_index_access(Arena* arena, ASTNode* array, ASTNode* index, int line, int column) {
    IndexAccessNode* node = ARENA_ALLOC(arena, IndexAccessNode);
    node->base.type = AST_INDEX_ACCESS;
    node->base.line = line;
    node->base.column = column;
    node->array = array;
    node->index = index;
    return (ASTNode*)node;
}

TypeAsExprNode* ast_type_as_expr(Arena* arena, ASTNode* type_node, int line, int col) {
    TypeAsExprNode* node = ARENA_ALLOC(arena, TypeAsExprNode);
    node->base.type = AST_TYPE_AS_EXPR;
    node->base.line = line;
    node->base.column = col;
    node->type_node = type_node;
    return node;
}
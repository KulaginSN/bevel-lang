// src/parser/ast.h
#ifndef BEVEL_AST_H
#define BEVEL_AST_H

#include "common/string.h"
#include "common/arena.h"
#include "lexer/token.h"

// Типы узлов AST
typedef enum {
    // Литералы
    AST_INT_LITERAL,
    AST_FLOAT_LITERAL,
    AST_STRING_LITERAL,
    AST_CHAR_LITERAL,
    AST_BOOL_LITERAL,
    AST_ARRAY_LITERAL,  // Литерал массива: [1, 2, 3, 4, 5]
    
    // Выражения
    AST_IDENTIFIER,
    AST_BINARY_EXPR,
    AST_UNARY_EXPR,
    AST_CALL_EXPR,
    AST_INDEX_EXPR,
    AST_FIELD_ACCESS,
    AST_SELF_EXPR,
    AST_TYPE_AS_EXPR,
    AST_ARRAY_TYPE,      // Тип массива: [T; N] или []T
    AST_INDEX_ACCESS,    // Доступ по индексу: arr[i]
    
    // Операторы
    AST_ASSIGN,
    AST_VAR_DECL,
    AST_RETURN,
    AST_ADDR_OF,   // &variable
    AST_DEREF,     // *pointer
    
    // Управляющие конструкции
    AST_IF,
    AST_FOR,
    AST_WHILE,
    AST_BREAK,
    AST_CONTINUE,
    AST_SWITCH,
    AST_CASE,
    AST_BLOCK,
    
    // Объявления
    AST_FUNCTION_DECL,
    AST_STRUCT_DECL,
    AST_ENUM_DECL,
    AST_INTERFACE_DECL,
    AST_METHODS_BLOCK,
    AST_IMPORT,
    
    // Типы
    AST_TYPE,
    AST_GENERIC_PARAM,
    AST_WHERE_CLAUSE,
    
    AST_NODE_COUNT,
    AST_POINTER_TYPE,   // Тип указателя: *T
} ASTNodeType;

// Базовая структура для всех узлов
typedef struct ASTNode {
    ASTNodeType type;
    int line;
    int column;
    struct ASTNode* next;  // Для связных списков
} ASTNode;

// ===== Литералы =====

typedef struct {
    ASTNode base;
    long long value;
} IntLiteralNode;

typedef struct {
    ASTNode base;
    double value;
} FloatLiteralNode;

typedef struct {
    ASTNode base;
    String value;  // Без кавычек
} StringLiteralNode;

typedef struct {
    ASTNode base;
    char value;
} CharLiteralNode;

typedef struct {
    ASTNode base;
    bool value;
} BoolLiteralNode;

typedef struct {
    ASTNode base;
    ASTNode** elements;
    int element_count;
} ArrayLiteralNode;


// ===== Выражения =====

typedef struct {
    ASTNode base;
    String name;
} IdentifierNode;

typedef struct {
    ASTNode base;
    TokenType op;
    ASTNode* left;
    ASTNode* right;
} BinaryExprNode;

typedef struct {
    ASTNode base;
    TokenType op;
    ASTNode* operand;
} UnaryExprNode;

typedef struct {
    ASTNode base;
    ASTNode* callee;
    ASTNode** args;
    int arg_count;
} CallExprNode;

typedef struct {
    ASTNode base;
    ASTNode* object;
    ASTNode* index;
} IndexExprNode;

typedef struct {
    ASTNode base;
    ASTNode* object;
    String field;
} FieldAccessNode;

typedef struct {
    ASTNode base;
} SelfExprNode;

// ===== Операторы =====

typedef struct {
    ASTNode base;
    ASTNode* target;  // Левая часть (например, переменная)
    ASTNode* value;   // Правая часть (выражение)
} AssignNode;

typedef struct {
    ASTNode base;
    ASTNode* type;
    String name;
    ASTNode* initializer;
} VarDeclNode;

typedef struct {
    ASTNode base;
    ASTNode* value;  // Может быть NULL
} ReturnNode;

typedef struct {
    ASTNode base;
    ASTNode* operand;
} AddrOfNode;

typedef struct {
    ASTNode base;
    ASTNode* operand;
} DerefNode;

typedef struct {
    ASTNode base;
    ASTNode* base_type; // Тип, на который указывает указатель
} PointerTypeNode;

// ===== Управляющие конструкции =====

typedef struct {
    ASTNode base;
    ASTNode* condition;
    ASTNode* then_branch;
    ASTNode* else_branch;  // Может быть NULL
} IfNode;

typedef struct {
    ASTNode base;
    ASTNode* init;
    ASTNode* condition;
    ASTNode* increment;
    ASTNode* body;
} ForNode;

typedef struct {
    ASTNode base;
    ASTNode* condition;
    ASTNode* body;
} WhileNode;

typedef struct {
    ASTNode base;
    ASTNode* value;
    ASTNode** cases;
    int case_count;
} SwitchNode;

typedef struct {
    ASTNode base;
    ASTNode* value;  // NULL для default
    ASTNode* body;
} CaseNode;

typedef struct {
    ASTNode base;
    ASTNode** statements;
    int statement_count;
} BlockNode;

// ===== Объявления =====

typedef struct {
    String name;
    ASTNode* type;
} ParameterNode;

typedef struct {
    ASTNode base;
    String name;
    ASTNode* return_type;
    ParameterNode* params;
    int param_count;
    ASTNode* body;
    bool is_generic;
    struct GenericParamNode* generic_params;
    int generic_param_count;
    struct WhereClauseNode* where_clauses;
    int where_clause_count;
} FunctionDeclNode;

typedef struct {
    String name;
    ASTNode* type;
} FieldNode;

typedef struct {
    ASTNode base;
    String name;
    FieldNode* fields;
    int field_count;
    
    // НОВОЕ: Поддержка дженериков
    bool is_generic;
    struct GenericParamNode* generic_params;
    int generic_param_count;
    
    // ДОБАВЬТЕ ЭТИ ДВА ПОЛЯ:
    struct WhereClauseNode* where_clauses;
    int where_clause_count;
} StructDeclNode;

typedef struct {
    String name;
    long long value;
} EnumValueNode;

typedef struct {
    ASTNode base;
    String name;
    ASTNode* base_type;
    EnumValueNode* values;
    int value_count;
} EnumDeclNode;

typedef struct {
    String name;
    ASTNode* return_type;
    ParameterNode* params;
    int param_count;
} InterfaceMethodNode;

typedef struct {
    ASTNode base;
    String name;
    InterfaceMethodNode* methods;
    int method_count;
} InterfaceDeclNode;

typedef struct {
    ASTNode base;
    String type_name;
    FunctionDeclNode* methods;
    int method_count;
} MethodsBlockNode;

typedef struct {
    ASTNode base;
    String path;
    String alias;
} ImportNode;

// ===== Типы =====

typedef struct {
    ASTNode base;
    TokenType token_type;
    String name;
    bool is_pointer;
    ASTNode* element_type;  // Для массивов
    int array_size;
    struct ASTNode** type_args;
    int type_arg_count;
} TypeNode;

// ===== Дженерики =====

typedef struct GenericParamNode {
    String name;
    struct GenericParamNode* next;
} GenericParamNode;

typedef struct WhereClauseNode {
    String type_param;
    String interface_name;
    struct WhereClauseNode* next;
} WhereClauseNode;

typedef struct {
    ASTNode base;
    // Break/Continue не имеют полей
} BreakNode;

typedef struct {
    ASTNode base;
} ContinueNode;

typedef struct {
    ASTNode base;
    ASTNode* element_type;  // Тип элементов
    ASTNode* size;          // Размер (NULL для срезов []T)
    bool is_slice;          // true для []T, false для [T; N]
} ArrayTypeNode;

typedef struct {
    ASTNode base;
    ASTNode* array;         // Массив или срез
    ASTNode* index;         // Индекс
} IndexAccessNode;

ASTNode* ast_array_type(Arena* arena, ASTNode* element_type, ASTNode* size, bool is_slice, int line, int column);
ASTNode* ast_index_access(Arena* arena, ASTNode* array, ASTNode* index, int line, int column);

// НОВОЕ: Тип как выражение (для generic вызовов)
typedef struct {
    ASTNode base;
    ASTNode* type_node;  // Сам тип (TypeNode)
} TypeAsExprNode;


// ===== Функции создания узлов =====

// Литералы
IntLiteralNode* ast_int_literal(Arena* arena, long long value, int line, int col);
FloatLiteralNode* ast_float_literal(Arena* arena, double value, int line, int col);
StringLiteralNode* ast_string_literal(Arena* arena, String value, int line, int col);
CharLiteralNode* ast_char_literal(Arena* arena, char value, int line, int col);
BoolLiteralNode* ast_bool_literal(Arena* arena, bool value, int line, int col);
ASTNode* ast_array_literal(Arena* arena, ASTNode** elements, int element_count, int line, int column);

// Выражения
IdentifierNode* ast_identifier(Arena* arena, String name, int line, int col);
BinaryExprNode* ast_binary_expr(Arena* arena, TokenType op, ASTNode* left, ASTNode* right, int line, int col);
UnaryExprNode* ast_unary_expr(Arena* arena, TokenType op, ASTNode* operand, int line, int col);
CallExprNode* ast_call_expr(Arena* arena, ASTNode* callee, ASTNode** args, int arg_count, int line, int col);
IndexExprNode* ast_index_expr(Arena* arena, ASTNode* object, ASTNode* index, int line, int col);
FieldAccessNode* ast_field_access(Arena* arena, ASTNode* object, String field, int line, int col);
SelfExprNode* ast_self_expr(Arena* arena, int line, int col);

// Операторы
ASTNode* ast_assign(Arena* arena, ASTNode* target, ASTNode* value, int line, int column);
VarDeclNode* ast_var_decl(Arena* arena, ASTNode* type, String name, ASTNode* initializer, int line, int col);
ReturnNode* ast_return(Arena* arena, ASTNode* value, int line, int col);
ASTNode* ast_addr_of(Arena* arena, ASTNode* operand, int line, int column);
ASTNode* ast_deref(Arena* arena, ASTNode* operand, int line, int column);
ASTNode* ast_pointer_type(Arena* arena, ASTNode* base_type, int line, int column);

// Управляющие конструкции
IfNode* ast_if(Arena* arena, ASTNode* condition, ASTNode* then_branch, ASTNode* else_branch, int line, int col);
ForNode* ast_for(Arena* arena, ASTNode* init, ASTNode* condition, ASTNode* increment, ASTNode* body, int line, int col);
WhileNode* ast_while(Arena* arena, ASTNode* condition, ASTNode* body, int line, int col);
ASTNode* ast_break(Arena* arena, int line, int column);
ASTNode* ast_continue(Arena* arena, int line, int column);
SwitchNode* ast_switch(Arena* arena, ASTNode* value, ASTNode** cases, int case_count, int line, int col);
CaseNode* ast_case(Arena* arena, ASTNode* value, ASTNode* body, int line, int col);
BlockNode* ast_block(Arena* arena, ASTNode** statements, int statement_count, int line, int col);

// Объявления
FunctionDeclNode* ast_function_decl(Arena* arena, String name, ASTNode* return_type,
                                     ParameterNode* params, int param_count,
                                     ASTNode* body, int line, int col);
StructDeclNode* ast_struct_decl(Arena* arena, String name, FieldNode* fields, int field_count,
                                bool is_generic, struct GenericParamNode* generic_params, int generic_param_count,
                                struct WhereClauseNode* where_clauses, int where_clause_count,
                                int line, int col);
EnumDeclNode* ast_enum_decl(Arena* arena, String name, ASTNode* base_type,
                             EnumValueNode* values, int value_count, int line, int col);
InterfaceDeclNode* ast_interface_decl(Arena* arena, String name,
                                       InterfaceMethodNode* methods, int method_count, int line, int col);
MethodsBlockNode* ast_methods_block(Arena* arena, String type_name,
                                     FunctionDeclNode* methods, int method_count, int line, int col);
ImportNode* ast_import(Arena* arena, String path, String alias, int line, int col);

ASTNode* ast_array_type(Arena* arena, ASTNode* element_type, ASTNode* size, bool is_slice, int line, int column);
ASTNode* ast_index_access(Arena* arena, ASTNode* array, ASTNode* index, int line, int column);
TypeAsExprNode* ast_type_as_expr(Arena* arena, ASTNode* type_node, int line, int col);

// Типы
TypeNode* ast_type(Arena* arena, TokenType token_type, String name, bool is_pointer,
                   ASTNode** type_args, int type_arg_count, int line, int col);

#endif // BEVEL_AST_H
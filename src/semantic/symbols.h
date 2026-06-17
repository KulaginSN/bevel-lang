// src/semantic/symbols.h
#ifndef BEVEL_SYMBOLS_H
#define BEVEL_SYMBOLS_H

#include "common/string.h"
#include "common/arena.h"
#include "semantic/types.h"
#include "parser/ast.h"

typedef enum {
    SYMBOL_VARIABLE,
    SYMBOL_FUNCTION,
    SYMBOL_STRUCT,
    SYMBOL_ENUM,
    SYMBOL_INTERFACE,
    SYMBOL_TYPE_ALIAS,
    SYMBOL_GENERIC_PARAM,
    SYMBOL_MODULE
} SymbolKind;

typedef struct Symbol {
    String name;
    SymbolKind kind;
    Type* type;
    ASTNode* declaration;   // Узел AST для сообщений об ошибках
    bool is_mutable;
    bool is_used;           // Для проверки неиспользуемых переменных
    int scope_level;
    struct Symbol* next;    // Для хеш-цепочки
} Symbol;

typedef struct Scope {
    Symbol** table;         // Хеш-таблица
    int capacity;
    int count;
    struct Scope* parent;
    int level;
} Scope;

typedef struct SymbolTable {
    Scope* current_scope;
    Arena* arena;
    int scope_depth;
} SymbolTable;

// ===== Инициализация и очистка =====
void symbol_table_init(SymbolTable* st, Arena* arena);
// Очистка не нужна — всё в арене

// ===== Области видимости =====
void scope_enter(SymbolTable* st);
void scope_leave(SymbolTable* st);
int  scope_level(SymbolTable* st);

// ===== Добавление символов =====
bool symbol_define(SymbolTable* st, String name, SymbolKind kind, 
                    Type* type, ASTNode* decl);
Symbol* symbol_lookup(SymbolTable* st, String name);
Symbol* symbol_lookup_local(SymbolTable* st, String name); // Только в текущем scope

// ===== Работа с типами верхнего уровня =====
bool symbol_define_type(SymbolTable* st, String name, Type* type, ASTNode* decl);
Type* symbol_lookup_type(SymbolTable* st, String name);

// ===== Для отладки =====
void symbol_table_dump(SymbolTable* st);
// Регистрация типа в глобальном scope (для инстанцированных дженериков)
bool symbol_define_type_global(SymbolTable* st, String name, Type* type, ASTNode* decl);


#endif // BEVEL_SYMBOLS_H
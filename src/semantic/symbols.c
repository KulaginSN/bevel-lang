// src/semantic/symbols.c
#include "semantic/symbols.h"
#include "common/utils.h"
#include <stdio.h>
#include <string.h>

#define INITIAL_SCOPE_CAPACITY 64

// ===== Вспомогательные функции =====

static Scope* scope_new(Arena* arena, Scope* parent, int level) {
    Scope* s = ARENA_ALLOC(arena, Scope);
    s->capacity = INITIAL_SCOPE_CAPACITY;
    s->table = ARENA_ARRAY(arena, Symbol*, s->capacity);
    for (int i = 0; i < s->capacity; i++) s->table[i] = NULL;
    s->count = 0;
    s->parent = parent;
    s->level = level;
    return s;
}

static int hash_slot(String name, int capacity) {
    return string_hash(name) % capacity;
}

// ===== Инициализация =====

void symbol_table_init(SymbolTable* st, Arena* arena) {
    st->arena = arena;
    st->current_scope = scope_new(arena, NULL, 0);
    st->scope_depth = 0;
}

// ===== Области видимости =====

void scope_enter(SymbolTable* st) {
    st->scope_depth++;
    st->current_scope = scope_new(st->arena, st->current_scope, st->scope_depth);
}

void scope_leave(SymbolTable* st) {
    if (!st->current_scope || !st->current_scope->parent) return;
    st->current_scope = st->current_scope->parent;
    st->scope_depth--;
}

int scope_level(SymbolTable* st) {
    return st->scope_depth;
}

// ===== Добавление и поиск символов =====

bool symbol_define(SymbolTable* st, String name, SymbolKind kind,
                    Type* type, ASTNode* decl) {
    if (!st->current_scope) return false;
    
    // Проверяем, не определён ли символ в текущей области
    Symbol* existing = symbol_lookup_local(st, name);
    if (existing) {
        // Разрешаем дубликаты для инстанцированных generic структур
        if (kind == SYMBOL_STRUCT && type && type->kind == TYPE_STRUCT && 
            type->structure.type_arg_count > 0) {
            // Возвращаем существующий тип вместо создания нового
            return true; // Фактически не создаём новый символ, но не считаем это ошибкой
        }
        return false; // Дубликат
    }
    
    Symbol* sym = ARENA_ALLOC(st->arena, Symbol);
    sym->name = arena_string(st->arena, name);
    sym->kind = kind;
    sym->type = type;
    sym->declaration = decl;
    sym->is_mutable = true;  // По умолчанию переменные изменяемые
    sym->is_used = false;
    sym->scope_level = st->scope_depth;
    
    // Вставляем в хеш-таблицу (в начало цепочки)
    int slot = hash_slot(name, st->current_scope->capacity);
    sym->next = st->current_scope->table[slot];
    st->current_scope->table[slot] = sym;
    st->current_scope->count++;
    
    return true;
}

Symbol* symbol_lookup_local(SymbolTable* st, String name) {
    if (!st->current_scope) return NULL;
    int slot = hash_slot(name, st->current_scope->capacity);
    for (Symbol* s = st->current_scope->table[slot]; s; s = s->next) {
        if (string_equals(s->name, name)) return s;
    }
    return NULL;
}

Symbol* symbol_lookup(SymbolTable* st, String name) {
    for (Scope* s = st->current_scope; s; s = s->parent) {
        int slot = hash_slot(name, s->capacity);
        for (Symbol* sym = s->table[slot]; sym; sym = sym->next) {
            if (string_equals(sym->name, name)) return sym;
        }
    }
    return NULL;
}

// ===== Работа с типами =====

bool symbol_define_type(SymbolTable* st, String name, Type* type, ASTNode* decl) {
    SymbolKind kind;
    switch (type->kind) {
        case TYPE_STRUCT:    kind = SYMBOL_STRUCT; break;
        case TYPE_ENUM:      kind = SYMBOL_ENUM; break;
        case TYPE_INTERFACE: kind = SYMBOL_INTERFACE; break;
        default:             kind = SYMBOL_TYPE_ALIAS; break;
    }
    return symbol_define(st, name, kind, type, decl);
}

Type* symbol_lookup_type(SymbolTable* st, String name) {
    Symbol* sym = symbol_lookup(st, name);
    if (!sym) return NULL;
    switch (sym->kind) {
        case SYMBOL_STRUCT:
        case SYMBOL_ENUM:
        case SYMBOL_INTERFACE:
        case SYMBOL_TYPE_ALIAS:
            return sym->type;
        default:
            return NULL;
    }
}

// ===== Отладка =====

static const char* kind_to_str(SymbolKind k) {
    switch (k) {
        case SYMBOL_VARIABLE:      return "variable";
        case SYMBOL_FUNCTION:      return "function";
        case SYMBOL_STRUCT:        return "struct";
        case SYMBOL_ENUM:          return "enum";
        case SYMBOL_INTERFACE:     return "interface";
        case SYMBOL_TYPE_ALIAS:    return "alias";
        case SYMBOL_GENERIC_PARAM: return "generic";
        case SYMBOL_MODULE:        return "module";
    }
    return "unknown";
}

void symbol_table_dump(SymbolTable* st) {
    printf("=== Symbol Table (depth=%d) ===\n", st->scope_depth);
    for (Scope* s = st->current_scope; s; s = s->parent) {
        printf("  [Scope level %d, %d symbols]\n", s->level, s->count);
        for (int i = 0; i < s->capacity; i++) {
            for (Symbol* sym = s->table[i]; sym; sym = sym->next) {
                printf("    %.*s : %s\n",
                    (int)sym->name.length, sym->name.data,
                    kind_to_str(sym->kind));
            }
        }
    }
}

bool symbol_define_type_global(SymbolTable* st, String name, Type* type, ASTNode* decl) {
    // Находим самый верхний (глобальный) scope
    Scope* global_scope = st->current_scope;
    while (global_scope && global_scope->parent) {
        global_scope = global_scope->parent;
    }
    
    if (!global_scope) return false;
    
    // Временно переключаемся на глобальный scope
    Scope* saved_scope = st->current_scope;
    st->current_scope = global_scope;
    
    // Регистрируем
    bool result = symbol_define_type(st, name, type, decl);
    
    // Восстанавливаем текущий scope
    st->current_scope = saved_scope;
    
    return result;
}
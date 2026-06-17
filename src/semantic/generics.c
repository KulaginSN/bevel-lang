// src/semantic/generics.c
#include "semantic/generics.h"
#include "semantic/analyzer.h"
#include "common/utils.h"
#include <stdio.h>
#include <string.h>

// ===== Управление generic параметрами =====

GenericParam* generic_param_new(Arena* arena, String name) {
    GenericParam* p = ARENA_ALLOC(arena, GenericParam);
    p->name = arena_string(arena, name);
    p->param_type = type_generic_param_new(arena, name, NULL);
    p->constraints = NULL;
    p->next = NULL;
    return p;
}

void generic_param_add_constraint(Arena* arena, GenericParam* param, Type* interface_type) {
    if (!param || !interface_type) return;
    
    GenericConstraint* c = ARENA_ALLOC(arena, GenericConstraint);
    c->param_name = param->name;
    c->interface_type = interface_type;
    c->next = NULL;
    
    // Добавляем в конец списка
    if (!param->constraints) {
        param->constraints = c;
    } else {
        GenericConstraint* cur = param->constraints;
        while (cur->next) cur = cur->next;
        cur->next = c;
    }
}

GenericParam* generic_param_find(GenericParam* params, String name) {
    for (GenericParam* p = params; p; p = p->next) {
        if (string_equals(p->name, name)) return p;
    }
    return NULL;
}

// ===== Инстанцирование дженериков =====

GenericContext* generic_context_new(Arena* arena, GenericParam* params, int count) {
    GenericContext* ctx = ARENA_ALLOC(arena, GenericContext);
    ctx->params = params;
    ctx->param_count = count;
    ctx->concrete_types = ARENA_ARRAY(arena, Type*, count);
    for (int i = 0; i < count; i++) {
        ctx->concrete_types[i] = NULL;
    }
    return ctx;
}

void generic_context_bind(GenericContext* ctx, String param_name, Type* concrete_type) {
    if (!ctx) return;
    
    int idx = 0;
    for (GenericParam* p = ctx->params; p; p = p->next, idx++) {
        if (string_equals(p->name, param_name)) {
            if (idx < ctx->param_count) {
                ctx->concrete_types[idx] = concrete_type;
            }
            return;
        }
    }
}

Type* generic_instantiate_type(Arena* arena, GenericContext* ctx, Type* type) {
    if (!type || !ctx) return type;
    
    // Если это generic параметр, заменяем на конкретный тип
    if (type->kind == TYPE_GENERIC_PARAM) {
        int idx = 0;
        for (GenericParam* p = ctx->params; p; p = p->next, idx++) {
            if (string_equals(p->name, type->generic_param.name)) {
                if (idx < ctx->param_count && ctx->concrete_types[idx]) {
                    return ctx->concrete_types[idx];
                }
                return type;  // Не привязан, возвращаем как есть
            }
        }
        return type;
    }
    
    // Рекурсивно обрабатываем составные типы
    switch (type->kind) {
        case TYPE_POINTER: {
            Type* pointee = generic_instantiate_type(arena, ctx, type->pointer.pointee);
            if (pointee != type->pointer.pointee) {
                return type_pointer_new(arena, pointee);
            }
            return type;
        }
        
        case TYPE_ARRAY: {
            Type* element = generic_instantiate_type(arena, ctx, type->array.element);
            if (element != type->array.element) {
                return type_array_new(arena, element, type->array.size);
            }
            return type;
        }
        
        case TYPE_FUNCTION: {
            bool changed = false;
            Type* ret = generic_instantiate_type(arena, ctx, type->function.return_type);
            if (ret != type->function.return_type) changed = true;
            
            Type** params = NULL;
            if (type->function.param_count > 0) {
                params = ARENA_ARRAY(arena, Type*, type->function.param_count);
                for (int i = 0; i < type->function.param_count; i++) {
                    params[i] = generic_instantiate_type(arena, ctx, type->function.param_types[i]);
                    if (params[i] != type->function.param_types[i]) changed = true;
                }
            }
            
            if (changed) {
                return type_function_new(arena, ret, params, 
                                        type->function.param_names, 
                                        type->function.param_count);
            }
            return type;
        }
        
        default:
            return type;
    }
}

// ===== Проверка ограничений =====

bool generic_check_constraints(Analyzer* a, GenericParam* param, Type* concrete_type, int line, int col) {
    if (!param || !concrete_type) return true;
    
    for (GenericConstraint* c = param->constraints; c; c = c->next) {
        if (!type_implements_interface(concrete_type, c->interface_type)) {
            error_reporter_semantic_error(
                a->errors, line, col,
                format_string(a->arena, 
                    "Type '%s' does not implement interface '%.*s' required by generic parameter '%.*s'",
                    type_to_string(a->arena, concrete_type),
                    (int)c->interface_type->interface.name.length,
                    c->interface_type->interface.name.data,
                    (int)param->name.length, param->name.data
                )
            );
            return false;
        }
    }
    
    return true;
}

// ===== Вывод для отладки =====

void generic_param_dump(GenericParam* param) {
    if (!param) {
        printf("  <null>\n");
        return;
    }
    
    printf("  GenericParam(%.*s", (int)param->name.length, param->name.data);
    
    if (param->constraints) {
        printf(" where");
        for (GenericConstraint* c = param->constraints; c; c = c->next) {
            printf(" %.*s: %.*s",
                   (int)c->param_name.length, c->param_name.data,
                   (int)c->interface_type->interface.name.length,
                   c->interface_type->interface.name.data);
            if (c->next) printf(",");
        }
    }
    printf(")\n");
}

void generic_context_dump(GenericContext* ctx) {
    if (!ctx) {
        printf("  <null context>\n");
        return;
    }
    
    printf("  GenericContext (%d params):\n", ctx->param_count);
    int idx = 0;
    for (GenericParam* p = ctx->params; p && idx < ctx->param_count; p = p->next, idx++) {
        printf("    %.*s = %s\n",
               (int)p->name.length, p->name.data,
               ctx->concrete_types[idx] ? 
                   type_to_string(NULL, ctx->concrete_types[idx]) : 
                   "<unbound>");
    }
}
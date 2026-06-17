// src/semantic/generics.h
#ifndef BEVEL_GENERICS_H
#define BEVEL_GENERICS_H

#include "semantic/types.h"
#include "common/string.h"
#include "common/arena.h"

// Forward declaration для разрыва циклической зависимости
typedef struct Analyzer Analyzer;

// Ограничение на generic параметр (например, T: IComparable)
typedef struct GenericConstraint {
    String param_name;        // Имя параметра (например, "T")
    Type* interface_type;     // Тип интерфейса, который должен реализовать T
    struct GenericConstraint* next;
} GenericConstraint;

// Информация о generic параметре
typedef struct GenericParam {
    String name;              // Имя параметра (например, "T")
    Type* param_type;         // TYPE_GENERIC_PARAM
    GenericConstraint* constraints;  // Список ограничений
    struct GenericParam* next;
} GenericParam;

// Контекст инстанцирования дженерика
typedef struct {
    GenericParam* params;     // Параметры дженерика
    Type** concrete_types;    // Конкретные типы для каждого параметра
    int param_count;
} GenericContext;

// ===== Управление generic параметрами =====

// Создаёт новый generic параметр
GenericParam* generic_param_new(Arena* arena, String name);

// Добавляет ограничение к параметру (T: IInterface)
void generic_param_add_constraint(Arena* arena, GenericParam* param, Type* interface_type);

// Ищет параметр по имени
GenericParam* generic_param_find(GenericParam* params, String name);

// ===== Инстанцирование дженериков =====

// Создаёт контекст инстанцирования
GenericContext* generic_context_new(Arena* arena, GenericParam* params, int count);

// Устанавливает конкретный тип для параметра
void generic_context_bind(GenericContext* ctx, String param_name, Type* concrete_type);

// Заменяет generic параметры на конкретные типы в типе
Type* generic_instantiate_type(Arena* arena, GenericContext* ctx, Type* type);

// ===== Проверка ограничений =====

// Проверяет, удовлетворяет ли конкретный тип всем ограничениям параметра
// Возвращает true, если тип удовлетворяет всем ограничениям
bool generic_check_constraints(Analyzer* a, GenericParam* param, Type* concrete_type, int line, int col);

// ===== Вывод для отладки =====
void generic_param_dump(GenericParam* param);
void generic_context_dump(GenericContext* ctx);

#endif // BEVEL_GENERICS_H
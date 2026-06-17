// src/semantic/types.h
#ifndef BEVEL_TYPES_H
#define BEVEL_TYPES_H

#include "common/string.h"
#include "common/arena.h"
#include <stdbool.h>
#include <stdint.h>

// Forward declarations
struct GenericParamNode;

typedef enum {
    TYPE_VOID,
    TYPE_BOOL,
    TYPE_CHAR,
    TYPE_STRING,
    TYPE_INT,         // i8, i16, i32, i64
    TYPE_UINT,        // u8, u16, u32, u64
    TYPE_FLOAT,       // f32, f64, f80, f128
    TYPE_POINTER,
    TYPE_STRUCT,
    TYPE_ENUM,
    TYPE_INTERFACE,
    TYPE_GENERIC_PARAM,
    TYPE_FUNCTION,
    TYPE_ERROR,        // Для ошибочных типов, чтобы не падать каскадом
    TYPE_ARRAY,    // Статический массив [T; N]
    TYPE_SLICE,    // Срез []T
} TypeKind;

typedef struct Type Type;

typedef struct Field {
    String name;
    Type* type;
    int offset;
    struct Field* next;
} Field;

typedef struct Method {
    String name;
    Type* function_type;
    struct Method* next;
} Method;

struct Type {
    TypeKind kind;
    
    union {
        // TYPE_INT / TYPE_UINT
        struct {
            int bits;
            bool is_signed;
        } integer;
        
        // TYPE_FLOAT
        struct {
            int bits;
        } floating;
        
        // TYPE_POINTER
        struct {
            Type* pointee;
        } pointer;
                
        // TYPE_STRUCT
        struct {
            String name;
            Field* fields;
            Method* methods;
            int field_count;
            int method_count;
            bool is_defined;  // false для forward declaration
            
            // НОВОЕ: Для дженерик-структур
            bool is_generic;
            struct GenericParamNode* generic_params;
            int generic_param_count;
            Type** type_args;
            int type_arg_count;
        } structure;
        
        // TYPE_ENUM
        struct {
            String name;
            Type* base_type;
            // НОВОЕ: Список значений enum
            String* value_names;
            long long* value_values;
            int value_count;
        } enumeration;
        
        // TYPE_INTERFACE
        struct {
            String name;
            Method* methods;
            int method_count;
        } interface;
        
        // TYPE_GENERIC_PARAM (например T в generic(T))
        struct {
            String name;
            Type* constraint;  // Может быть NULL (без ограничения)
        } generic_param;
        
        // TYPE_FUNCTION
        struct {
            Type* return_type;
            Type** param_types;
            String* param_names;
            int param_count;
        } function;

        // TYPE_ARRAY
        struct {
            Type* element;
            int size;
        } array;

        // TYPE_SLICE
        struct {
            Type* element;
        } slice;
    };
};

// ===== Предопределённые типы (синглтоны) =====
Type* type_void(void);
Type* type_bool(void);
Type* type_char(void);
Type* type_string(void);
Type* type_int(int bits, bool is_signed);
Type* type_float(int bits);

// ===== Конструкторы типов =====
Type* type_pointer_new(Arena* arena, Type* pointee);
Type* type_struct_new(Arena* arena, String name);
Type* type_enum_new(Arena* arena, String name, Type* base,
                    String* value_names, long long* value_values, int value_count);
Type* type_interface_new(Arena* arena, String name);
Type* type_generic_param_new(Arena* arena, String name, Type* constraint);
Type* type_function_new(Arena* arena, Type* return_type, 
                         Type** params, String* param_names, int count);
Type* type_error_new(Arena* arena);
Type* type_array_new(Arena* arena, Type* element, int size);
Type* type_slice_new(Arena* arena, Type* element);

// ===== Работа с полями и методами =====
void type_add_field(Arena* arena, Type* struct_type, String name, Type* field_type);
Field* type_find_field(Type* struct_type, String name);
void type_add_method(Arena* arena, Type* type, String name, Type* func_type);
Method* type_find_method(Type* type, String name);

// ===== Сравнение и совместимость типов =====
bool types_equal(Type* a, Type* b);
bool types_assignable(Type* target, Type* source);
// Проверяет, можно ли неявно преобразовать source в target
bool types_implicitly_convertible(Type* target, Type* source);

bool types_compatible_for_op(Type* left, Type* right, int op_token);

// Проверка неявной реализации интерфейса структурой
bool type_implements_interface(Type* struct_type, Type* interface_type);

// Сравнение сигнатур двух функций (для проверки реализации)
bool types_function_equal(Type* func1, Type* func2);


// ===== Вывод типов (для ошибок) =====
const char* type_to_string(Arena* arena, Type* type);

// ===== Инициализация =====
void types_init(Arena* arena);

bool type_equals(Type* a, Type* b);

#endif // BEVEL_TYPES_H
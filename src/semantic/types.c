// src/semantic/types.c
#include "semantic/types.h"
#include "common/utils.h"
#include <stdio.h>
#include <string.h>

// Статические синглтоны для примитивных типов
static Type g_type_void;
static Type g_type_bool;
static Type g_type_char;
static Type g_type_string;
static Type g_type_int[4];   // i8, i16, i32, i64
static Type g_type_uint[4];  // u8, u16, u32, u64
static Type g_type_float[4]; // f32, f64, f80, f128
static bool g_types_initialized = false;

void types_init(Arena* arena) {
    (void)arena;
    if (g_types_initialized) return;
    
    g_type_void.kind = TYPE_VOID;
    g_type_bool.kind = TYPE_BOOL;
    g_type_char.kind = TYPE_CHAR;
    g_type_string.kind = TYPE_STRING;
    
    int bits[] = {8, 16, 32, 64};
    for (int i = 0; i < 4; i++) {
        g_type_int[i].kind = TYPE_INT;
        g_type_int[i].integer.bits = bits[i];
        g_type_int[i].integer.is_signed = true;
        
        g_type_uint[i].kind = TYPE_UINT;
        g_type_uint[i].integer.bits = bits[i];
        g_type_uint[i].integer.is_signed = false;
    }
    
    int float_bits[] = {32, 64, 80, 128};
    for (int i = 0; i < 4; i++) {
        g_type_float[i].kind = TYPE_FLOAT;
        g_type_float[i].floating.bits = float_bits[i];
    }
    
    g_types_initialized = true;
}

// ===== Синглтоны =====
Type* type_void(void)    { return &g_type_void; }
Type* type_bool(void)    { return &g_type_bool; }
Type* type_char(void)    { return &g_type_char; }
Type* type_string(void)  { return &g_type_string; }

Type* type_int(int bits, bool is_signed) {
    int idx = -1;
    switch (bits) {
        case 8: idx = 0; break;
        case 16: idx = 1; break;
        case 32: idx = 2; break;
        case 64: idx = 3; break;
    }
    if (idx < 0) return type_error_new(NULL);
    return is_signed ? &g_type_int[idx] : &g_type_uint[idx];
}

Type* type_float(int bits) {
    int idx = -1;
    switch (bits) {
        case 32: idx = 0; break;
        case 64: idx = 1; break;
        case 80: idx = 2; break;
        case 128: idx = 3; break;
    }
    if (idx < 0) return type_error_new(NULL);
    return &g_type_float[idx];
}

// ===== Конструкторы =====

Type* type_pointer_new(Arena* arena, Type* pointee) {
    Type* t = ARENA_ALLOC(arena, Type);
    t->kind = TYPE_POINTER;
    t->pointer.pointee = pointee;
    return t;
}


Type* type_struct_new(Arena* arena, String name) {
    Type* type = ARENA_ALLOC(arena, Type);
    type->kind = TYPE_STRUCT;
    type->structure.name = arena_string(arena, name);
    type->structure.fields = NULL;
    type->structure.field_count = 0;
    type->structure.methods = NULL;
    type->structure.method_count = 0;
    type->structure.is_defined = false;
    
    // НОВОЕ: Инициализация полей дженериков
    type->structure.is_generic = false;
    type->structure.generic_params = NULL;
    type->structure.generic_param_count = 0;
    type->structure.type_args = NULL;
    type->structure.type_arg_count = 0;
    
    return type;
}

Type* type_enum_new(Arena* arena, String name, Type* base, 
                    String* value_names, long long* value_values, int value_count) {
    Type* t = ARENA_ALLOC(arena, Type);
    t->kind = TYPE_ENUM;
    t->enumeration.name = arena_string(arena, name);
    t->enumeration.base_type = base ? base : type_int(32, true);
    
    // НОВОЕ: Копируем значения enum
    t->enumeration.value_count = value_count;
    if (value_count > 0 && value_names && value_values) {
        t->enumeration.value_names = ARENA_ARRAY(arena, String, value_count);
        t->enumeration.value_values = ARENA_ARRAY(arena, long long, value_count);
        for (int i = 0; i < value_count; i++) {
            t->enumeration.value_names[i] = arena_string(arena, value_names[i]);
            t->enumeration.value_values[i] = value_values[i];
        }
    } else {
        t->enumeration.value_names = NULL;
        t->enumeration.value_values = NULL;
    }
    
    return t;
}

Type* type_interface_new(Arena* arena, String name) {
    Type* t = ARENA_ALLOC(arena, Type);
    t->kind = TYPE_INTERFACE;
    t->interface.name = arena_string(arena, name);
    t->interface.methods = NULL;
    t->interface.method_count = 0;
    return t;
}

Type* type_generic_param_new(Arena* arena, String name, Type* constraint) {
    Type* t = ARENA_ALLOC(arena, Type);
    t->kind = TYPE_GENERIC_PARAM;
    t->generic_param.name = arena_string(arena, name);
    t->generic_param.constraint = constraint;
    return t;
}

Type* type_function_new(Arena* arena, Type* return_type,
                         Type** params, String* param_names, int count) {
    Type* t = ARENA_ALLOC(arena, Type);
    t->kind = TYPE_FUNCTION;
    t->function.return_type = return_type;
    t->function.param_count = count;
    if (count > 0) {
        t->function.param_types = ARENA_ARRAY(arena, Type*, count);
        t->function.param_names = ARENA_ARRAY(arena, String, count);
        for (int i = 0; i < count; i++) {
            t->function.param_types[i] = params ? params[i] : NULL;
            t->function.param_names[i] = param_names ? param_names[i] : STRING_EMPTY;
        }
    } else {
        t->function.param_types = NULL;
        t->function.param_names = NULL;
    }
    return t;
}

Type* type_error_new(Arena* arena) {
    if (!arena) {
        static Type err = {.kind = TYPE_ERROR};
        return &err;
    }
    Type* t = ARENA_ALLOC(arena, Type);
    t->kind = TYPE_ERROR;
    return t;
}

Type* type_array_new(Arena* arena, Type* element, int size) {
    Type* t = ARENA_ALLOC(arena, Type);
    t->kind = TYPE_ARRAY;
    t->array.element = element;
    t->array.size = size;
    return t;
}

Type* type_slice_new(Arena* arena, Type* element) {
    Type* t = ARENA_ALLOC(arena, Type);
    t->kind = TYPE_SLICE;
    t->slice.element = element;
    return t;
}

// ===== Работа с полями и методами =====

void type_add_field(Arena* arena, Type* struct_type, String name, Type* field_type) {
    if (!struct_type || struct_type->kind != TYPE_STRUCT) return;
    
    Field* f = ARENA_ALLOC(arena, Field);
    f->name = arena_string(arena, name);
    f->type = field_type;
    f->offset = struct_type->structure.field_count;
    f->next = NULL;
    
    // Добавляем в конец списка
    if (!struct_type->structure.fields) {
        struct_type->structure.fields = f;
    } else {
        Field* cur = struct_type->structure.fields;
        while (cur->next) cur = cur->next;
        cur->next = f;
    }
    struct_type->structure.field_count++;
}

Field* type_find_field(Type* struct_type, String name) {
    if (!struct_type || struct_type->kind != TYPE_STRUCT) return NULL;
    for (Field* f = struct_type->structure.fields; f; f = f->next) {
        if (string_equals(f->name, name)) return f;
    }
    return NULL;
}

void type_add_method(Arena* arena, Type* type, String name, Type* func_type) {
    if (!type) return;
    
    Method* m = ARENA_ALLOC(arena, Method);
    m->name = arena_string(arena, name);
    m->function_type = func_type;
    m->next = NULL;
    
    Method** head = NULL;
    int* count = NULL;
    
    switch (type->kind) {
        case TYPE_STRUCT:
            head = &type->structure.methods;
            count = &type->structure.method_count;
            break;
        case TYPE_INTERFACE:
            head = &type->interface.methods;
            count = &type->interface.method_count;
            break;
        default:
            return;
    }
    
    if (!*head) {
        *head = m;
    } else {
        Method* cur = *head;
        while (cur->next) cur = cur->next;
        cur->next = m;
    }
    (*count)++;
}

Method* type_find_method(Type* type, String name) {
    if (!type) return NULL;
    
    Method* head = NULL;
    switch (type->kind) {
        case TYPE_STRUCT:    head = type->structure.methods; break;
        case TYPE_INTERFACE: head = type->interface.methods; break;
        default: return NULL;
    }
    
    for (Method* m = head; m; m = m->next) {
        if (string_equals(m->name, name)) return m;
    }
    return NULL;
}

// ===== Сравнение типов =====

bool types_equal(Type* a, Type* b) {
    if (a == b) return true;
    if (!a || !b) return false;
    if (a->kind == TYPE_ERROR || b->kind == TYPE_ERROR) return true; // Error совместим с чем угодно
    if (a->kind != b->kind) return false;
    
    switch (a->kind) {
        case TYPE_VOID:
        case TYPE_BOOL:
        case TYPE_CHAR:
        case TYPE_STRING:
            return true;
        case TYPE_INT:
        case TYPE_UINT:
            return a->integer.bits == b->integer.bits && 
                   a->integer.is_signed == b->integer.is_signed;
        case TYPE_FLOAT:
            return a->floating.bits == b->floating.bits;
        case TYPE_POINTER:
            return types_equal(a->pointer.pointee, b->pointer.pointee);
        case TYPE_ARRAY:
            return a->array.size == b->array.size && 
                   types_equal(a->array.element, b->array.element);
        
        case TYPE_SLICE:
            return types_equal(a->slice.element, b->slice.element);

        case TYPE_STRUCT:
        case TYPE_ENUM:
        case TYPE_INTERFACE:
            // Именованные типы — сравниваем по имени
            return string_equals(a->structure.name, b->structure.name);
        case TYPE_GENERIC_PARAM:
            return string_equals(a->generic_param.name, b->generic_param.name);
        case TYPE_FUNCTION:
            if (a->function.param_count != b->function.param_count) return false;
            if (!types_equal(a->function.return_type, b->function.return_type)) return false;
            for (int i = 0; i < a->function.param_count; i++) {
                if (!types_equal(a->function.param_types[i], b->function.param_types[i]))
                    return false;
            }
            return true;
        case TYPE_ERROR:
            return true;
    }
    return false;
}

bool types_assignable(Type* target, Type* source) {
    if (types_equal(target, source)) return true;
    if (!target || !source) return false;
    if (target->kind == TYPE_ERROR || source->kind == TYPE_ERROR) return true;
    
    // Используем неявное приведение
    if (types_implicitly_convertible(target, source)) {
        return true;
    }
    
    return false;
}

bool types_implicitly_convertible(Type* target, Type* source) {
    if (types_equal(target, source)) return true;
    if (!target || !source) return false;
    if (target->kind == TYPE_ERROR || source->kind == TYPE_ERROR) return true;
    
    // Целочисленное расширение: меньший тип → больший тип
    if ((target->kind == TYPE_INT || target->kind == TYPE_UINT) &&
        (source->kind == TYPE_INT || source->kind == TYPE_UINT)) {
        // Можно расширять битность
        if (target->integer.bits >= source->integer.bits) {
            // Если signedness совпадает или target signed, а source unsigned
            if (target->integer.is_signed == source->integer.is_signed ||
                (target->integer.is_signed && !source->integer.is_signed)) {
                return true;
            }
        }
    }
    
    // Integer → Float (i32 → f64 безопасно)
    if (target->kind == TYPE_FLOAT && 
        (source->kind == TYPE_INT || source->kind == TYPE_UINT)) {
        // f32 может точно представить i8-i16, f64 может i8-i32
        if ((target->floating.bits == 32 && source->integer.bits <= 24) ||
            (target->floating.bits == 64 && source->integer.bits <= 53) ||
            (target->floating.bits >= 80)) {
            return true;
        }
    }
    
    // Float расширение: f32 → f64 → f80 → f128
    if (target->kind == TYPE_FLOAT && source->kind == TYPE_FLOAT) {
        if (target->floating.bits > source->floating.bits) {
            return true;
        }
    }
    
    // NULL (void*) → любой указатель
    if (target->kind == TYPE_POINTER && source->kind == TYPE_POINTER) {
        if (!source->pointer.pointee || source->pointer.pointee->kind == TYPE_VOID) {
            return true;
        }
    }

    // НОВОЕ: Неявная реализация интерфейса (struct → interface)
    if (target->kind == TYPE_INTERFACE && source->kind == TYPE_STRUCT) {
        return type_implements_interface(source, target);
    }
    
    
    return false;
}

// ===== Сравнение сигнатур функций =====
bool types_function_equal(Type* func1, Type* func2) {
    if (!func1 || !func2) return false;
    if (func1->kind != TYPE_FUNCTION || func2->kind != TYPE_FUNCTION) return false;
    
    // Сравниваем количество параметров
    if (func1->function.param_count != func2->function.param_count) return false;
    
    // Сравниваем тип возврата
    if (!types_equal(func1->function.return_type, func2->function.return_type)) return false;
    
    // Сравниваем типы параметров
    for (int i = 0; i < func1->function.param_count; i++) {
        if (!types_equal(func1->function.param_types[i], func2->function.param_types[i])) {
            return false;
        }
    }
    
    return true;
}

// ===== Проверка неявной реализации интерфейса =====
bool type_implements_interface(Type* struct_type, Type* interface_type) {
    if (!struct_type || !interface_type) return false;
    if (struct_type->kind != TYPE_STRUCT) return false;
    if (interface_type->kind != TYPE_INTERFACE) return false;
    
    // Для каждого метода интерфейса проверяем наличие у структуры
    for (Method* iface_method = interface_type->interface.methods; 
         iface_method != NULL; 
         iface_method = iface_method->next) {
        
        // Ищем метод с таким же именем у структуры
        Method* struct_method = type_find_method(struct_type, iface_method->name);
        
        if (!struct_method) {
            // Метод не найден — структура не реализует интерфейс
            return false;
        }
        
        // Сравниваем сигнатуры (с учётом того, что у struct метода первый параметр — *Struct,
        // а у interface метода — *Interface, поэтому сравниваем со смещением)
        // TODO: Упростить — пока считаем, что сигнатуры совпадают по именам
        if (struct_method->function_type->function.param_count != 
            iface_method->function_type->function.param_count) {
            return false;
        }
        
        // Сравниваем тип возврата
        if (!types_equal(struct_method->function_type->function.return_type,
                        iface_method->function_type->function.return_type)) {
            return false;
        }
        
        // Сравниваем параметры со 2-го (пропуская self)
        for (int i = 1; i < struct_method->function_type->function.param_count; i++) {
            if (!types_equal(struct_method->function_type->function.param_types[i],
                            iface_method->function_type->function.param_types[i])) {
                return false;
            }
        }
    }
    
    return true;  // Все методы интерфейса найдены и имеют правильные сигнатуры
}

// ===== Вывод типов =====

const char* type_to_string(Arena* arena, Type* type) {
    if (!type) return "void";
    
    switch (type->kind) {
        case TYPE_VOID:   return "void";
        case TYPE_BOOL:   return "bool";
        case TYPE_CHAR:   return "char";
        case TYPE_STRING: return "string";
        
        case TYPE_INT:
            // Статические строки для примитивных типов (не требуют арены)
            switch (type->integer.bits) {
                case 8:  return type->integer.is_signed ? "i8"  : "u8";
                case 16: return type->integer.is_signed ? "i16" : "u16";
                case 32: return type->integer.is_signed ? "i32" : "u32";
                case 64: return type->integer.is_signed ? "i64" : "u64";
                default: return "int";
            }
            
        case TYPE_UINT:
            switch (type->integer.bits) {
                case 8:  return "u8";
                case 16: return "u16";
                case 32: return "u32";
                case 64: return "u64";
                default: return "uint";
            }
            
        case TYPE_FLOAT:
            switch (type->floating.bits) {
                case 32:  return "f32";
                case 64:  return "f64";
                case 80:  return "f80";
                case 128: return "f128";
                default:  return "float";
            }
            
        case TYPE_POINTER:
            if (!type->pointer.pointee) return "void*";
            if (arena) {
                return format_string(arena, "%s*", type_to_string(arena, type->pointer.pointee));
            }
            // Если нет арены, пытаемся получить имя типа
            if (type->pointer.pointee->kind == TYPE_STRUCT ||
                type->pointer.pointee->kind == TYPE_INTERFACE) {
                if (type->pointer.pointee->structure.name.length > 0) {
                    static char ptr_buf[256];
                    int len = type->pointer.pointee->structure.name.length;
                    if (len > 250) len = 250;
                    memcpy(ptr_buf, type->pointer.pointee->structure.name.data, len);
                    ptr_buf[len] = '*';
                    ptr_buf[len + 1] = '\0';
                    return ptr_buf;
                }
            }
            return "ptr";
            
        case TYPE_ARRAY: {
            static char buf[256];
            const char* elem_str = type_to_string(arena, type->array.element);
            snprintf(buf, sizeof(buf), "[%s; %d]", elem_str, type->array.size);
            return buf;
        }
        
        case TYPE_SLICE: {
            static char buf[256];
            const char* elem_str = type_to_string(arena, type->slice.element);
            snprintf(buf, sizeof(buf), "[]%s", elem_str);
            return buf;
        }
            
        case TYPE_STRUCT:
        case TYPE_ENUM:
        case TYPE_INTERFACE:
            if (type->structure.name.length > 0 && type->structure.name.data) {
                if (arena) {
                    return format_string(arena, "%.*s",
                        (int)type->structure.name.length, type->structure.name.data);
                }
                // Если нет арены, возвращаем указатель на данные строки напрямую
                // (это работает, потому что строка в арене)
                static char name_buf[256];
                int len = type->structure.name.length < 255 ? type->structure.name.length : 255;
                memcpy(name_buf, type->structure.name.data, len);
                name_buf[len] = '\0';
                return name_buf;
            }
            return "<anonymous>";
            
        case TYPE_GENERIC_PARAM:
            if (type->generic_param.name.length > 0 && type->generic_param.name.data) {
                if (arena) {
                    return format_string(arena, "%.*s",
                        (int)type->generic_param.name.length, type->generic_param.name.data);
                }
                static char param_buf[256];
                int len = type->generic_param.name.length < 255 ? type->generic_param.name.length : 255;
                memcpy(param_buf, type->generic_param.name.data, len);
                param_buf[len] = '\0';
                return param_buf;
            }
            return "<generic>";
            
        case TYPE_FUNCTION:
            return "<function>";
            
        case TYPE_ERROR:
            return "<error>";
    }
    return "<unknown>";
}

bool type_equals(Type* a, Type* b) {
    if (!a || !b) return false;
    if (a->kind != b->kind) return false;
    
    switch (a->kind) {
        case TYPE_INT:
        case TYPE_UINT:
            return a->integer.bits == b->integer.bits;
        case TYPE_FLOAT:
            return a->floating.bits == b->floating.bits;
        case TYPE_POINTER:
            return type_equals(a->pointer.pointee, b->pointer.pointee);
        case TYPE_ARRAY:
            return a->array.size == b->array.size && 
                   type_equals(a->array.element, b->array.element);
        case TYPE_SLICE:
            return type_equals(a->slice.element, b->slice.element);
        default:
            return true; // void, bool, char, string
    }
}
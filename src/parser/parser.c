// src/parser/parser.c
#include "parser/parser.h"
#include "parser/printer.h"
#include "common/utils.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

// ===== Forward declarations (предварительные объявления) =====
// Все функции парсера объявлены здесь, чтобы они могли вызывать друг друга
// в любом порядке, независимо от порядка определения в файле.

static ASTNode* parse_type(Parser* p);
static ASTNode* parse_expression(Parser* p);
static ASTNode* parse_equality(Parser* p);
static ASTNode* parse_comparison(Parser* p);
static ASTNode* parse_term(Parser* p);
static ASTNode* parse_factor(Parser* p);
static ASTNode* parse_unary(Parser* p);
static ASTNode* parse_primary(Parser* p);
static ASTNode* parse_call(Parser* p);
static ASTNode* parse_block(Parser* p);
static ASTNode* parse_statement(Parser* p);
static ASTNode* parse_var_declaration(Parser* p);
static ASTNode* parse_return_statement(Parser* p);
static ASTNode* parse_if_statement(Parser* p);
static ASTNode* parse_function_declaration(Parser* p);
static ASTNode* parse_method_declaration(Parser* p, String type_name);
static ASTNode* parse_import_declaration(Parser* p);
static ASTNode* parse_struct_declaration(Parser* p);
static ASTNode* parse_enum_declaration(Parser* p);
static ASTNode* parse_interface_declaration(Parser* p);
static ASTNode* parse_methods_block(Parser* p);
static ASTNode* parse_declaration(Parser* p);
static ASTNode* parse_while_statement(Parser* p);
static ASTNode* parse_for_statement(Parser* p);
static ASTNode* parse_or(Parser* p);
static ASTNode* parse_and(Parser* p);

static bool is_type_token(TokenType t) {
    switch (t) {
        case TOKEN_I8: case TOKEN_I16: case TOKEN_I32: case TOKEN_I64:
        case TOKEN_U8: case TOKEN_U16: case TOKEN_U32: case TOKEN_U64:
        case TOKEN_F32: case TOKEN_F64: case TOKEN_F80: case TOKEN_F128:
        case TOKEN_SINGLE: case TOKEN_DOUBLE: case TOKEN_EXTENDED: case TOKEN_QUAD:
        case TOKEN_BOOL_TYPE: case TOKEN_STRING_TYPE: case TOKEN_CHAR_TYPE:
        case TOKEN_INT_TYPE: case TOKEN_VOID:
        case TOKEN_IDENTIFIER: // Пользовательские типы
        case TOKEN_LBRACKET:   // типы массивов T[N] и []T
        case TOKEN_STAR:       // <-- НОВОЕ: указатели *T
            return true;
        default:
            return false;
    }
}

// ===== Утилиты парсера =====

static void advance(Parser* p) {
    p->previous = p->current;
    p->current = lexer_next_token(&p->lexer);
}

static bool check(Parser* p, TokenType type) {
    return p->current.type == type;
}

static bool match(Parser* p, TokenType type) {
    if (check(p, type)) {
        advance(p);
        return true;
    }
    return false;
}


static bool expect(Parser* p, TokenType type, const char* message) {
    if (check(p, type)) {
        advance(p);
        return true;
    }
    error_reporter_parser_error(p->errors, p->current.line, p->current.column, message);
    p->had_error = true;
    return false;
}

// Заглядывание вперёд для различения функций и переменных
// Заглядывание вперёд для различения функций и переменных
static bool is_function_start(Parser* p) {

    fprintf(stderr, "[DEBUG PARSER] is_function_start called, current token: type=%d '%.*s'\n",
            p->current.type, (int)p->current.lexeme.length, p->current.lexeme.data);

    // Сохраняем ВСЁ состояние лексера и парсера
    const char* saved_start = p->lexer.start;
    const char* saved_lexer_current = p->lexer.current;
    int saved_line = p->lexer.line;
    int saved_column = p->lexer.column;
    int saved_pending = p->lexer.pending_dedents;
    int saved_indent_depth = p->lexer.indent_depth;
    bool saved_at_line_start = p->lexer.at_line_start;
    int saved_indent_stack[32];
    for (int i = 0; i < 32; i++) {
        saved_indent_stack[i] = p->lexer.indent_stack[i];
    }
    
    Token saved_curr = p->current;
    Token saved_prev = p->previous;
    int saved_error_count = p->errors->count;
    bool saved_had_error = p->had_error;
    
    ASTNode* type = NULL;
    if (is_type_token(p->current.type)) {
        type = parse_type(p); 
    }
    
    bool is_func = false;
    if (type) {
        bool is_id = match(p, TOKEN_IDENTIFIER);
        bool is_lparen = check(p, TOKEN_LPAREN);
        is_func = is_id && is_lparen;
    }
    
    // Восстанавливаем ВСЁ состояние
    p->lexer.start = saved_start;
    p->lexer.current = saved_lexer_current;
    p->lexer.line = saved_line;
    p->lexer.column = saved_column;
    p->lexer.pending_dedents = saved_pending;
    p->lexer.indent_depth = saved_indent_depth;
    p->lexer.at_line_start = saved_at_line_start;
    for (int i = 0; i < 32; i++) {
        p->lexer.indent_stack[i] = saved_indent_stack[i];
    }
    
    p->current = saved_curr;
    p->previous = saved_prev;
    p->errors->count = saved_error_count;
    p->had_error = saved_had_error;
    
    fprintf(stderr, "[DEBUG PARSER] is_function_start returning: %s\n", is_func ? "true" : "false");
    return is_func;
}

static void synchronize(Parser* p) {
    p->had_error = false;
    bool advanced = false; // Флаг: продвинулись ли мы хотя бы раз?
    
    while (p->current.type != TOKEN_EOF) {
        // Проверяем точки остановки ТОЛЬКО если мы уже продвинулись
        if (advanced) {
            if (p->previous.type == TOKEN_SEMICOLON) return;
            if (p->previous.type == TOKEN_COLON) return;
            if (p->previous.type == TOKEN_DEDENT) return;
            
            switch (p->current.type) {
                case TOKEN_STRUCT: case TOKEN_INTERFACE: case TOKEN_METHODS:
                case TOKEN_IMPORT: case TOKEN_IF: case TOKEN_FOR:
                case TOKEN_WHILE: case TOKEN_RETURN:
                    return;
                default: break;
            }
        }
        advance(p);
        advanced = true;
    }
}

// ===== Парсинг типов =====
static ASTNode* parse_type(Parser* p) {
    int line = p->current.line;
    int col = p->current.column;
    
    // 1. Обработка срезов []T
    if (check(p, TOKEN_LBRACKET)) {
        advance(p); // съели '['
        
        if (match(p, TOKEN_RBRACKET)) {
            // Это срез: []T
            ASTNode* element_type = parse_type(p);
            return ast_array_type(p->arena, element_type, NULL, true, line, col);
        } else {
            // Ошибка: [T без ] или [T; N] больше не поддерживается
            error_reporter_parser_error(p->errors, p->current.line, p->current.column, 
                                        "Expected ']' for slice type []T.");
            return (ASTNode*)ast_type(p->arena, TOKEN_VOID, string_from_cstr("void"), false, NULL, 0, line, col);
        }
    }
    
    TokenType token_type = TOKEN_VOID;
    String name = string_from_cstr("void");
    
    // 2. Парсим базовый тип
    if (match(p, TOKEN_I8))        { token_type = TOKEN_I8;  name = string_from_cstr("i8"); }
    else if (match(p, TOKEN_I16))  { token_type = TOKEN_I16; name = string_from_cstr("i16"); }
    else if (match(p, TOKEN_I32))  { token_type = TOKEN_I32; name = string_from_cstr("i32"); }
    else if (match(p, TOKEN_I64))  { token_type = TOKEN_I64; name = string_from_cstr("i64"); }
    else if (match(p, TOKEN_U8))   { token_type = TOKEN_U8;  name = string_from_cstr("u8"); }
    else if (match(p, TOKEN_U16))  { token_type = TOKEN_U16; name = string_from_cstr("u16"); }
    else if (match(p, TOKEN_U32))  { token_type = TOKEN_U32; name = string_from_cstr("u32"); }
    else if (match(p, TOKEN_U64))  { token_type = TOKEN_U64; name = string_from_cstr("u64"); }
    else if (match(p, TOKEN_F32) || match(p, TOKEN_SINGLE))   { token_type = TOKEN_F32; name = string_from_cstr("f32"); }
    else if (match(p, TOKEN_F64) || match(p, TOKEN_DOUBLE))   { token_type = TOKEN_F64; name = string_from_cstr("f64"); }
    else if (match(p, TOKEN_F80) || match(p, TOKEN_EXTENDED)) { token_type = TOKEN_F80; name = string_from_cstr("f80"); }
    else if (match(p, TOKEN_F128) || match(p, TOKEN_QUAD))    { token_type = TOKEN_F128; name = string_from_cstr("f128"); }
    else if (match(p, TOKEN_BOOL_TYPE))   { token_type = TOKEN_BOOL_TYPE;   name = string_from_cstr("bool"); }
    else if (match(p, TOKEN_STRING_TYPE)) { token_type = TOKEN_STRING_TYPE; name = string_from_cstr("string"); }
    else if (match(p, TOKEN_CHAR_TYPE))   { token_type = TOKEN_CHAR_TYPE;   name = string_from_cstr("char"); }
    else if (match(p, TOKEN_VOID))        { token_type = TOKEN_VOID;        name = string_from_cstr("void"); }
    else if (match(p, TOKEN_INT_TYPE))    { token_type = TOKEN_INT_TYPE;    name = string_from_cstr("int"); }
    
    // 3. НОВОЕ: Обработка пользовательских типов и generic параметров (T, E, ...)
    else if (match(p, TOKEN_IDENTIFIER)) {
        token_type = TOKEN_IDENTIFIER;
        name = p->previous.lexeme;
        
        // НОВОЕ: Проверяем, является ли это инстанциацией дженерик-типа
        bool is_generic_inst = false;
        ASTNode** type_args = NULL;
        int type_arg_count = 0;
        
        if (match(p, TOKEN_LPAREN)) {
            is_generic_inst = true;
            type_args = ARENA_ARRAY(p->arena, ASTNode*, 16);
            
            if (!check(p, TOKEN_RPAREN)) {
                do {
                    if (type_arg_count < 16) {
                        type_args[type_arg_count++] = parse_type(p);
                    }
                } while (match(p, TOKEN_COMMA));
            }
            expect(p, TOKEN_RPAREN, "Expected ')' after generic type arguments.");
        }
        
        // Обработка указателей *T
        bool is_pointer = false;
        if (match(p, TOKEN_STAR)) {
            is_pointer = true;
        }
        
        return (ASTNode*)ast_type(p->arena, token_type, name, is_pointer, 
                                  is_generic_inst ? type_args : NULL, 
                                  is_generic_inst ? type_arg_count : 0, 
                                  line, col);
    }
    else {
        error_reporter_parser_error(p->errors, p->current.line, p->current.column, 
                                    "Expected type name.");
        return (ASTNode*)ast_type(p->arena, TOKEN_VOID, string_from_cstr("void"), false, NULL, 0, line, col);
    }
    
    // 4. Обработка указателей *T
    bool is_pointer = false;
    if (match(p, TOKEN_STAR)) {
        is_pointer = true;
    }
    
    return (ASTNode*)ast_type(p->arena, token_type, name, is_pointer, NULL, 0, line, col);
}
// static ASTNode* parse_type(Parser* p) {
//     int line = p->current.line;
//     int col = p->current.column;
    
//     // Обработка срезов []T
//     if (check(p, TOKEN_LBRACKET)) {
//         advance(p); // съели '['
        
//         if (match(p, TOKEN_RBRACKET)) {
//             // Это срез: []T
//             ASTNode* element_type = parse_type(p);
//             return ast_array_type(p->arena, element_type, NULL, true, line, col);
//             // НОВОЕ: Обработка пользовательских типов и generic параметров (T, E, ...)
//         else if (match(p, TOKEN_IDENTIFIER)) {
//             token_type = TOKEN_IDENTIFIER;
//             name = p->previous.lexeme;
//         }
        
//         // Обработка указателей *T
//         bool is_pointer = false;
//         if (match(p, TOKEN_STAR)) {
//             is_pointer = true;
//         }
        
//         return (ASTNode*)ast_type(p->arena, token_type, name, is_pointer, line, col);
//         } else {
//             // Ошибка: [T без ] или [T; N] больше не поддерживается
//             error_reporter_parser_error(p->errors, p->current.line, p->current.column, 
//                                         "Expected ']' for slice type []T.");
//             return (ASTNode*)ast_type(p->arena, TOKEN_VOID, string_from_cstr("void"), false, line, col);
//         }
//     }
    
//     TokenType token_type = TOKEN_VOID;
//     String name = string_from_cstr("void");
    
//     // Парсим базовый тип
//     if (match(p, TOKEN_I8))        { token_type = TOKEN_I8;  name = string_from_cstr("i8"); }
//     else if (match(p, TOKEN_I16))  { token_type = TOKEN_I16; name = string_from_cstr("i16"); }
//     else if (match(p, TOKEN_I32))  { token_type = TOKEN_I32; name = string_from_cstr("i32"); }
//     else if (match(p, TOKEN_I64))  { token_type = TOKEN_I64; name = string_from_cstr("i64"); }
//     else if (match(p, TOKEN_U8))   { token_type = TOKEN_U8;  name = string_from_cstr("u8"); }
//     else if (match(p, TOKEN_U16))  { token_type = TOKEN_U16; name = string_from_cstr("u16"); }
//     else if (match(p, TOKEN_U32))  { token_type = TOKEN_U32; name = string_from_cstr("u32"); }
//     else if (match(p, TOKEN_U64))  { token_type = TOKEN_U64; name = string_from_cstr("u64"); }
//     else if (match(p, TOKEN_F32) || match(p, TOKEN_SINGLE))   { token_type = TOKEN_F32; name = string_from_cstr("f32"); }
//     else if (match(p, TOKEN_F64) || match(p, TOKEN_DOUBLE))   { token_type = TOKEN_F64; name = string_from_cstr("f64"); }
//     else if (match(p, TOKEN_F80) || match(p, TOKEN_EXTENDED)) { token_type = TOKEN_F80; name = string_from_cstr("f80"); }
//     else if (match(p, TOKEN_F128) || match(p, TOKEN_QUAD))    { token_type = TOKEN_F128; name = string_from_cstr("f128"); }
//     else if (match(p, TOKEN_BOOL_TYPE))   { token_type = TOKEN_BOOL_TYPE;   name = string_from_cstr("bool"); }
//     else if (match(p, TOKEN_STRING_TYPE)) { token_type = TOKEN_STRING_TYPE; name = string_from_cstr("string"); }
//     else if (match(p, TOKEN_CHAR_TYPE))   { token_type = TOKEN_CHAR_TYPE;   name = string_from_cstr("char"); }
//     else if (match(p, TOKEN_INT_TYPE))    { token_type = TOKEN_INT_TYPE;    name = string_from_cstr("int"); }
//     else if (match(p, TOKEN_VOID))        { token_type = TOKEN_VOID;        name = string_from_cstr("void"); }
//     else if (check(p, TOKEN_IDENTIFIER)) {
//         name = p->current.lexeme;
//         token_type = TOKEN_IDENTIFIER;
//         advance(p);
//     } else {
//         error_reporter_parser_error(p->errors, p->current.line, p->current.column, "Expected type name.");
//         return (ASTNode*)ast_type(p->arena, TOKEN_VOID, string_from_cstr("void"), false, line, col);
//     }
    
//     // Создаём базовый TypeNode
//     ASTNode* base_type_node = (ASTNode*)ast_type(p->arena, token_type, name, false, line, col);
    
//     // Постфиксные звёздочки (как в C): i32*, Point*
//     while (match(p, TOKEN_STAR)) {
//         base_type_node = ast_pointer_type(p->arena, base_type_node, line, col);
//     }
    
//     // НОВОЕ: Обработка C-style массивов T[N]
//     if (match(p, TOKEN_LBRACKET)) {
//         ASTNode* size = parse_expression(p);
//         expect(p, TOKEN_RBRACKET, "Expected ']' after array size.");
//         return ast_array_type(p->arena, base_type_node, size, false, line, col);
//     }
    
//     return base_type_node;
// }

// ===== Парсинг выражений =====

static ASTNode* parse_expression(Parser* p);
static ASTNode* parse_equality(Parser* p);
static ASTNode* parse_comparison(Parser* p);
static ASTNode* parse_term(Parser* p);
static ASTNode* parse_factor(Parser* p);
static ASTNode* parse_unary(Parser* p);
static ASTNode* parse_primary(Parser* p);
static ASTNode* parse_call(Parser* p);

static ASTNode* parse_assignment(Parser* p);

static ASTNode* parse_expression(Parser* p) {
    return parse_assignment(p);
}

// Парсинг присваивания (самый низкий приоритет)
static ASTNode* parse_assignment(Parser* p) {
    // Парсим левую часть (это может быть идентификатор, field access и т.д.)
    ASTNode* target = parse_or(p); 
    
    // Если после левой части идет '=', это присваивание
    if (match(p, TOKEN_EQ)) {
        ASTNode* value = parse_assignment(p);
        
        // ИСПРАВЛЕНО: expr -> target
        if (target->type == AST_IDENTIFIER || 
            target->type == AST_DEREF || 
            target->type == AST_FIELD_ACCESS ||
            target->type == AST_INDEX_ACCESS) {  // <-- ДОБАВЛЕНО
            return (ASTNode*)ast_assign(p->arena, target, value, p->previous.line, p->previous.column);
        }
        
        error_reporter_parser_error(p->errors, p->previous.line, p->previous.column, 
                                    "Invalid assignment target.");
    }
    
    return target;
}

static ASTNode* parse_or(Parser* p) {
    ASTNode* expr = parse_and(p);
    while (match(p, TOKEN_OR)) {
        TokenType op = p->previous.type;
        expr = (ASTNode*)ast_binary_expr(p->arena, op, expr, parse_and(p), p->previous.line, p->previous.column);
    }
    return expr;
}

static ASTNode* parse_and(Parser* p) {
    ASTNode* expr = parse_equality(p);
    while (match(p, TOKEN_AND)) {
        TokenType op = p->previous.type;
        expr = (ASTNode*)ast_binary_expr(p->arena, op, expr, parse_equality(p), p->previous.line, p->previous.column);
    }
    return expr;
}

static ASTNode* parse_equality(Parser* p) {
    ASTNode* expr = parse_comparison(p);
    while (match(p, TOKEN_EQEQ) || match(p, TOKEN_NOTEQ)) {
        TokenType op = p->previous.type;
        expr = (ASTNode*)ast_binary_expr(p->arena, op, expr, parse_comparison(p), p->previous.line, p->previous.column);
    }
    return expr;
}

static ASTNode* parse_comparison(Parser* p) {
    ASTNode* expr = parse_term(p);
    while (match(p, TOKEN_LT) || match(p, TOKEN_LTE) || match(p, TOKEN_GT) || match(p, TOKEN_GTE)) {
        TokenType op = p->previous.type;
        expr = (ASTNode*)ast_binary_expr(p->arena, op, expr, parse_term(p), p->previous.line, p->previous.column);
    }
    return expr;
}

static ASTNode* parse_term(Parser* p) {
    ASTNode* expr = parse_factor(p);
    while (match(p, TOKEN_PLUS) || match(p, TOKEN_MINUS)) {
        TokenType op = p->previous.type;
        expr = (ASTNode*)ast_binary_expr(p->arena, op, expr, parse_factor(p), p->previous.line, p->previous.column);
    }
    return expr;
}

static ASTNode* parse_factor(Parser* p) {
    ASTNode* expr = parse_unary(p);
    while (match(p, TOKEN_STAR) || match(p, TOKEN_SLASH) || match(p, TOKEN_PERCENT)) {
        TokenType op = p->previous.type;
        expr = (ASTNode*)ast_binary_expr(p->arena, op, expr, parse_unary(p), p->previous.line, p->previous.column);
    }
    return expr;
}

static ASTNode* parse_unary(Parser* p) {
    // 1. Взятие адреса: &variable
    // 1. Взятие адреса: &variable
    if (match(p, TOKEN_AMPERSAND)) {
        TokenType op = p->previous.type;
        int line = p->previous.line;
        int col = p->previous.column;
        ASTNode* operand = parse_unary(p);
        return (ASTNode*)ast_addr_of(p->arena, operand, line, col);
    }
    
    // 2. Разыменование: *pointer

    // 2.5. Логическое НЕ: !expression
    if (match(p, TOKEN_NOT)) {
        TokenType op = p->previous.type;  // Сохраняем op ДО рекурсии!
        int line = p->previous.line;
        int col = p->previous.column;
        ASTNode* operand = parse_unary(p);
        return (ASTNode*)ast_unary_expr(p->arena, op, operand, line, col);
    }

    if (match(p, TOKEN_STAR)) {
        TokenType op = p->previous.type;
        int line = p->previous.line;
        int col = p->previous.column;
        ASTNode* operand = parse_unary(p);
        return (ASTNode*)ast_deref(p->arena, operand, line, col);
    }
    
    // 3. Иначе переходим к обработке постфиксных операций (вызовы, поля, индексы)
    return parse_call(p);  // <-- СТАЛО
}

static ASTNode* parse_call(Parser* p) {
    ASTNode* expr = parse_primary(p);
    
    while (true) {
        if (match(p, TOKEN_LPAREN)) {
            // Вызов функции
            int arg_count = 0;
            ASTNode** args = ARENA_ARRAY(p->arena, ASTNode*, 16);
            
            if (!check(p, TOKEN_RPAREN)) {
                do {
                    if (arg_count < 16) {
                        // НОВОЕ: Проверяем, не является ли аргумент типом (для generic вызовов)
                        // Если следующий токен — примитивный тип (i32, f64, ...), парсим как тип
                        if (is_type_token(p->current.type) && 
                            p->current.type != TOKEN_IDENTIFIER &&
                            p->current.type != TOKEN_LBRACKET) {
                            // Это примитивный тип — парсим как TypeAsExpr
                            ASTNode* type_node = parse_type(p);
                            args[arg_count++] = (ASTNode*)ast_type_as_expr(p->arena, type_node, 
                                                                           p->previous.line, p->previous.column);
                        } else {
                            // Обычное выражение
                            args[arg_count++] = parse_expression(p);
                        }
                    }
                } while (match(p, TOKEN_COMMA));
            }
            expect(p, TOKEN_RPAREN, "Expected ')' after arguments.");
            expr = (ASTNode*)ast_call_expr(p->arena, expr, args, arg_count, p->previous.line, p->previous.column);
        }
        else if (match(p, TOKEN_DOT)) {
            // Доступ к полю
            if (!check(p, TOKEN_IDENTIFIER)) {
                error_reporter_parser_error(p->errors, p->current.line, p->current.column,
                                            "Expected field name after '.'.");
                break;
            }
            String field = p->current.lexeme;
            advance(p);
            expr = (ASTNode*)ast_field_access(p->arena, expr, field, p->previous.line, p->previous.column);
        }
        // НОВОЕ: Доступ по индексу arr[i]
        else if (match(p, TOKEN_LBRACKET)) {
            ASTNode* index = parse_expression(p);
            expect(p, TOKEN_RBRACKET, "Expected ']' after array index.");
            expr = ast_index_access(p->arena, expr, index, p->previous.line, p->previous.column);
        }
        else {
            break;
        }
    }
    
    return expr;
}

static ASTNode* parse_primary(Parser* p) {
    
    // НОВОЕ: Литерал массива [1, 2, 3, 4, 5]
    if (match(p, TOKEN_LBRACKET)) {
        int line = p->previous.line, col = p->previous.column;
        int element_count = 0;
        ASTNode** elements = ARENA_ARRAY(p->arena, ASTNode*, 16);
        
        if (!check(p, TOKEN_RBRACKET)) {
            do {
                if (element_count < 16) {
                    elements[element_count++] = parse_expression(p);
                }
            } while (match(p, TOKEN_COMMA));
        }
        expect(p, TOKEN_RBRACKET, "Expected ']' after array literal.");
        return ast_array_literal(p->arena, elements, element_count, line, col);
    }

    if (match(p, TOKEN_TRUE)) {
        return (ASTNode*)ast_bool_literal(p->arena, true, p->previous.line, p->previous.column);
    }
    if (match(p, TOKEN_FALSE)) {
        return (ASTNode*)ast_bool_literal(p->arena, false, p->previous.line, p->previous.column);
    }
    
    if (match(p, TOKEN_INT_LIT)) {
        bool ok; long long val = parse_int(p->previous.lexeme, 10, &ok);
        return (ASTNode*)ast_int_literal(p->arena, val, p->previous.line, p->previous.column);
    }
    if (match(p, TOKEN_FLOAT_LIT)) {
        bool ok; double val = parse_float(p->previous.lexeme, &ok);
        return (ASTNode*)ast_float_literal(p->arena, val, p->previous.line, p->previous.column);
    }
    if (match(p, TOKEN_STRING_LIT)) {
        String s = p->previous.lexeme;
        if (s.length >= 2) s = string_substr(s, 1, s.length - 2);
        return (ASTNode*)ast_string_literal(p->arena, s, p->previous.line, p->previous.column);
    }
    if (match(p, TOKEN_BOOL_TYPE)) {
        bool val = string_equals(p->previous.lexeme, STRING_FROM_LITERAL("true"));
        return (ASTNode*)ast_bool_literal(p->arena, val, p->previous.line, p->previous.column);
    }
    if (match(p, TOKEN_SELF)) {
        return (ASTNode*)ast_self_expr(p->arena, p->previous.line, p->previous.column);
    }
    if (match(p, TOKEN_IDENTIFIER)) {
        // Можно убрать проверку на "self" здесь, так как TOKEN_SELF обработан выше
        return (ASTNode*)ast_identifier(p->arena, p->previous.lexeme, p->previous.line, p->previous.column);
    }
    if (match(p, TOKEN_LPAREN)) {
        ASTNode* expr = parse_expression(p);
        expect(p, TOKEN_RPAREN, "Expected ')' after expression.");
        return expr;
    }
    error_reporter_parser_error(p->errors, p->current.line, p->current.column, "Expected expression.");
    p->had_error = true;
    return NULL;
}

// ===== Парсинг блоков и операторов =====
static ASTNode* parse_block(Parser* p) {
    int line = p->current.line, col = p->current.column;
    ASTNode** statements = ARENA_ARRAY(p->arena, ASTNode*, 64);
    int count = 0;
    
    if (match(p, TOKEN_INDENT)) {
        while (!check(p, TOKEN_DEDENT) && !check(p, TOKEN_EOF)) {
            ASTNode* stmt = parse_statement(p);
            
            if (stmt) {
                if (count < 64) statements[count++] = stmt;
            } else if (p->had_error) {
                // Мягкое восстановление: пропускаем до конца строки
                while (!check(p, TOKEN_SEMICOLON) && !check(p, TOKEN_DEDENT) && !check(p, TOKEN_EOF)) {
                    advance(p);
                }
                match(p, TOKEN_SEMICOLON);
                p->had_error = false;
            } else {
                advance(p); // Защита от бесконечного цикла
            }
        }
        match(p, TOKEN_DEDENT);
    } else {
        ASTNode* stmt = parse_statement(p);
        if (stmt && count < 64) statements[count++] = stmt;
    }
    return (ASTNode*)ast_block(p->arena, statements, count, line, col);
}

static ASTNode* parse_var_declaration(Parser* p) {
    int line = p->current.line, col = p->current.column;
    
    // НОВОЕ: Проверяем, является ли тип указателем (одна или несколько звездочек)
    int pointer_depth = 0;
    while (match(p, TOKEN_STAR)) {
        pointer_depth++;
    }
    
    // Парсим базовый тип (i32, f64, Point и т.д.)
    ASTNode* type_node = parse_type(p);
    
    // Если были звездочки, оборачиваем тип в PointerTypeNode
    for (int i = 0; i < pointer_depth; i++) {
        type_node = ast_pointer_type(p->arena, type_node, line, col);
    }
    
    // Ожидаем имя переменной
    if (!check(p, TOKEN_IDENTIFIER)) {
        error_reporter_parser_error(p->errors, p->current.line, p->current.column, 
                                    "Expected variable name after type.");
        return NULL;
    }
    
    String name = p->current.lexeme;
    advance(p);
    
    // Опциональное присваивание
    ASTNode* initializer = NULL;
    if (match(p, TOKEN_EQ)) {
        initializer = parse_expression(p);
    }
    
    expect(p, TOKEN_SEMICOLON, "Expected ';' after variable declaration.");
    
        return (ASTNode*)ast_var_decl(p->arena, type_node, name, initializer, line, col);
}

static ASTNode* parse_return_statement(Parser* p) {
    int line = p->previous.line, col = p->previous.column;
    ASTNode* value = NULL;
    if (!check(p, TOKEN_SEMICOLON) && !check(p, TOKEN_EOF) && !check(p, TOKEN_RBRACE)) {
        value = parse_expression(p);
    }
    expect(p, TOKEN_SEMICOLON, "Expected ';' after return statement.");
    return (ASTNode*)ast_return(p->arena, value, line, col);
}


static ASTNode* parse_if_statement(Parser* p) {
    int line = p->previous.line, col = p->previous.column;
    
    // Опциональные скобки вокруг условия: if (x < y) или if x < y
    bool has_paren = match(p, TOKEN_LPAREN);
    
    ASTNode* condition = parse_expression(p);
    if (!condition) {
        error_reporter_parser_error(p->errors, p->current.line, p->current.column, "Expected condition expression after 'if'.");
        return NULL;
    }
    
    if (has_paren) {
        expect(p, TOKEN_RPAREN, "Expected ')' after if condition.");
    }
    
    // Ожидаем двоеточие (как в Python)
    expect(p, TOKEN_COLON, "Expected ':' after if condition.");
    
    // Парсим блок then
    ASTNode* then_branch = NULL;
    if (check(p, TOKEN_INDENT)) {
        then_branch = parse_block(p);
    } else {
        error_reporter_parser_error(p->errors, p->current.line, p->current.column, "Expected indented block after 'if'.");
        ASTNode** empty = ARENA_ARRAY(p->arena, ASTNode*, 1);
        then_branch = (ASTNode*)ast_block(p->arena, empty, 0, line, col);
    }
    
    // Парсим опциональный else или elif
    ASTNode* else_branch = NULL;
    if (match(p, TOKEN_ELIF)) {
        // НОВОЕ: Прямая поддержка elif
        else_branch = parse_if_statement(p);
    } else if (match(p, TOKEN_ELSE)) {
        expect(p, TOKEN_COLON, "Expected ':' after 'else'.");
        if (check(p, TOKEN_INDENT)) {
            else_branch = parse_block(p);
        } else if (check(p, TOKEN_IF)) {
            // Поддержка else if (два отдельных токена)
            else_branch = parse_if_statement(p);
        } else {
            error_reporter_parser_error(p->errors, p->current.line, p->current.column, "Expected indented block or 'if' after 'else'.");
            ASTNode** empty = ARENA_ARRAY(p->arena, ASTNode*, 1);
            else_branch = (ASTNode*)ast_block(p->arena, empty, 0, line, col);
        }
    }
    
    return (ASTNode*)ast_if(p->arena, condition, then_branch, else_branch, line, col);
}

static ASTNode* parse_statement(Parser* p) {
    if (match(p, TOKEN_RETURN)) return parse_return_statement(p);
    if (match(p, TOKEN_IF)) return parse_if_statement(p);
    if (match(p, TOKEN_WHILE)) return parse_while_statement(p);
    if (match(p, TOKEN_FOR)) return parse_for_statement(p);
    
    // Break и Continue — это statement'ы, заканчиваются ;
    if (match(p, TOKEN_BREAK)) {
        ASTNode* node = ast_break(p->arena, p->previous.line, p->previous.column);
        expect(p, TOKEN_SEMICOLON, "Expected ';' after 'break'.");
        return node;
    }
    if (match(p, TOKEN_CONTINUE)) {
        ASTNode* node = ast_continue(p->arena, p->previous.line, p->previous.column);
        expect(p, TOKEN_SEMICOLON, "Expected ';' after 'continue'.");
        return node;
    }
    
    // Объявление переменной: встроенный тип (i32, f64, bool и т.д.)
    if (is_type_token(p->current.type)) {
        if (p->current.type == TOKEN_IDENTIFIER) {
            Token next = lexer_peek_token(&p->lexer);
            
            if (next.type == TOKEN_IDENTIFIER || next.type == TOKEN_STAR) {
                // Point x; или *Point ptr; — это точно объявление
                return parse_var_declaration(p);
            } else if (next.type == TOKEN_LPAREN) {
                // Result(f64, string) res; — это generic инстанциация типа
                // НО может быть и вызов функции: Result(5, 10);
                // Поэтому нужен speculative parsing с откатом
                
                Token saved_prev = p->previous;
                Token saved_current = p->current;
                const char* saved_start = p->lexer.start;
                const char* saved_lexer_current = p->lexer.current;
                int saved_line = p->lexer.line;
                int saved_column = p->lexer.column;
                int saved_pending = p->lexer.pending_dedents;
                int saved_indent_depth = p->lexer.indent_depth;
                bool saved_at_line_start = p->lexer.at_line_start;
                int saved_errors = p->errors->count;
                bool saved_had_error = p->had_error;
                
                int saved_indent_stack[32];
                for (int i = 0; i < 32; i++) {
                    saved_indent_stack[i] = p->lexer.indent_stack[i];
                }
                
                // Пытаемся распарсить как объявление переменной
                ASTNode* var_decl = parse_var_declaration(p);
                
                if (var_decl && !p->had_error) {
                    return var_decl;
                }
                
                // Откат: восстанавливаем состояние
                p->previous = saved_prev;
                p->current = saved_current;
                p->lexer.start = saved_start;
                p->lexer.current = saved_lexer_current;
                p->lexer.line = saved_line;
                p->lexer.column = saved_column;
                p->lexer.pending_dedents = saved_pending;
                p->lexer.indent_depth = saved_indent_depth;
                p->lexer.at_line_start = saved_at_line_start;
                p->errors->count = saved_errors;
                p->had_error = saved_had_error;
                
                for (int i = 0; i < 32; i++) {
                    p->lexer.indent_stack[i] = saved_indent_stack[i];
                }
                
                // Продолжаем парсить как выражение (вызов функции)
            } else if (next.type == TOKEN_LBRACKET) {
                // Может быть T[N] или arr[i] — нужно заглянуть дальше
                Token saved_prev = p->previous;
                Token saved_current = p->current;
                const char* saved_start = p->lexer.start;
                const char* saved_lexer_current = p->lexer.current;
                int saved_line = p->lexer.line;
                int saved_column = p->lexer.column;
                int saved_pending = p->lexer.pending_dedents;
                int saved_indent_depth = p->lexer.indent_depth;
                bool saved_at_line_start = p->lexer.at_line_start;
                int saved_errors = p->errors->count;
                bool saved_had_error = p->had_error;
                
                int saved_indent_stack[32];
                for (int i = 0; i < 32; i++) {
                    saved_indent_stack[i] = p->lexer.indent_stack[i];
                }
                
                ASTNode* var_decl = parse_var_declaration(p);
                
                if (var_decl && !p->had_error) {
                    return var_decl;
                }
                
                // Откат: восстанавливаем состояние
                p->previous = saved_prev;
                p->current = saved_current;
                p->lexer.start = saved_start;
                p->lexer.current = saved_lexer_current;
                p->lexer.line = saved_line;
                p->lexer.column = saved_column;
                p->lexer.pending_dedents = saved_pending;
                p->lexer.indent_depth = saved_indent_depth;
                p->lexer.at_line_start = saved_at_line_start;
                p->errors->count = saved_errors;
                p->had_error = saved_had_error;
                
                for (int i = 0; i < 32; i++) {
                    p->lexer.indent_stack[i] = saved_indent_stack[i];
                }
            }
        } else {
            return parse_var_declaration(p);
        }
    }
    
    // Выражение (включая присваивание)
    ASTNode* expr = parse_expression(p);
    expect(p, TOKEN_SEMICOLON, "Expected ';' after expression statement.");
    return expr;
}

// Парсинг списка параметров: (type1 name1, type2 name2, ...)
// is_method: если true, поддерживает self как первый параметр
// type_name: имя типа структуры (для self)
static ParameterNode* parse_parameter_list(Parser* p, bool is_method, String type_name, int* out_count) {
    ParameterNode* params = ARENA_ARRAY(p->arena, ParameterNode, 16);
    int param_count = 0;
    
    // Проверяем на пустой список параметров
    if (check(p, TOKEN_RPAREN)) {
        *out_count = 0;
        return params;
    }
    
    // НОВОЕ: Обрабатываем self как первый параметр метода
    fprintf(stderr, "[DEBUG PARSER] parse_parameter_list: is_method=%d, current token: type=%d '%.*s'\n",
            is_method, p->current.type, (int)p->current.lexeme.length, p->current.lexeme.data);
    
    if (is_method && match(p, TOKEN_SELF)) {
        fprintf(stderr, "[DEBUG PARSER] parse_parameter_list: matched self!\n");
        if (param_count < 16) {
            params[param_count].name = string_from_cstr("self");
            // Тип self = тип структуры (type_name)
            params[param_count].type = (ASTNode*)ast_type(p->arena, TOKEN_IDENTIFIER, type_name, false, NULL, 0, p->previous.line, p->previous.column);
            param_count++;
        }
        
        // Если после self есть запятая — продолжаем парсить остальные параметры
        if (match(p, TOKEN_COMMA)) {
            // продолжим ниже
        } else {
            // self — единственный параметр
            *out_count = param_count;
            return params;
        }
    }
    
    // Парсим остальные параметры: type name, type name, ...
    while (!check(p, TOKEN_RPAREN) && !check(p, TOKEN_EOF)) {
        if (param_count >= 16) break;
        
        // Парсим тип параметра
        ASTNode* param_type = parse_type(p);
        
        // Парсим имя параметра
        if (!expect(p, TOKEN_IDENTIFIER, "Expected parameter name.")) break;
        String param_name = p->previous.lexeme;
        
        params[param_count].name = param_name;
        params[param_count].type = param_type;
        param_count++;
        
        // Проверяем запятую
        if (!match(p, TOKEN_COMMA)) break;
    }
    
    *out_count = param_count;
    return params;
}

// ===== Парсинг объявлений верхнего уровня =====

static ASTNode* parse_function_declaration(Parser* p) {
    int line = p->current.line, col = p->current.column;
    ASTNode* ret_type = parse_type(p);
    if (!expect(p, TOKEN_IDENTIFIER, "Expected function name.")) return NULL;
    String name = p->previous.lexeme;
    
    expect(p, TOKEN_LPAREN, "Expected '('.");
    
    // НОВОЕ: Парсим список параметров
    int param_count = 0;
    ParameterNode* params = parse_parameter_list(p, false, STRING_EMPTY, &param_count);
    
    expect(p, TOKEN_RPAREN, "Expected ')'.");
    expect(p, TOKEN_COLON, "Expected ':' before function body.");
    
    // Явно ожидаем INDENT для тела функции
    if (check(p, TOKEN_INDENT)) {
        ASTNode* body = parse_block(p);
        return (ASTNode*)ast_function_decl(p->arena, name, ret_type, params, param_count, body, line, col);
    }
    
    error_reporter_parser_error(p->errors, p->current.line, p->current.column, 
                                "Expected indented block after function declaration.");
    ASTNode** empty_stmts = ARENA_ARRAY(p->arena, ASTNode*, 1);
    ASTNode* body = (ASTNode*)ast_block(p->arena, empty_stmts, 0, line, col);
    return (ASTNode*)ast_function_decl(p->arena, name, ret_type, params, param_count, body, line, col);
}

// Парсинг метода: i32 sum(self): ... или Vector3 add(self, Vector3 other): ...
static ASTNode* parse_method_declaration(Parser* p, String type_name) {
    fprintf(stderr, "[DEBUG PARSER] parse_method_declaration START, current token: type=%d '%.*s'\n",
            p->current.type, (int)p->current.lexeme.length, p->current.lexeme.data);
    
    int line = p->current.line, col = p->current.column;
    ASTNode* ret_type = parse_type(p);
    
    fprintf(stderr, "[DEBUG PARSER] parse_method_declaration after parse_type, current token: type=%d '%.*s'\n",
            p->current.type, (int)p->current.lexeme.length, p->current.lexeme.data);
    
    if (!expect(p, TOKEN_IDENTIFIER, "Expected method name.")) return NULL;
    String name = p->previous.lexeme;
    
    fprintf(stderr, "[DEBUG PARSER] parse_method_declaration name='%.*s', current token: type=%d '%.*s'\n",
            (int)name.length, name.data,
            p->current.type, (int)p->current.lexeme.length, p->current.lexeme.data);
    
    expect(p, TOKEN_LPAREN, "Expected '('.");
    
    fprintf(stderr, "[DEBUG PARSER] parse_method_declaration after '(', current token: type=%d '%.*s'\n",
            p->current.type, (int)p->current.lexeme.length, p->current.lexeme.data);
    
    // НОВОЕ: Парсим параметры с поддержкой self
    int param_count = 0;
    ParameterNode* params = parse_parameter_list(p, true, type_name, &param_count);
    
    fprintf(stderr, "[DEBUG PARSER] parse_method_declaration after parse_parameter_list, param_count=%d, current token: type=%d '%.*s'\n",
            param_count,
            p->current.type, (int)p->current.lexeme.length, p->current.lexeme.data);
    
    expect(p, TOKEN_RPAREN, "Expected ')'.");
    expect(p, TOKEN_COLON, "Expected ':' before method body.");
    
    // Ожидаем INDENT для тела метода
    if (check(p, TOKEN_INDENT)) {
        ASTNode* body = parse_block(p);
        return (ASTNode*)ast_function_decl(p->arena, name, ret_type, params, param_count, body, line, col);
    }
    
    error_reporter_parser_error(p->errors, p->current.line, p->current.column, 
                                "Expected indented block after method declaration.");
    ASTNode** empty_stmts = ARENA_ARRAY(p->arena, ASTNode*, 1);
    ASTNode* body = (ASTNode*)ast_block(p->arena, empty_stmts, 0, line, col);
    return (ASTNode*)ast_function_decl(p->arena, name, ret_type, params, param_count, body, line, col);
}

static ASTNode* parse_import_declaration(Parser* p) {
    int line = p->previous.line, col = p->previous.column;
    if (!expect(p, TOKEN_STRING_LIT, "Expected path string after 'import'.")) return NULL;
    String path = p->previous.lexeme;
    if (path.length >= 2) path = string_substr(path, 1, path.length - 2);
    
    String alias = STRING_EMPTY;
    if (match(p, TOKEN_AS)) {
        if (!expect(p, TOKEN_IDENTIFIER, "Expected alias name after 'as'.")) return NULL;
        alias = p->previous.lexeme;
    }
    expect(p, TOKEN_SEMICOLON, "Expected ';' after import statement.");
    return (ASTNode*)ast_import(p->arena, path, alias, line, col);
}

static ASTNode* parse_struct_declaration(Parser* p) {
    int line = p->previous.line, col = p->previous.column;
    if (!expect(p, TOKEN_IDENTIFIER, "Expected struct name.")) return NULL;
    String name = p->previous.lexeme;
    expect(p, TOKEN_COLON, "Expected ':' after struct name.");
    
    // ИСПРАВЛЕНО: потребляем INDENT перед чтением полей
    bool has_indent = match(p, TOKEN_INDENT);
    
    FieldNode* fields = ARENA_ARRAY(p->arena, FieldNode, 32);
    int field_count = 0;
    
    while (!check(p, TOKEN_EOF) && !check(p, TOKEN_DEDENT) && 
           !check(p, TOKEN_STRUCT) && !check(p, TOKEN_INTERFACE) && 
           !check(p, TOKEN_METHODS) && !check(p, TOKEN_IMPORT)) {
        ASTNode* type = parse_type(p);
        if (!type) break;
        if (!expect(p, TOKEN_IDENTIFIER, "Expected field name.")) break;
        String field_name = p->previous.lexeme;
        expect(p, TOKEN_SEMICOLON, "Expected ';' after field.");
        
        if (field_count < 32) {
            fields[field_count].name = arena_string(p->arena, field_name);
            fields[field_count].type = type;
            field_count++;
        }
    }
    
    // ИСПРАВЛЕНО: потребляем DEDENT в конце блока
    if (has_indent) {
        expect(p, TOKEN_DEDENT, "Expected dedent after struct fields.");
    }

    
    return (ASTNode*)ast_struct_decl(p->arena, name, fields, field_count, false, NULL, 0, NULL, 0, line, col);
}

static ASTNode* parse_enum_declaration(Parser* p) {
    int line = p->previous.line, col = p->previous.column;
    
    // Парсим базовый тип (i32, u32 и т.д.)
    ASTNode* base_type = parse_type(p);
    
    // Парсим имя enum
    if (!expect(p, TOKEN_IDENTIFIER, "Expected enum name.")) return NULL;
    String name = p->previous.lexeme;
    
    expect(p, TOKEN_COLON, "Expected ':' after enum name.");
    
    // Ожидаем INDENT
    if (!match(p, TOKEN_INDENT)) {
        error_reporter_parser_error(p->errors, p->current.line, p->current.column,
                                    "Expected indented block after enum declaration.");
        return NULL;
    }
    
    // Парсим значения enum
    EnumValueNode* values = ARENA_ARRAY(p->arena, EnumValueNode, 16);
    int value_count = 0;
    
    while (!check(p, TOKEN_EOF) && !check(p, TOKEN_DEDENT)) {
        if (!expect(p, TOKEN_IDENTIFIER, "Expected enum value name.")) break;
        String val_name = p->previous.lexeme;
        
        long long val_value = 0;
        
        // НОВОЕ: используем TOKEN_EQ или TOKEN_ASSIGN (проверьте в token.h)
        if (match(p, TOKEN_EQ)) {  // <-- Проверьте правильное имя!
            if (!check(p, TOKEN_INT_LIT)) {  // <-- ИСПРАВЛЕНО: TOKEN_INT_LIT
                error_reporter_parser_error(p->errors, p->current.line, p->current.column,
                                           "Expected integer literal for enum value.");
                break;
            }
            IntLiteralNode* lit = (IntLiteralNode*)parse_primary(p);
            val_value = lit->value;
        } else {
            // Если значение не указано, используем предыдущее + 1
            if (value_count > 0) {
                val_value = values[value_count - 1].value + 1;
            } else {
                val_value = 0;
            }
        }
        
        expect(p, TOKEN_SEMICOLON, "Expected ';' after enum value.");
        
        if (value_count < 16) {
            values[value_count].name = val_name;
            values[value_count].value = val_value;
            value_count++;
        }
    }
    
    match(p, TOKEN_DEDENT);
    
    return (ASTNode*)ast_enum_decl(p->arena, name, base_type, values, value_count, line, col);
}

static ASTNode* parse_interface_declaration(Parser* p) {
    int line = p->previous.line, col = p->previous.column;
    if (!expect(p, TOKEN_IDENTIFIER, "Expected interface name.")) return NULL;
    String name = p->previous.lexeme;
    expect(p, TOKEN_COLON, "Expected ':' after interface name.");
    
    // ИСПРАВЛЕНО: потребляем INDENT
    bool has_indent = match(p, TOKEN_INDENT);
    
    InterfaceMethodNode* methods = ARENA_ARRAY(p->arena, InterfaceMethodNode, 32);
    int method_count = 0;
    
    while (!check(p, TOKEN_EOF) && !check(p, TOKEN_DEDENT) && 
           !check(p, TOKEN_STRUCT) && !check(p, TOKEN_INTERFACE) && 
           !check(p, TOKEN_METHODS) && !check(p, TOKEN_IMPORT)) {
        ASTNode* ret_type = parse_type(p);
        if (!ret_type) break;
        if (!expect(p, TOKEN_IDENTIFIER, "Expected method name.")) break;
        String m_name = p->previous.lexeme;
        expect(p, TOKEN_LPAREN, "Expected '('.");
        
        // Парсим список параметров: (self), (self, i32 x), и т.д.
        int param_count = 0;
        ParameterNode* params = parse_parameter_list(p, true, name, &param_count); 
        
        expect(p, TOKEN_RPAREN, "Expected ')'.");
        expect(p, TOKEN_SEMICOLON, "Expected ';' after interface method.");
        
        if (method_count < 32) {
            methods[method_count].name = arena_string(p->arena, m_name);
            methods[method_count].return_type = ret_type;
            methods[method_count].params = params;
            methods[method_count].param_count = param_count;
            method_count++;
        }
    }
    
    // ИСПРАВЛЕНО: потребляем DEDENT
    if (has_indent) {
        expect(p, TOKEN_DEDENT, "Expected dedent after interface methods.");
    }
    
    return (ASTNode*)ast_interface_decl(p->arena, name, methods, method_count, line, col);
}

static ASTNode* parse_methods_block(Parser* p) {
    int line = p->previous.line, col = p->previous.column;
    if (!expect(p, TOKEN_IDENTIFIER, "Expected type name after 'methods'.")) return NULL;
    String type_name = p->previous.lexeme;
    expect(p, TOKEN_COLON, "Expected ':' after type name.");
    
    bool has_indent = match(p, TOKEN_INDENT);
    int methods_indent_level = lexer_indent_level(&p->lexer); // Запоминаем уровень отступа
    
    FunctionDeclNode* methods = ARENA_ARRAY(p->arena, FunctionDeclNode, 32);
    int method_count = 0;
    
    while (!check(p, TOKEN_EOF) && !check(p, TOKEN_DEDENT) && 
           !check(p, TOKEN_STRUCT) && !check(p, TOKEN_INTERFACE) && 
           !check(p, TOKEN_METHODS) && !check(p, TOKEN_IMPORT)) {
        
        // КЛЮЧЕВАЯ ПРОВЕРКА: если уровень отступа уменьшился, значит, мы вышли из блока
        if (has_indent && lexer_indent_level(&p->lexer) < methods_indent_level) {
            break;
        }
        
        // if (p->previous.type == TOKEN_DEDENT) {
        //     break;
        // }
        
        if (is_function_start(p)) {
            ASTNode* func = parse_method_declaration(p, type_name);
            if (func && method_count < 32) {
                methods[method_count] = *((FunctionDeclNode*)func);
                method_count++;
            }
        } else {
            break;
        }
    }
    
    if (has_indent) {
        match(p, TOKEN_DEDENT);
    }
    
    return (ASTNode*)ast_methods_block(p->arena, type_name, methods, method_count, line, col);
}

// ===== Парсинг generic параметров: (T), (T, E), (T, E, F) =====
// ===== Парсинг generic параметров: (T), (T, E), (T, E, F) =====
// Возвращает параметры и where clauses через out-параметры
static GenericParamNode* parse_generic_params(Parser* p, int* count, 
                                               WhereClauseNode** out_where_clauses, 
                                               int* out_where_count) {
    *count = 0;
    GenericParamNode* params = ARENA_ARRAY(p->arena, GenericParamNode, 16);
    
    expect(p, TOKEN_LPAREN, "Expected '(' after 'generic'.");
    
    if (!check(p, TOKEN_RPAREN)) {
        do {
            if (!expect(p, TOKEN_IDENTIFIER, "Expected generic parameter name.")) {
                break;
            }
            
            if (*count < 16) {
                params[*count].name = p->previous.lexeme;
                params[*count].next = NULL;
                (*count)++;
            }
        } while (match(p, TOKEN_COMMA));
    }
    
    expect(p, TOKEN_RPAREN, "Expected ')' after generic parameters.");
    
    // Связываем параметры в список
    for (int i = 0; i < *count - 1; i++) {
        params[i].next = &params[i + 1];
    }
    
    // НОВОЕ: Парсинг where clauses: where T: Interface, E: OtherInterface
    *out_where_clauses = NULL;
    *out_where_count = 0;
    
    if (match(p, TOKEN_WHERE)) {
        WhereClauseNode* where_clauses = ARENA_ARRAY(p->arena, WhereClauseNode, 16);
        
        do {
            if (!expect(p, TOKEN_IDENTIFIER, "Expected type parameter name after 'where'.")) {
                break;
            }
            String type_param = p->previous.lexeme;
            
            if (!expect(p, TOKEN_COLON, "Expected ':' after type parameter.")) {
                break;
            }
            
            if (!expect(p, TOKEN_IDENTIFIER, "Expected interface name after ':'.")) {
                break;
            }
            String interface_name = p->previous.lexeme;
            
            if (*out_where_count < 16) {
                where_clauses[*out_where_count].type_param = type_param;
                where_clauses[*out_where_count].interface_name = interface_name;
                where_clauses[*out_where_count].next = NULL;
                (*out_where_count)++;
            }
        } while (match(p, TOKEN_COMMA));
        
        // Связываем where clauses в список
        for (int i = 0; i < *out_where_count - 1; i++) {
            where_clauses[i].next = &where_clauses[i + 1];
        }
        
        *out_where_clauses = where_clauses;
    }
    
    return params;
}

static ASTNode* parse_declaration(Parser* p) {
    // ИСПРАВЛЕНО: пропускаем DEDENT токены на верхнем уровне
    if (match(p, TOKEN_DEDENT)) return NULL;
    
    // Объявляем переменные ОДИН РАЗ здесь
    bool is_generic = false;
    GenericParamNode* generic_params = NULL;
    int generic_param_count = 0;
    
    // Объявляем переменные для where clauses
    WhereClauseNode* where_clauses = NULL;
    int where_clause_count = 0;
    
    // Проверяем generic перед struct или function
    if (match(p, TOKEN_GENERIC)) {
        is_generic = true;
        generic_params = parse_generic_params(p, &generic_param_count, 
                                                &where_clauses, &where_clause_count);
    }
    
    if (match(p, TOKEN_IMPORT)) return parse_import_declaration(p);
    
    if (match(p, TOKEN_STRUCT)) {
        ASTNode* node = parse_struct_declaration(p);
        if (node && is_generic) {
            StructDeclNode* sd = (StructDeclNode*)node;
            sd->is_generic = true;
            sd->generic_params = generic_params;
            sd->generic_param_count = generic_param_count;
            sd->where_clauses = where_clauses;        // ← ДОБАВИТЬ
            sd->where_clause_count = where_clause_count;  // ← ДОБАВИТЬ
        }
        return node;
    }
    
    if (match(p, TOKEN_ENUM)) return parse_enum_declaration(p); 
    if (match(p, TOKEN_INTERFACE)) return parse_interface_declaration(p);
    if (match(p, TOKEN_METHODS)) return parse_methods_block(p);
    
    if (is_function_start(p)) {
        ASTNode* func = parse_function_declaration(p);
        if (func && is_generic) {
            FunctionDeclNode* fd = (FunctionDeclNode*)func;
            fd->is_generic = true;
            fd->generic_params = generic_params;
            fd->generic_param_count = generic_param_count;
            fd->where_clauses = where_clauses;        // ← ДОБАВИТЬ
            fd->where_clause_count = where_clause_count;  // ← ДОБАВИТЬ
        }
        return func;
    }
    
    return parse_statement(p);
}

// ===== Главный интерфейс =====

void parser_init(Parser* p, const char* source, Arena* arena, ErrorReporter* errors) {
    lexer_init(&p->lexer, source, arena, errors);
    p->arena = arena;
    p->errors = errors;
    p->had_error = false;
    p->current.type = TOKEN_EOF;
    p->previous.type = TOKEN_EOF;
    
    advance(p);
}

ASTNode* parser_parse(Parser* p) {
    ASTNode** statements = ARENA_ARRAY(p->arena, ASTNode*, 64);
    int count = 0;

    while (!check(p, TOKEN_EOF)) {
        ASTNode* decl = parse_declaration(p);
        if (decl) {
            if (count < 64) statements[count++] = decl;
        }
        if (p->had_error) synchronize(p);
    }
    return (ASTNode*)ast_block(p->arena, statements, count, 1, 1);
}

static ASTNode* parse_while_statement(Parser* p) {
    int line = p->previous.line, col = p->previous.column;

    // 1. Ожидаем '('
    if (!expect(p, TOKEN_LPAREN, "Expected '(' after 'while'.")) {
        // При ошибке создаем фиктивный узел, чтобы не возвращать NULL и не ронять парсер
        ASTNode** empty = ARENA_ARRAY(p->arena, ASTNode*, 1);
        return (ASTNode*)ast_block(p->arena, empty, 0, line, col);
    }

    // 2. Парсим условие
    ASTNode* condition = parse_expression(p);
    if (!condition) {
        error_reporter_parser_error(p->errors, p->current.line, p->current.column, 
                                    "Expected condition expression after 'while ('.");
        // Создаем фиктивное условие 'false', чтобы AST был валидным
        condition = (ASTNode*)ast_bool_literal(p->arena, false, line, col);
    }

    expect(p, TOKEN_RPAREN, "Expected ')' after while condition.");
    expect(p, TOKEN_COLON, "Expected ':' after while condition.");

    // 3. Парсим тело
    ASTNode* body = NULL;
    if (check(p, TOKEN_INDENT)) {
        body = parse_block(p);
    } else {
        error_reporter_parser_error(p->errors, p->current.line, p->current.column, 
                                    "Expected indented block after 'while'.");
        ASTNode** empty = ARENA_ARRAY(p->arena, ASTNode*, 1);
        body = (ASTNode*)ast_block(p->arena, empty, 0, line, col);
    }

    return (ASTNode*)ast_while(p->arena, condition, body, line, col);
}

static ASTNode* parse_for_statement(Parser* p) {
    int line = p->previous.line, col = p->previous.column;
    
    // for (init; condition; increment):
    expect(p, TOKEN_LPAREN, "Expected '(' after 'for'.");
    
    // 1. Инициализация
    ASTNode* init = NULL;
    if (!check(p, TOKEN_SEMICOLON)) {
        if (is_type_token(p->current.type) && p->current.type != TOKEN_IDENTIFIER) {
            init = parse_var_declaration(p);
        } else {
            init = parse_expression(p);
            expect(p, TOKEN_SEMICOLON, "Expected ';' after for init.");
        }
    } else {
        expect(p, TOKEN_SEMICOLON, "Expected ';' after for init.");
    }
    
    // 2. Условие
    ASTNode* condition = NULL;
    if (!check(p, TOKEN_SEMICOLON)) {
        condition = parse_expression(p);
    }
    expect(p, TOKEN_SEMICOLON, "Expected ';' after for condition.");
    
    // 3. Инкремент
    ASTNode* increment = NULL;
    if (!check(p, TOKEN_RPAREN)) {
        increment = parse_expression(p);
    }
    expect(p, TOKEN_RPAREN, "Expected ')' after for increment.");
    expect(p, TOKEN_COLON, "Expected ':' after for.");
    
    // 4. Тело
    ASTNode* body = NULL;
    if (check(p, TOKEN_INDENT)) {
        body = parse_block(p);
    } else {
        error_reporter_parser_error(p->errors, p->current.line, p->current.column, 
                                    "Expected indented block after 'for'.");
        ASTNode** empty = ARENA_ARRAY(p->arena, ASTNode*, 1);
        body = (ASTNode*)ast_block(p->arena, empty, 0, line, col);
    }
    
    return (ASTNode*)ast_for(p->arena, init, condition, increment, body, line, col);
}

// ===== Отладочная печать AST =====

// static void print_indent(int depth) {
//     for (int i = 0; i < depth; i++) printf("  ");
// }

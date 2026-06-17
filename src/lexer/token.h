// src/lexer/token.h
#ifndef BEVEL_TOKEN_H
#define BEVEL_TOKEN_H

#include "common/string.h"

typedef enum {
    // Специальные
    TOKEN_EOF,
    TOKEN_ERROR,
    TOKEN_NEWLINE,
    TOKEN_INDENT,
    TOKEN_DEDENT,
    
    // Литералы
    TOKEN_INT_LIT,
    TOKEN_FLOAT_LIT,
    TOKEN_STRING_LIT,
    TOKEN_CHAR_LIT,
    
    // Идентификаторы
    TOKEN_IDENTIFIER,
    
    // Типы
    TOKEN_VOID,
    TOKEN_BOOL_TYPE,
    TOKEN_CHAR_TYPE,
    TOKEN_STRING_TYPE,
    TOKEN_I8, TOKEN_I16, TOKEN_I32, TOKEN_I64,
    TOKEN_U8, TOKEN_U16, TOKEN_U32, TOKEN_U64,
    TOKEN_F32, TOKEN_F64, TOKEN_F80, TOKEN_F128,
    TOKEN_SINGLE, TOKEN_DOUBLE, TOKEN_EXTENDED, TOKEN_QUAD,
    TOKEN_INT_TYPE,
    
    // Структурные ключевые слова
    TOKEN_IMPORT, TOKEN_AS,
    TOKEN_STRUCT, TOKEN_ENUM, TOKEN_INTERFACE, TOKEN_METHODS,
    TOKEN_GENERIC,
    TOKEN_WHERE,          // <-- ДЛЯ ДЖЕНЕРИКОВ
    
    // Модификаторы
    TOKEN_CONST, TOKEN_MUT,
    TOKEN_SELF,
    TOKEN_DEFER,
    TOKEN_MACRO,
    
    // Управление потоком
    TOKEN_IF,
    TOKEN_ELIF,
    TOKEN_ELSE,
    TOKEN_WHILE,
    TOKEN_FOR,
    TOKEN_BREAK,
    TOKEN_CONTINUE,
    TOKEN_SWITCH,
    TOKEN_CASE,
    TOKEN_DEFAULT,
    TOKEN_RETURN,
    
    // Значения
    TOKEN_TRUE,
    TOKEN_FALSE,
    TOKEN_NULL,
    
    // Операторы
    TOKEN_PLUS, TOKEN_MINUS, TOKEN_STAR, TOKEN_SLASH, TOKEN_PERCENT,
    TOKEN_EQ, TOKEN_EQEQ, TOKEN_NOTEQ,
    TOKEN_LT, TOKEN_LTE, TOKEN_GT, TOKEN_GTE,
    TOKEN_AND, TOKEN_OR, TOKEN_NOT,
    TOKEN_AMPERSAND, TOKEN_PIPE, TOKEN_CARET, TOKEN_TILDE,
    TOKEN_LSHIFT, TOKEN_RSHIFT,
    TOKEN_PLUSPLUS, TOKEN_MINUSMINUS,
    TOKEN_PLUSEQ, TOKEN_MINUSEQ, TOKEN_STAREQ, TOKEN_SLASHEQ,
    TOKEN_ARROW,
    TOKEN_AT,
    
    // Разделители
    TOKEN_LPAREN, TOKEN_RPAREN,
    TOKEN_LBRACKET, TOKEN_RBRACKET,
    TOKEN_LBRACE, TOKEN_RBRACE,
    TOKEN_COLON,
    TOKEN_SEMICOLON,
    TOKEN_COMMA,
    TOKEN_DOT,
    
    TOKEN_COUNT
} TokenType;

typedef struct {
    TokenType type;
    String lexeme;
    int line;
    int column;
} Token;

static inline Token token_new(TokenType type, String lexeme, int line, int col) {
    return (Token){type, lexeme, line, col};
}

static inline Token token_error(int line, int col) {
    return (Token){TOKEN_ERROR, STRING_EMPTY, line, col};
}

static inline Token token_eof(int line, int col) {
    return (Token){TOKEN_EOF, STRING_EMPTY, line, col};
}

// ОБЯЗАТЕЛЬНО ДОБАВЬТЕ ЭТУ СТРОКУ:
const char* token_type_name(TokenType type);

#endif // BEVEL_TOKEN
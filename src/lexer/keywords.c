// src/lexer/keywords.c
#include "lexer/keywords.h"
#include "lexer/lexer.h"
#include <stdlib.h>
#include <string.h>

typedef struct {
    const char* name;
    TokenType type;
} Keyword;

// ВАЖНО: массив должен быть отсортирован в алфавитном порядке по name!
// Это требуется для корректной работы бинарного поиска.
static const Keyword keywords[] = {
    {"and",       TOKEN_AND},
    {"as",        TOKEN_AS},
    {"bool",      TOKEN_BOOL_TYPE},
    {"break",     TOKEN_BREAK},
    {"case",      TOKEN_CASE},
    {"char",      TOKEN_CHAR_TYPE},
    {"const",     TOKEN_CONST},
    {"continue",  TOKEN_CONTINUE},
    {"default",   TOKEN_DEFAULT},
    {"defer",     TOKEN_DEFER},
    {"double",    TOKEN_DOUBLE},
    {"elif",      TOKEN_ELIF},
    {"else",      TOKEN_ELSE},
    {"enum",      TOKEN_ENUM},
    {"extended",  TOKEN_EXTENDED},
    {"f128",      TOKEN_F128},
    {"f32",       TOKEN_F32},
    {"f64",       TOKEN_F64},
    {"f80",       TOKEN_F80},
    {"false",     TOKEN_FALSE},
    {"for",       TOKEN_FOR},
    {"generic",   TOKEN_GENERIC},
    {"i16",       TOKEN_I16},
    {"i32",       TOKEN_I32},
    {"i64",       TOKEN_I64},
    {"i8",        TOKEN_I8},
    {"if",        TOKEN_IF},
    {"import",    TOKEN_IMPORT},
    {"int",       TOKEN_INT_TYPE},
    {"interface", TOKEN_INTERFACE},
    {"macro",     TOKEN_MACRO},
    {"methods",   TOKEN_METHODS},
    {"mut",       TOKEN_MUT},
    {"not",       TOKEN_NOT},
    {"null",      TOKEN_NULL},
    {"or",        TOKEN_OR},
    {"quad",      TOKEN_QUAD},
    {"return",    TOKEN_RETURN},
    {"self",      TOKEN_SELF},
    {"single",    TOKEN_SINGLE},
    {"string",    TOKEN_STRING_TYPE},
    {"struct",    TOKEN_STRUCT},
    {"switch",    TOKEN_SWITCH},
    {"true",      TOKEN_TRUE},
    {"u16",       TOKEN_U16},
    {"u32",       TOKEN_U32},
    {"u64",       TOKEN_U64},
    {"u8",        TOKEN_U8},
    {"void",      TOKEN_VOID},
    {"where",     TOKEN_WHERE},
    {"while",     TOKEN_WHILE},
    {NULL,        TOKEN_EOF} // Терминатор (не участвует в поиске)
};


// Функция бинарного поиска (убедитесь, что она использует strcmp)
TokenType get_keyword_type(const char* str) {
    Keyword key = {str, TOKEN_EOF};
    // bsearch требует, чтобы массив был отсортирован по ключу (word)
    Keyword* result = (Keyword*)bsearch(&key, keywords, 
                                        sizeof(keywords) / sizeof(keywords[0]) - 1, 
                                        sizeof(Keyword), 
                                        (int (*)(const void*, const void*))strcmp);
    return result ? result->type : TOKEN_IDENTIFIER;
}

#define KEYWORD_COUNT (sizeof(keywords) / sizeof(keywords[0]) - 1)  // Вычитаем терминатор 

TokenType keyword_lookup(String name) {
    if (name.length == 0 || !name.data) return TOKEN_IDENTIFIER;
    
    // Бинарный поиск
    int left = 0;
    int right = (int)KEYWORD_COUNT - 1;
    
    while (left <= right) {
        int mid = left + (right - left) / 2;
        
        // БЕЗОПАСНОСТЬ: пропускаем NULL-терминатор
        if (!keywords[mid].name) {
            right = mid - 1;
            continue;
        }
        
        // Сравниваем строки
        size_t kw_len = strlen(keywords[mid].name);
        size_t min_len = name.length < kw_len ? name.length : kw_len;
        int cmp = memcmp(name.data, keywords[mid].name, min_len);
        
        if (cmp == 0) {
            if (name.length == kw_len) {
                return keywords[mid].type;  // Найдено!
            }
            cmp = (name.length < kw_len) ? -1 : 1;
        }
        
        if (cmp < 0) {
            right = mid - 1;
        } else {
            left = mid + 1;
        }
    }
    
    return TOKEN_IDENTIFIER;  // Не ключевое слово
}

int keyword_count(void) {
    return (int)KEYWORD_COUNT;
}
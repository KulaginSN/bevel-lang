// src/lexer/keywords.h
#ifndef BEVEL_KEYWORDS_H
#define BEVEL_KEYWORDS_H

#include "lexer/token.h"
#include "common/string.h"

// Ищет ключевое слово по имени.
// Возвращает соответствующий TokenType (например, TOKEN_IMPORT),
// либо TOKEN_IDENTIFIER, если строка не является ключевым словом.
TokenType keyword_lookup(String name);

// Возвращает количество ключевых слов в языке (для отладки)
int keyword_count(void);

#endif // BEVEL_KEYWORDS_H
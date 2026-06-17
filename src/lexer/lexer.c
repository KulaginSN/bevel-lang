// src/lexer/lexer.c
#include "lexer/lexer.h"
#include "lexer/keywords.h"
#include "common/utils.h"
#include <ctype.h>
#include <stdio.h>
#include <string.h>

// --- Реализация token_type_name ---
const char* token_type_name(TokenType type) {
    switch (type) {
        case TOKEN_EOF: return "EOF";
        case TOKEN_ERROR: return "ERROR";
        case TOKEN_NEWLINE: return "NEWLINE";
        case TOKEN_INDENT: return "INDENT";
        case TOKEN_DEDENT: return "DEDENT";
        
        case TOKEN_INT_LIT: return "INT_LIT";
        case TOKEN_FLOAT_LIT: return "FLOAT_LIT";
        case TOKEN_STRING_LIT: return "STRING_LIT";
        case TOKEN_CHAR_LIT: return "CHAR_LIT";
        
        case TOKEN_IDENTIFIER: return "IDENTIFIER";
        
        case TOKEN_VOID: return "VOID";
        case TOKEN_BOOL_TYPE: return "BOOL_TYPE";
        case TOKEN_CHAR_TYPE: return "CHAR_TYPE";
        case TOKEN_STRING_TYPE: return "STRING_TYPE";
        case TOKEN_I8: return "I8";
        case TOKEN_I16: return "I16";
        case TOKEN_I32: return "I32";
        case TOKEN_I64: return "I64";
        case TOKEN_U8: return "U8";
        case TOKEN_U16: return "U16";
        case TOKEN_U32: return "U32";
        case TOKEN_U64: return "U64";
        case TOKEN_F32: return "F32";
        case TOKEN_F64: return "F64";
        case TOKEN_F80: return "F80";
        case TOKEN_F128: return "F128";
        case TOKEN_SINGLE: return "SINGLE";
        case TOKEN_DOUBLE: return "DOUBLE";
        case TOKEN_EXTENDED: return "EXTENDED";
        case TOKEN_QUAD: return "QUAD";
        case TOKEN_INT_TYPE: return "INT_TYPE";
        
        case TOKEN_IMPORT: return "IMPORT";
        case TOKEN_AS: return "AS";
        case TOKEN_STRUCT: return "STRUCT";
        case TOKEN_ENUM: return "ENUM";
        case TOKEN_INTERFACE: return "INTERFACE";
        case TOKEN_METHODS: return "METHODS";
        case TOKEN_GENERIC: return "GENERIC";
        case TOKEN_WHERE: return "WHERE";
        
        case TOKEN_CONST: return "CONST";
        case TOKEN_MUT: return "MUT";
        case TOKEN_SELF: return "SELF";
        case TOKEN_DEFER: return "DEFER";
        case TOKEN_MACRO: return "MACRO";
        
        case TOKEN_IF: return "IF";
        case TOKEN_ELIF: return "ELIF";
        case TOKEN_ELSE: return "ELSE";
        case TOKEN_WHILE: return "WHILE";
        case TOKEN_FOR: return "FOR";
        case TOKEN_BREAK: return "BREAK";
        case TOKEN_CONTINUE: return "CONTINUE";
        case TOKEN_SWITCH: return "SWITCH";
        case TOKEN_CASE: return "CASE";
        case TOKEN_DEFAULT: return "DEFAULT";
        case TOKEN_RETURN: return "RETURN";
        
        case TOKEN_TRUE: return "TRUE";
        case TOKEN_FALSE: return "FALSE";
        case TOKEN_NULL: return "NULL";
        
        case TOKEN_PLUS: return "PLUS";
        case TOKEN_MINUS: return "MINUS";
        case TOKEN_STAR: return "STAR";
        case TOKEN_SLASH: return "SLASH";
        case TOKEN_PERCENT: return "PERCENT";
        case TOKEN_EQ: return "EQ";
        case TOKEN_EQEQ: return "EQEQ";
        case TOKEN_NOTEQ: return "NOTEQ";
        case TOKEN_LT: return "LT";
        case TOKEN_LTE: return "LTE";
        case TOKEN_GT: return "GT";
        case TOKEN_GTE: return "GTE";
        case TOKEN_AND: return "AND";
        case TOKEN_OR: return "OR";
        case TOKEN_NOT: return "NOT";
        case TOKEN_AMPERSAND: return "AMPERSAND";
        case TOKEN_PIPE: return "PIPE";
        case TOKEN_CARET: return "CARET";
        case TOKEN_TILDE: return "TILDE";
        case TOKEN_LSHIFT: return "LSHIFT";
        case TOKEN_RSHIFT: return "RSHIFT";
        case TOKEN_PLUSPLUS: return "PLUSPLUS";
        case TOKEN_MINUSMINUS: return "MINUSMINUS";
        case TOKEN_PLUSEQ: return "PLUSEQ";
        case TOKEN_MINUSEQ: return "MINUSEQ";
        case TOKEN_STAREQ: return "STAREQ";
        case TOKEN_SLASHEQ: return "SLASHEQ";
        case TOKEN_ARROW: return "ARROW";
        case TOKEN_AT: return "AT";
        
        case TOKEN_LPAREN: return "LPAREN";
        case TOKEN_RPAREN: return "RPAREN";
        case TOKEN_LBRACKET: return "LBRACKET";
        case TOKEN_RBRACKET: return "RBRACKET";
        case TOKEN_LBRACE: return "LBRACE";
        case TOKEN_RBRACE: return "RBRACE";
        case TOKEN_COLON: return "COLON";
        case TOKEN_SEMICOLON: return "SEMICOLON";
        case TOKEN_COMMA: return "COMMA";
        case TOKEN_DOT: return "DOT";
        
        default: return "UNKNOWN";
    }
}

// --- Вспомогательные функции ---

static bool is_at_end(Lexer* l) {
    return *l->current == '\0';
}

static char advance(Lexer* l) {
    char c = *l->current;
    l->current++;
    l->column++;
    if (c == '\n') {
        l->line++;
        l->column = 1;
    }
    return c;
}

static char peek(Lexer* l) {
    return *l->current;
}

static char peek_next(Lexer* l) {
    if (is_at_end(l)) return '\0';
    return l->current[1];
}

static bool match(Lexer* l, char expected) {
    if (is_at_end(l)) return false;
    if (*l->current != expected) return false;
    advance(l);
    return true;
}

static Token make_token(Lexer* l, TokenType type) {
    int col = l->column - (int)(l->current - l->start);
    if (col < 1) col = 1;
    return token_new(type, string_new(l->start, l->current - l->start), l->line, col);
}

static Token error_token(Lexer* l, const char* message) {
    error_reporter_add(l->errors, ERROR_LEXER, l->line, l->column, 1, message, NULL);
    return token_error(l->line, l->column);
}

// ===== Пропуск пробелов и комментариев =====
static void skip_whitespace(Lexer* l) {
    while (!is_at_end(l)) {
        char c = peek(l);
        
        // Пропускаем пробельные символы
        if (c == ' ' || c == '\r' || c == '\t') {
            advance(l);
            continue;
        }
        
        // Новая строка — конец пропуска пробелов (важно для индентации!)
        if (c == '\n') {
            break;
        }
        
        // Однострочный комментарий: // ... до конца строки
        if (c == '/' && peek_next(l) == '/') {
            advance(l);  // пропустить первый /
            advance(l);  // пропустить второй /
            while (!is_at_end(l) && peek(l) != '\n') {
                advance(l);  // пропустить всё до \n
            }
            continue;
        }
        
        // Многострочный комментарий: /* ... */
        if (c == '/' && peek_next(l) == '*') {
            advance(l);  // пропустить /
            advance(l);  // пропустить *
            while (!is_at_end(l)) {
                if (peek(l) == '*' && peek_next(l) == '/') {
                    advance(l);  // пропустить *
                    advance(l);  // пропустить /
                    break;  // конец комментария
                }
                if (peek(l) == '\n') {
                    l->line++;  // считать новую строку внутри комментария
                }
                advance(l);
            }
            continue;
        }
        
        // Любой другой символ — конец пробелов
        break;
    }
}

// --- Распознавание литералов и идентификаторов ---

static Token number(Lexer* l) {
    while (is_digit(peek(l))) advance(l);

    if (peek(l) == '.' && is_digit(peek_next(l))) {
        advance(l);
        while (is_digit(peek(l))) advance(l);
        return make_token(l, TOKEN_FLOAT_LIT);
    }

    return make_token(l, TOKEN_INT_LIT);
}

static Token identifier(Lexer* l) {
    while (is_alnum(peek(l)) || peek(l) == '_') advance(l);

    String text = string_new(l->start, l->current - l->start);
    
    // ИСПОЛЬЗУЕМ МОДУЛЬ keywords
    TokenType type = keyword_lookup(text);
    return make_token(l, type);
}

// --- Основной цикл лексера ---

void lexer_init(Lexer* l, const char* source, Arena* arena, ErrorReporter* errors) {
    l->source = source;
    l->start = source;
    l->current = source;
    l->line = 1;
    l->column = 1;
    l->arena = arena;
    l->errors = errors;
    
    l->indent_stack[0] = 0;
    l->indent_depth = 0;
    l->at_line_start = true;
    l->pending_dedents = 0;
}

Token lexer_next_token(Lexer* l) {

    skip_whitespace(l);

    while (true) {
        // СНАЧАЛА выдаём все накопленные DEDENT
        if (l->pending_dedents > 0) {
            l->pending_dedents--;
            return token_new(TOKEN_DEDENT, STRING_EMPTY, l->line, 1);
        }
        
        if (is_at_end(l)) {
            if (l->indent_depth > 0) {
                l->indent_depth--;
                l->pending_dedents = l->indent_depth; // Все оставшиеся DEDENT
                return token_new(TOKEN_DEDENT, STRING_EMPTY, l->line, l->column);
            }
            return token_eof(l->line, l->column);
        }

        if (l->at_line_start) {
            int spaces = 0;
            while (peek(l) == ' ' || peek(l) == '\t') {
                if (peek(l) == '\t') spaces += 4;
                else spaces++;
                advance(l);
            }

            // Пустая строка или комментарий — пропускаем
            if (peek(l) == '#' || peek(l) == '\n' || peek(l) == '\r' || is_at_end(l)) {
                while (peek(l) != '\n' && !is_at_end(l)) advance(l);
                if (peek(l) == '\n') advance(l);
                continue;
            }

            int current_indent = spaces;
            int top_indent = l->indent_stack[l->indent_depth];

            l->at_line_start = false;

            if (current_indent > top_indent) {
                l->indent_depth++;
                if (l->indent_depth < 32) {
                    l->indent_stack[l->indent_depth] = current_indent;
                }
                l->start = l->current;
                return token_new(TOKEN_INDENT, STRING_EMPTY, l->line, 1);
            } else if (current_indent < top_indent) {
                // СЧИТАЕМ, сколько DEDENT нужно выдать
                int dedents_needed = 0;
                int temp_depth = l->indent_depth;
                while (temp_depth > 0 && current_indent < l->indent_stack[temp_depth]) {
                    temp_depth--;
                    dedents_needed++;
                }
                
                // Обновляем реальную глубину
                l->indent_depth = temp_depth;
                
                // Первый DEDENT выдаём сразу, остальные запоминаем
                if (dedents_needed > 1) {
                    l->pending_dedents = dedents_needed - 1;
                }
                
                l->start = l->current;
                return token_new(TOKEN_DEDENT, STRING_EMPTY, l->line, 1);
            }
            // current_indent == top_indent — просто продолжаем
        }

        // Пропуск пробелов внутри строки
        while (peek(l) == ' ' || peek(l) == '\t' || peek(l) == '\r') {
            advance(l);
        }

        if (peek(l) == '#') {
            while (peek(l) != '\n' && !is_at_end(l)) advance(l);
            continue;
        }

        if (is_at_end(l)) continue;

        l->start = l->current;
        char c = advance(l);

        if (c == '\n') {
            l->at_line_start = true;
            continue;
        }

        if (is_alpha(c) || c == '_') return identifier(l);
        if (is_digit(c)) return number(l);

        switch (c) {
            case '(': return make_token(l, TOKEN_LPAREN);
            case ')': return make_token(l, TOKEN_RPAREN);
            case '[': return make_token(l, TOKEN_LBRACKET);
            case ']': return make_token(l, TOKEN_RBRACKET);
            case ':': return make_token(l, TOKEN_COLON);
            case ';': return make_token(l, TOKEN_SEMICOLON);
            case ',': return make_token(l, TOKEN_COMMA);
            case '.': return make_token(l, TOKEN_DOT);
            
            case '+': return match(l, '+') ? make_token(l, TOKEN_PLUSPLUS) : (match(l, '=') ? make_token(l, TOKEN_PLUSEQ) : make_token(l, TOKEN_PLUS));
            case '-': return match(l, '-') ? make_token(l, TOKEN_MINUSMINUS) : (match(l, '=') ? make_token(l, TOKEN_MINUSEQ) : (match(l, '>') ? make_token(l, TOKEN_ARROW) : make_token(l, TOKEN_MINUS)));
            case '*': return match(l, '=') ? make_token(l, TOKEN_STAREQ) : make_token(l, TOKEN_STAR);
            case '/': return match(l, '=') ? make_token(l, TOKEN_SLASHEQ) : make_token(l, TOKEN_SLASH);
            case '%': return make_token(l, TOKEN_PERCENT);
            
            case '=': return match(l, '=') ? make_token(l, TOKEN_EQEQ) : make_token(l, TOKEN_EQ);
            case '!': return match(l, '=') ? make_token(l, TOKEN_NOTEQ) : make_token(l, TOKEN_NOT);
            case '<': return match(l, '=') ? make_token(l, TOKEN_LTE) : (match(l, '<') ? make_token(l, TOKEN_LSHIFT) : make_token(l, TOKEN_LT));
            case '>': return match(l, '=') ? make_token(l, TOKEN_GTE) : (match(l, '>') ? make_token(l, TOKEN_RSHIFT) : make_token(l, TOKEN_GT));
            
            case '&': return match(l, '&') ? make_token(l, TOKEN_AND) : make_token(l, TOKEN_AMPERSAND);
            case '|': return match(l, '|') ? make_token(l, TOKEN_OR) : make_token(l, TOKEN_PIPE);
            case '^': return make_token(l, TOKEN_CARET);
            case '~': return make_token(l, TOKEN_TILDE);
            case '@': return make_token(l, TOKEN_AT);

            case '"': {
                while (peek(l) != '"' && !is_at_end(l)) {
                    if (peek(l) == '\n') l->line++;
                    advance(l);
                }
                if (is_at_end(l)) return error_token(l, "Unterminated string.");
                advance(l);
                return make_token(l, TOKEN_STRING_LIT);
            }
            
            case '\'': {
                while (peek(l) != '\'' && !is_at_end(l)) advance(l);
                if (is_at_end(l)) return error_token(l, "Unterminated character.");
                advance(l); 
                return make_token(l, TOKEN_CHAR_LIT);
            }
        }

        return error_token(l, "Unexpected character.");
    }
}

int lexer_indent_level(Lexer* l) {
    return l ? l->indent_depth : 0;
}

// Заглядывание вперёд на один токен без потребления
Token lexer_peek_token(Lexer* l) {
    // Сохраняем ВСЕ состояние лексера
    const char* saved_start = l->start;
    const char* saved_current = l->current;
    int saved_line = l->line;
    int saved_column = l->column;
    int saved_indent_depth = l->indent_depth;
    bool saved_at_line_start = l->at_line_start;
    int saved_pending_dedents = l->pending_dedents;
    
    // Копируем стек отступов
    int saved_indent_stack[32];
    for (int i = 0; i < 32; i++) {
        saved_indent_stack[i] = l->indent_stack[i];
    }
    
    // Читаем следующий токен
    Token peeked = lexer_next_token(l);
    
    // Восстанавливаем ВСЕ состояние
    l->start = saved_start;
    l->current = saved_current;
    l->line = saved_line;
    l->column = saved_column;
    l->indent_depth = saved_indent_depth;
    l->at_line_start = saved_at_line_start;
    l->pending_dedents = saved_pending_dedents;
    
    for (int i = 0; i < 32; i++) {
        l->indent_stack[i] = saved_indent_stack[i];
    }
    
    return peeked;
}
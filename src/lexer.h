#pragma once

#include <stddef.h>

#include "range.h"

typedef struct {
    const char *buffer;
    uint32_t index;
} Lexer;

typedef enum {
    TOK_EOF,
    TOK_INVALID,
    TOK_IDENTIFIER,
    TOK_INT,
    TOK_FLOAT,
    TOK_STRING,
    TOK_OPAREN,
    TOK_CPAREN,
    TOK_OBRACE,
    TOK_CBRACE,
    TOK_OBRACKET,
    TOK_CBRACKET,
    TOK_LARROW,
    TOK_RARROW,
    TOK_ASSIGN,
    TOK_EQL,
    TOK_LOGICAL_NOT,
    TOK_NOT_EQL,
    TOK_DOT,
    TOK_COMMA,
    TOK_COLON,
    TOK_PLUS,
    TOK_PLUS_ASSIGN,
    TOK_MINUS,
    TOK_MINUS_ASSIGN,
    TOK_MULTIPLY,
    TOK_MULTIPLY_ASSIGN,
    TOK_DIVIDE,
    TOK_DIVIDE_ASSIGN,
    TOK_MODULO,
    TOK_MODULO_ASSIGN,
    TOK_EXPONENT,
    TOK_EXPONENT_ASSIGN,
    TOK_LESS_THAN,
    TOK_LESS_THAN_OR_EQL,
    TOK_GREATER_THAN,
    TOK_GREATER_THAN_OR_EQL,
    TOK_KEYWORD_IF,
    TOK_KEYWORD_ELSE,
    TOK_KEYWORD_WHILE,
    TOK_KEYWORD_BREAK,
    TOK_KEYWORD_CONTINUE,
    TOK_KEYWORD_FN,
    TOK_KEYWORD_RETURN,
} TokenTag;

typedef struct {
    Range range;
    TokenTag tag;
} Token;

Token lexer_next(Lexer *);

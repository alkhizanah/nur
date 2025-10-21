#pragma once

#include <stddef.h>

#include "range.h"

typedef struct {
    const char *buffer;
    size_t index;
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
    TOK_ASSIGN,
    TOK_EQL,
    TOK_LOGICAL_NOT,
    TOK_NOT_EQL,
    TOK_DOT,
    TOK_COMMA,
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
    TOK_LESS_THAN,
    TOK_LESS_THAN_OR_EQL,
    TOK_GREATER_THAN,
    TOK_GREATER_THAN_OR_EQL,
} TokenTag;

typedef struct {
    Range range;
    TokenTag tag;
} Token;

Token lexer_next(Lexer *);

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
} TokenTag;

typedef struct {
    Range range;
    TokenTag tag;
} Token;

Token lexer_next(Lexer *);

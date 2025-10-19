#include <ctype.h>
#include <stdbool.h>

#include "lexer.h"

Token lexer_next(Lexer *lexer) {
    while (isspace(lexer->buffer[lexer->index]))
        lexer->index++;

    Token token = {.range = {lexer->index, lexer->index}};

    char character = lexer->buffer[lexer->index++];

    switch (character) {
    case 0:
        token.tag = TOK_EOF;
        break;

    case '(':
        token.tag = TOK_OPAREN;
        token.range.end = lexer->index;
        break;

    case ')':
        token.tag = TOK_CPAREN;
        token.range.end = lexer->index;
        break;

    case '{':
        token.tag = TOK_OBRACE;
        token.range.end = lexer->index;
        break;

    case '}':
        token.tag = TOK_CBRACE;
        token.range.end = lexer->index;
        break;

    case '[':
        token.tag = TOK_OBRACKET;
        token.range.end = lexer->index;
        break;

    case ']':
        token.tag = TOK_CBRACKET;
        token.range.end = lexer->index;
        break;

    case '"':
    case '\'': {
        token.tag = TOK_STRING;

        token.range.start = lexer->index;

        bool escaping = false;

        while (escaping || lexer->buffer[lexer->index++] != character) {
            if (escaping) {
                escaping = false;
            } else if (lexer->buffer[lexer->index] == '\\') {
                escaping = true;
            }
        }

        token.range.end = lexer->index - 1;

        break;
    }

    default:
        if (isalpha(character)) {
            token.tag = TOK_IDENTIFIER;

            while (isalpha(lexer->buffer[lexer->index]))
                lexer->index++;

            token.range.end = lexer->index;
        } else if (isdigit(character)) {
            token.tag = TOK_INT;

            while (isdigit(lexer->buffer[lexer->index]) ||
                   lexer->buffer[lexer->index] == '.') {
                if (lexer->buffer[lexer->index] == '.') {
                    token.tag = TOK_FLOAT;
                }

                lexer->index++;
            }

            token.range.end = lexer->index;
        } else {
            token.tag = TOK_INVALID;
            token.range.end = lexer->index;
        }

        break;
    }

    return token;
}

#include <ctype.h>
#include <stdbool.h>

#include "lexer.h"

Token lexer_next(Lexer *lexer) {
    while (true) {
        while (isspace(lexer->buffer[lexer->index]))
            lexer->index++;

        Token token = {.range = {lexer->index, lexer->index}};

        char character = lexer->buffer[lexer->index++];

        switch (character) {
        case 0:
            token.tag = TOK_EOF;
            break;

        case '#':
            while (lexer->buffer[lexer->index] != '\n')
                lexer->index++;

            continue;

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

        case '=':
            if (lexer->buffer[lexer->index] == '=') {
                token.tag = TOK_EQL;
                token.range.end = ++lexer->index;
            } else {
                token.tag = TOK_ASSIGN;
                token.range.end = lexer->index;
            }

            break;

        case '!':
            if (lexer->buffer[lexer->index] == '=') {
                token.tag = TOK_NOT_EQL;
                token.range.end = ++lexer->index;
            } else {
                token.tag = TOK_LOGICAL_NOT;
                token.range.end = lexer->index;
            }

            break;

        case '+':
            if (lexer->buffer[lexer->index] == '=') {
                token.tag = TOK_PLUS_ASSIGN;
                token.range.end = ++lexer->index;
            } else {
                token.tag = TOK_PLUS;
                token.range.end = lexer->index;
            }

            break;

        case '-':
            if (lexer->buffer[lexer->index] == '=') {
                token.tag = TOK_MINUS_ASSIGN;
                token.range.end = ++lexer->index;
            } else {
                token.tag = TOK_MINUS;
                token.range.end = lexer->index;
            }

            break;

        case '*':
            if (lexer->buffer[lexer->index] == '*') {
                if (lexer->buffer[++lexer->index] == '=') {
                    token.tag = TOK_EXPONENT_ASSIGN;
                    token.range.end = ++lexer->index;
                } else {
                    token.tag = TOK_EXPONENT;
                    token.range.end = lexer->index;
                }
            } else if (lexer->buffer[lexer->index] == '=') {
                token.tag = TOK_MULTIPLY_ASSIGN;
                token.range.end = ++lexer->index;
            } else {
                token.tag = TOK_MULTIPLY;
                token.range.end = lexer->index;
            }

            break;

        case '/':
            if (lexer->buffer[lexer->index] == '=') {
                token.tag = TOK_DIVIDE_ASSIGN;
                token.range.end = ++lexer->index;
            } else {
                token.tag = TOK_DIVIDE;
                token.range.end = lexer->index;
            }

            break;

        case '%':
            if (lexer->buffer[lexer->index] == '=') {
                token.tag = TOK_MODULO_ASSIGN;
                token.range.end = ++lexer->index;
            } else {
                token.tag = TOK_MODULO;
                token.range.end = lexer->index;
            }

            break;

        case '<':
            if (lexer->buffer[lexer->index] == '=') {
                token.tag = TOK_LESS_THAN_OR_EQL;
                token.range.end = ++lexer->index;
            } else {
                token.tag = TOK_LESS_THAN;
                token.range.end = lexer->index;
            }

            break;

        case '>':
            if (lexer->buffer[lexer->index] == '=') {
                token.tag = TOK_GREATER_THAN_OR_EQL;
                token.range.end = ++lexer->index;
            } else {
                token.tag = TOK_GREATER_THAN;
                token.range.end = lexer->index;
            }

            break;

        case '.':
            token.tag = TOK_DOT;
            token.range.end = lexer->index;
            break;

        case ',':
            token.tag = TOK_COMMA;
            token.range.end = lexer->index;
            break;

        case ':':
            token.tag = TOK_COLON;
            token.range.end = lexer->index;
            break;

        case ';':
            token.tag = TOK_SEMICOLON;
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
                } else if (lexer->buffer[lexer->index - 1] == '\0' ||
                           lexer->buffer[lexer->index - 1] == '\n') {
                    token.tag = TOK_INVALID;
                    token.range.end = lexer->index;
                    break;
                }
            }

            token.range.end = lexer->index - 1;

            break;
        }

        default:
            if (isalpha(character) || character == '_') {
                token.tag = TOK_IDENTIFIER;

                while (isalnum(lexer->buffer[lexer->index]) ||
                       lexer->buffer[lexer->index] == '_')
                    lexer->index++;

                token.range.end = lexer->index;
            } else if (isdigit(character)) {
                token.tag = TOK_INT;

                while (isalnum(lexer->buffer[lexer->index]) ||
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
}

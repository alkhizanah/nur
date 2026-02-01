#include <stdbool.h>
#include <stddef.h>
#include <string.h>

#include "lexer.h"

static const uint8_t CHAR_TABLE[256] = {
    [' '] = 1 << 0,  ['\t'] = 1 << 0,        ['\n'] = 1 << 0,
    ['\r'] = 1 << 0, ['a' ... 'z'] = 1 << 1, ['A' ... 'Z'] = 1 << 1,
    ['_'] = 1 << 1,  ['0' ... '9'] = 1 << 2};

static inline bool is_space(char c) {
    return (CHAR_TABLE[(uint8_t)c] & (1 << 0)) != 0;
}

static inline bool is_identifier_continuation(char c) {
    return (CHAR_TABLE[(uint8_t)c] & ((1 << 1) | (1 << 2))) != 0;
}

static inline bool is_identifier_beginning(char c) {
    return (CHAR_TABLE[(uint8_t)c] & (1 << 1)) != 0;
}

static inline bool is_digit(char c) {
    return (CHAR_TABLE[(uint8_t)c] & (1 << 2)) != 0;
}

Token lexer_next(Lexer *lexer) {
retry:
    while (is_space(lexer->buffer[lexer->index]))
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

        goto retry;

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
        } else if (lexer->buffer[lexer->index] == '>') {
            token.tag = TOK_RARROW;
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
        } else if (lexer->buffer[lexer->index] == '-') {
            token.tag = TOK_LARROW;
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

    case '"':
    case '\'': {
        token.tag = TOK_STRING;

        token.range.start = lexer->index;

        bool escaping = false;

        while (true) {
            if (escaping) {
                escaping = false;
            } else if (lexer->buffer[lexer->index] == '\\') {
                escaping = true;
            } else if (lexer->buffer[lexer->index] == '\0' ||
                       lexer->buffer[lexer->index] == '\n') {
                token.tag = TOK_INVALID;
                token.range.end = lexer->index;
                break;
            } else if (lexer->buffer[lexer->index] == character) {
                lexer->index++;

                break;
            }

            lexer->index++;
        }

        token.range.end = lexer->index - 1;

        break;
    }

    default:
        if (is_identifier_beginning(character)) {
            while (is_identifier_continuation(lexer->buffer[lexer->index]))
                lexer->index++;

            token.range.end = lexer->index;

            token.tag = TOK_IDENTIFIER;

            const char *s = lexer->buffer + token.range.start;
            uint32_t len = token.range.end - token.range.start;

            if (len == 2 && s[0] == 'i' && s[1] == 'f') {
                token.tag = TOK_KEYWORD_IF;
            } else if (len == 4 && s[0] == 'e' && s[1] == 'l' && s[2] == 's' &&
                       s[3] == 'e') {
                token.tag = TOK_KEYWORD_ELSE;
            } else if (len == 5 && s[0] == 'w' && s[1] == 'h' && s[2] == 'i' &&
                       s[3] == 'l' && s[4] == 'e') {
                token.tag = TOK_KEYWORD_WHILE;
            } else if (len == 5 && s[0] == 'b' && s[1] == 'r' && s[2] == 'e' &&
                       s[3] == 'a' && s[4] == 'k') {
                token.tag = TOK_KEYWORD_BREAK;
            } else if (len == 8 && s[0] == 'c' && s[1] == 'o' && s[2] == 'n' &&
                       s[3] == 't' && s[4] == 'i' && s[5] == 'n' &&
                       s[6] == 'u' && s[7] == 'e') {
                token.tag = TOK_KEYWORD_CONTINUE;
            } else if (len == 2 && s[0] == 'f' && s[1] == 'n') {
                token.tag = TOK_KEYWORD_FN;
            } else if (len == 6 && s[0] == 'r' && s[1] == 'e' && s[2] == 't' &&
                       s[3] == 'u' && s[4] == 'r' && s[5] == 'n') {
                token.tag = TOK_KEYWORD_RETURN;
            }
        } else if (is_digit(character)) {
            token.tag = TOK_INT;

            while (is_identifier_continuation(lexer->buffer[lexer->index]) ||
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

#include <stdbool.h>

#include "array.h"
#include "ast.h"
#include "diagnoser.h"
#include "lexer.h"
#include "source_location.h"

typedef enum : uint8_t {
    PR_LOWEST,
    PR_ASSIGN,
    PR_COMPARISON,
    PR_SUM,
    PR_PRODUCT,
    PR_EXPONENT,
    PR_PREFIX,
    PR_CALL,
    PR_SUBSCRIPT,
} OperatorPrecedence;

static OperatorPrecedence operator_precedence_of(TokenTag token) {
    switch (token) {
    case TOK_ASSIGN:
    case TOK_PLUS_ASSIGN:
    case TOK_MINUS_ASSIGN:
    case TOK_MULTIPLY_ASSIGN:
    case TOK_DIVIDE_ASSIGN:
    case TOK_EXPONENT_ASSIGN:
    case TOK_MODULO_ASSIGN:
        return PR_ASSIGN;

    case TOK_NOT_EQL:
    case TOK_EQL:
    case TOK_GREATER_THAN:
    case TOK_LESS_THAN:
    case TOK_GREATER_THAN_OR_EQL:
    case TOK_LESS_THAN_OR_EQL:
        return PR_COMPARISON;

    case TOK_PLUS:
    case TOK_MINUS:
        return PR_SUM;

    case TOK_MULTIPLY:
    case TOK_DIVIDE:
    case TOK_MODULO:
        return PR_PRODUCT;

    case TOK_EXPONENT:
        return PR_EXPONENT;

    case TOK_OPAREN:
        return PR_CALL;

    case TOK_OBRACKET:
    case TOK_DOT:
        return PR_SUBSCRIPT;

    default:
        return PR_LOWEST;
    }
}

static AstNodeIdx ast_push_node(AstParser *parser, AstNodeTag tag,
                                AstNodePayload payload) {
    if (parser->ast.nodes.len + 1 > parser->ast.nodes.capacity) {
        size_t new_cap =
            parser->ast.nodes.capacity ? parser->ast.nodes.capacity * 2 : 4;

        parser->ast.nodes.tags = realloc(
            parser->ast.nodes.tags, sizeof(*parser->ast.nodes.tags) * new_cap);

        parser->ast.nodes.payloads =
            realloc(parser->ast.nodes.payloads,
                    sizeof(*parser->ast.nodes.payloads) * new_cap);

        if (parser->ast.nodes.tags == NULL ||
            parser->ast.nodes.payloads == NULL) {
            fprintf(stderr, "error: out of memory\n");

            // We shouldn't return INVALID_NODE_IDX as being out of memory is
            // irrecoverible (not sure)
            exit(1);
        }

        parser->ast.nodes.capacity = new_cap;
    }

    parser->ast.nodes.tags[parser->ast.nodes.len] = tag;
    parser->ast.nodes.payloads[parser->ast.nodes.len] = payload;

    return parser->ast.nodes.len++;
}

static AstNodeIdx ast_parse_binary_expr(AstParser *parser, AstNodeIdx lhs) {
    switch (lexer_peek(&parser->lexer).tag) {
    default:
        diagnoser_error(source_location_of(parser->file_path,
                                           parser->lexer.buffer,
                                           lexer_peek(&parser->lexer).range),
                        "unknown binary operator\n");

        return INVALID_NODE_IDX;
    }
}

static AstNodeIdx ast_parse_identifier(AstParser *parser) {
    Token token = lexer_next(&parser->lexer);

    return ast_push_node(
        parser, NODE_IDENTIFIER,
        (AstNodePayload){.lhs = token.range.start, .rhs = token.range.end});
}

static AstNodeIdx ast_parse_string(AstParser *parser) {
    Token token = lexer_next(&parser->lexer);

    bool unescaping = false;

    uint32_t unescaped_string_start = parser->ast.strings.len;

    size_t len = token.range.end - token.range.start;

    for (size_t i = 0; i < len; i++) {
        const char escaped = parser->lexer.buffer[token.range.start + i];

        char unescaped;

        if (unescaping) {
            switch (escaped) {
            case '\\':
                unescaped = '\\';
                break;
            case 'n':
                unescaped = '\n';
                break;
            case 'r':
                unescaped = '\r';
                break;
            case 't':
                unescaped = '\t';
                break;
            case 'e':
                unescaped = 27;
                break;
            case 'v':
                unescaped = 11;
                break;
            case 'b':
                unescaped = 8;
                break;
            case 'f':
                unescaped = 12;
                break;
            case '"':
                unescaped = '"';
                break;
            default:
                diagnoser_error(
                    source_location_of(parser->file_path, parser->lexer.buffer,
                                       (Range){
                                           .start = token.range.start + i,
                                           .end = token.range.start + i + 1,
                                       }),
                    "invalid string escape character: '%c'\n", escaped);

                return INVALID_NODE_IDX;
            }

            unescaping = false;
        } else if (escaped == '\\') {
            unescaping = true;
            continue;
        } else {
            unescaped = escaped;
        }

        ARRAY_PUSH(&parser->ast.strings, unescaped);
    }

    uint32_t unescaped_string_end = parser->ast.strings.len;

    return ast_push_node(
        parser, NODE_STRING,
        (AstNodePayload){.lhs = unescaped_string_start,
                         .rhs = unescaped_string_end - unescaped_string_start});
}

static AstNodeIdx ast_parse_int(AstParser *parser) {
    Token token = lexer_next(&parser->lexer);

    uint64_t v = 0;

    const char *s = parser->lexer.buffer + token.range.start;
    uint32_t s_len = token.range.end - token.range.start;

    for (uint32_t i = 0; i < s_len; i++) {
        // TODO: This is assuming decimal digits only, hexadecmial (0x0), octal
        // (0o0), binary (0b0) need special treatment
        if (s[i] < '0' || s[i] > '9') {
            diagnoser_error(source_location_of(parser->file_path,
                                               parser->lexer.buffer,
                                               token.range),
                            "unsuitable digit in number: '%c'\n", s[i]);

            return INVALID_NODE_IDX;
        }

        uint64_t digit = s[i] - '0';

        if (v > UINT64_MAX / 10 || (v == UINT64_MAX / 10 && digit > UINT64_MAX % 10)) {
            diagnoser_error(source_location_of(parser->file_path,
                                               parser->lexer.buffer,
                                               token.range),
                            "number is too big to be represented\n");

            return INVALID_NODE_IDX;
        }

        v = v * 10 + digit;
    }

    return ast_push_node(parser, NODE_INT,
                         (AstNodePayload){
                             .lhs = v >> 32,
                             .rhs = v,
                         });
}

static AstNodeIdx ast_parse_unary_expr(AstParser *parser) {
    switch (lexer_peek(&parser->lexer).tag) {
    case TOK_IDENTIFIER:
        return ast_parse_identifier(parser);
    case TOK_STRING:
        return ast_parse_string(parser);
    case TOK_INT:
        return ast_parse_int(parser);
    default:
        diagnoser_error(source_location_of(parser->file_path,
                                           parser->lexer.buffer,
                                           lexer_peek(&parser->lexer).range),
                        "unknown expression\n");

        return INVALID_NODE_IDX;
    }
}

static AstNodeIdx ast_parse_expr(AstParser *parser,
                                 OperatorPrecedence precedence) {
    AstNodeIdx lhs = ast_parse_unary_expr(parser);

    while (operator_precedence_of(lexer_peek(&parser->lexer).tag) >
           precedence) {
        if (lhs == INVALID_NODE_IDX) {
            return INVALID_NODE_IDX;
        }

        lhs = ast_parse_binary_expr(parser, lhs);
    }

    return lhs;
}

static AstNodeIdx ast_parse_stmt(AstParser *parser) {
    switch (lexer_peek(&parser->lexer).tag) {
    default:
        return ast_parse_expr(parser, PR_LOWEST);
    }
}

bool ast_parse(AstParser *parser) {
    // We intentionally make our own AstExtra so it won't be modified by
    // other "ast_parse_*" functions
    AstExtra stmts = {0};

    while (lexer_peek(&parser->lexer).tag != TOK_EOF) {
        AstNodeIdx stmt = ast_parse_stmt(parser);

        if (stmt == INVALID_NODE_IDX) {
            return false;
        }

        ARRAY_PUSH(&stmts, stmt);
    }

    uint32_t stmts_index = parser->ast.extra.len;

    ARRAY_EXPAND(&parser->ast.extra, stmts.items, stmts.len);

    ast_push_node(parser, NODE_BLOCK,
                  (AstNodePayload){.lhs = stmts_index, .rhs = stmts.len});

    ARRAY_FREE(&stmts);

    return true;
}

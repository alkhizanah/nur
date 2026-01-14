#include <assert.h>
#include <ctype.h>
#include <errno.h>
#include <float.h>
#include <stdbool.h>

#include "array.h"
#include "ast.h"
#include "diagnoser.h"
#include "lexer.h"
#include "parser.h"
#include "source_location.h"

typedef enum : uint8_t {
    PR_LOWEST,
    PR_ASSIGN,
    PR_COMPARISON,
    PR_SUM,
    PR_PRODUCT,
    PR_EXPONENT,
    PR_PREFIX,
    PR_POSTFIX,
} Precedence;

static Precedence precedence_of(TokenTag token) {
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
    case TOK_OBRACKET:
    case TOK_DOT:
        return PR_POSTFIX;

    default:
        return PR_LOWEST;
    }
}

static inline Token parser_advance(AstParser *parser) {
    parser->current_token = parser->next_token;
    parser->next_token = lexer_next(&parser->lexer);
    return parser->current_token;
}

static inline Token parser_peek(const AstParser *parser) { return parser->next_token; }

static AstNodeIdx parse_stmt(AstParser *parser);

static AstNodeIdx parse_expr(AstParser *parser,
                                 Precedence precedence);

static AstNodeIdx parse_block(AstParser *parser) {
    Token token = parser_advance(parser);

    AstExtra stmts = {0};

    while (parser_peek(parser).tag != TOK_CBRACE) {
        AstNodeIdx stmt = parse_stmt(parser);

        if (stmt == INVALID_NODE_IDX) {
            return INVALID_NODE_IDX;
        }

        if (parser_peek(parser).tag == TOK_EOF) {
            diagnoser_error(source_location_of(parser->file_path,
                                               parser->lexer.buffer,
                                               token.range),
                            "'{' did not get closed\n");

            return INVALID_NODE_IDX;
        }

        ARRAY_PUSH(&stmts, stmt);
    }

    parser_advance(parser);

    uint32_t stmts_start = parser->ast.extra.count;

    ARRAY_EXPAND(&parser->ast.extra, stmts.items, stmts.count);

    AstNodeIdx block = ast_push(&parser->ast, NODE_BLOCK, stmts_start,
                                stmts.count, token.range.start);

    ARRAY_FREE(&stmts);

    return block;
}

static AstNodeIdx parse_assign(AstParser *parser, AstNodeIdx target,
                                   AstNodeTag tag) {
    Token token = parser_advance(parser);

    AstNodeIdx value = parse_expr(parser, PR_ASSIGN);

    if (value == INVALID_NODE_IDX) {
        return INVALID_NODE_IDX;
    }

    return ast_push(&parser->ast, tag, target, value, token.range.start);
}

static AstNodeIdx parse_call(AstParser *parser, AstNodeIdx callee) {
    Token token = parser_advance(parser);

    AstExtra arguments = {0};

    while (parser_peek(parser).tag != TOK_CPAREN) {
        AstNodeIdx argument = parse_expr(parser, PR_LOWEST);

        if (argument == INVALID_NODE_IDX) {
            return INVALID_NODE_IDX;
        }

        if (parser_peek(parser).tag == TOK_COMMA) {
            parser_advance(parser);
        }

        if (parser_peek(parser).tag == TOK_EOF) {
            diagnoser_error(source_location_of(parser->file_path,
                                               parser->lexer.buffer,
                                               token.range),
                            "'(' did not get closed\n");

            return INVALID_NODE_IDX;
        }

        ARRAY_PUSH(&arguments, argument);
    }

    parser_advance(parser);

    if (arguments.count == 0) {
        return ast_push(&parser->ast, NODE_CALL, callee, INVALID_EXTRA_IDX,
                        token.range.start);
    } else {
        uint32_t start = parser->ast.extra.count;

        ARRAY_PUSH(&parser->ast.extra, arguments.count);

        ARRAY_EXPAND(&parser->ast.extra, arguments.items, arguments.count);

        ARRAY_FREE(&arguments);

        return ast_push(&parser->ast, NODE_CALL, callee, start,
                        token.range.start);
    }
}

static AstNodeIdx parse_identifier(AstParser *parser) {
    Token token = parser_advance(parser);

    return ast_push(&parser->ast, NODE_IDENTIFIER, token.range.start,
                    token.range.end, token.range.start);
}

static AstNodeIdx parse_member_access(AstParser *parser,
                                          AstNodeIdx target) {
    Token token = parser_advance(parser);

    if (parser_peek(parser).tag != TOK_IDENTIFIER) {
        diagnoser_error(source_location_of(parser->file_path,
                                           parser->lexer.buffer,
                                           parser_peek(parser).range),
                        "expected an identifier after '.'\n");

        return INVALID_NODE_IDX;
    }

    AstNodeIdx identifier = parse_identifier(parser);

    return ast_push(&parser->ast, NODE_MEMBER, target, identifier,
                    token.range.start);
}

static AstNodeIdx parse_subscript_access(AstParser *parser,
                                             AstNodeIdx target) {
    Token token = parser_advance(parser);

    AstNodeIdx start = parse_expr(parser, PR_LOWEST);

    if (parser_peek(parser).tag != TOK_CBRACKET) {
        diagnoser_error(source_location_of(parser->file_path,
                                           parser->lexer.buffer, token.range),
                        "'[' did not get closed\n");

        return INVALID_NODE_IDX;
    }

    parser_advance(parser);

    return ast_push(&parser->ast, NODE_SUBSCRIPT, target, start,
                    token.range.start);
}

static AstNodeIdx parse_binary_op(AstParser *parser, AstNodeIdx lhs,
                                      AstNodeTag tag,
                                      Precedence precedence) {
    Token token = parser_advance(parser);

    AstNodeIdx rhs = parse_expr(parser, precedence);

    if (rhs == INVALID_NODE_IDX) {
        return INVALID_NODE_IDX;
    }

    return ast_push(&parser->ast, tag, lhs, rhs, token.range.start);
}

static AstNodeIdx parse_binary_expr(AstParser *parser, AstNodeIdx lhs) {
    switch (parser_peek(parser).tag) {
    case TOK_ASSIGN:
        return parse_assign(parser, lhs, NODE_ASSIGN);
    case TOK_PLUS_ASSIGN:
        return parse_assign(parser, lhs, NODE_ASSIGN_ADD);
    case TOK_MINUS_ASSIGN:
        return parse_assign(parser, lhs, NODE_ASSIGN_SUB);
    case TOK_MULTIPLY_ASSIGN:
        return parse_assign(parser, lhs, NODE_ASSIGN_MUL);
    case TOK_DIVIDE_ASSIGN:
        return parse_assign(parser, lhs, NODE_ASSIGN_DIV);
    case TOK_EXPONENT_ASSIGN:
        return parse_assign(parser, lhs, NODE_ASSIGN_POW);
    case TOK_MODULO_ASSIGN:
        return parse_assign(parser, lhs, NODE_ASSIGN_MOD);
    case TOK_OPAREN:
        return parse_call(parser, lhs);
    case TOK_DOT:
        return parse_member_access(parser, lhs);
    case TOK_OBRACKET:
        return parse_subscript_access(parser, lhs);
    case TOK_PLUS:
        return parse_binary_op(parser, lhs, NODE_ADD, PR_SUM);
    case TOK_MINUS:
        return parse_binary_op(parser, lhs, NODE_SUB, PR_SUM);
    case TOK_MULTIPLY:
        return parse_binary_op(parser, lhs, NODE_MUL, PR_PRODUCT);
    case TOK_DIVIDE:
        return parse_binary_op(parser, lhs, NODE_DIV, PR_PRODUCT);
    case TOK_EXPONENT:
        return parse_binary_op(parser, lhs, NODE_POW, PR_EXPONENT);
    case TOK_MODULO:
        return parse_binary_op(parser, lhs, NODE_MOD, PR_PRODUCT);
    case TOK_EQL:
        return parse_binary_op(parser, lhs, NODE_EQL, PR_COMPARISON);
    case TOK_NOT_EQL:
        return parse_binary_op(parser, lhs, NODE_NEQ, PR_COMPARISON);
    case TOK_LESS_THAN:
        return parse_binary_op(parser, lhs, NODE_LT, PR_COMPARISON);
    case TOK_LESS_THAN_OR_EQL:
        return parse_binary_op(parser, lhs, NODE_LTE, PR_COMPARISON);
    case TOK_GREATER_THAN:
        return parse_binary_op(parser, lhs, NODE_GT, PR_COMPARISON);
    case TOK_GREATER_THAN_OR_EQL:
        return parse_binary_op(parser, lhs, NODE_GTE, PR_COMPARISON);
    default:
        diagnoser_error(source_location_of(parser->file_path,
                                           parser->lexer.buffer,
                                           parser_peek(parser).range),
                        "unknown binary operator\n");

        return INVALID_NODE_IDX;
    }
}

static AstNodeIdx parse_string(AstParser *parser) {
    Token token = parser_advance(parser);

    bool unescaping = false;

    uint32_t unescaped_string_start = parser->ast.strings.count;

    size_t count = token.range.end - token.range.start;

    for (size_t i = 0; i < count; i++) {
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

    uint32_t unescaped_string_end = parser->ast.strings.count;

    return ast_push(&parser->ast, NODE_STRING, unescaped_string_start,
                    unescaped_string_end - unescaped_string_start,
                    token.range.start);
}

static AstNodeIdx parse_int(AstParser *parser) {
    Token token = parser_advance(parser);

    char *endptr;

    errno = 0;

    uint64_t v = strtoull(parser->lexer.buffer + token.range.start, &endptr, 0);

    if (errno == ERANGE) {
        diagnoser_error(source_location_of(parser->file_path,
                                           parser->lexer.buffer, token.range),
                        "number too big to be represented\n");

        return INVALID_NODE_IDX;
    }

    if (endptr != parser->lexer.buffer + token.range.end) {
        diagnoser_error(source_location_of(parser->file_path,
                                           parser->lexer.buffer, token.range),
                        "unsuitable digit in number: '%c'\n", *endptr);

        return INVALID_NODE_IDX;
    }

    return ast_push(&parser->ast, NODE_INT, v >> 32, v, token.range.start);
}

static AstNodeIdx parse_float(AstParser *parser) {
    Token token = parser_advance(parser);

    char *endptr;

    errno = 0;

    double v = strtod(parser->lexer.buffer + token.range.start, &endptr);

    if (errno == ERANGE) {
        diagnoser_error(source_location_of(parser->file_path,
                                           parser->lexer.buffer, token.range),
                        "number too big to be represented\n");

        return INVALID_NODE_IDX;
    }

    if (endptr != parser->lexer.buffer + token.range.end) {
        diagnoser_error(source_location_of(parser->file_path,
                                           parser->lexer.buffer, token.range),
                        "unsuitable digit in number: '%c'\n", *endptr);

        return INVALID_NODE_IDX;
    }

    uint64_t vi;

    memcpy(&vi, &v, sizeof(double));

    return ast_push(&parser->ast, NODE_FLOAT, vi >> 32, vi, token.range.start);
}

static AstNodeIdx parse_parentheses_expr(AstParser *parser) {
    Token token = parser_advance(parser);

    AstNodeIdx value = parse_expr(parser, PR_LOWEST);

    if (value == INVALID_NODE_IDX) {
        return INVALID_NODE_IDX;
    }

    if (parser_peek(parser).tag != TOK_CPAREN) {
        diagnoser_error(source_location_of(parser->file_path,
                                           parser->lexer.buffer, token.range),
                        "'(' did not get closed\n");

        return INVALID_NODE_IDX;
    }

    parser_advance(parser);

    return value;
}

static AstNodeIdx parse_function(AstParser *parser) {
    Token fn_token = parser_advance(parser);

    if (parser_peek(parser).tag != TOK_OPAREN) {
        diagnoser_error(source_location_of(parser->file_path,
                                           parser->lexer.buffer,
                                           parser_peek(parser).range),
                        "expected '('\n");

        return INVALID_NODE_IDX;
    }

    Token oparen_token = parser_advance(parser);

    AstExtra parameters = {0};

    while (parser_peek(parser).tag != TOK_CPAREN) {
        if (parser_peek(parser).tag != TOK_IDENTIFIER) {
            diagnoser_error(source_location_of(parser->file_path,
                                               parser->lexer.buffer,
                                               parser_peek(parser).range),
                            "expected a parameter name to be an identifier\n");

            return INVALID_NODE_IDX;
        }

        AstNodeIdx parameter = parse_identifier(parser);

        if (parameter == INVALID_NODE_IDX) {
            return INVALID_NODE_IDX;
        }

        if (parser_peek(parser).tag == TOK_COMMA) {
            parser_advance(parser);
        }

        if (parser_peek(parser).tag == TOK_EOF) {
            diagnoser_error(source_location_of(parser->file_path,
                                               parser->lexer.buffer,
                                               oparen_token.range),
                            "'(' did not get closed\n");

            return INVALID_NODE_IDX;
        }

        ARRAY_PUSH(&parameters, parameter);
    }

    parser_advance(parser);

    if (parser_peek(parser).tag != TOK_OBRACE) {
        diagnoser_error(source_location_of(parser->file_path,
                                           parser->lexer.buffer,
                                           parser_peek(parser).range),
                        "expected '{'\n");

        return INVALID_NODE_IDX;
    }

    AstNodeIdx block = parse_block(parser);

    if (block == INVALID_NODE_IDX) {
        return INVALID_NODE_IDX;
    }

    if (parameters.count == 0) {
        return ast_push(&parser->ast, NODE_FUNCTION, INVALID_EXTRA_IDX, block,
                        fn_token.range.start);
    } else {
        uint32_t start = parser->ast.extra.count;

        ARRAY_PUSH(&parser->ast.extra, parameters.count);

        ARRAY_EXPAND(&parser->ast.extra, parameters.items, parameters.count);

        ARRAY_FREE(&parameters);

        return ast_push(&parser->ast, NODE_FUNCTION, start, block,
                        fn_token.range.start);
    }
}

static AstNodeIdx parse_array(AstParser *parser) {
    Token token = parser_advance(parser);

    AstExtra values = {0};

    while (parser_peek(parser).tag != TOK_CBRACKET) {
        AstNodeIdx value = parse_expr(parser, PR_LOWEST);

        if (value == INVALID_NODE_IDX) {
            return INVALID_NODE_IDX;
        }

        if (parser_peek(parser).tag == TOK_COMMA) {
            parser_advance(parser);
        }

        if (parser_peek(parser).tag == TOK_EOF) {
            diagnoser_error(source_location_of(parser->file_path,
                                               parser->lexer.buffer,
                                               token.range),
                            "'[' did not get closed\n");

            return INVALID_NODE_IDX;
        }

        ARRAY_PUSH(&values, value);
    }

    parser_advance(parser);

    uint32_t start = parser->ast.extra.count;

    ARRAY_EXPAND(&parser->ast.extra, values.items, values.count);

    uint32_t count = values.count;

    ARRAY_FREE(&values);

    return ast_push(&parser->ast, NODE_ARRAY, start, count, token.range.start);
}

static AstNodeIdx parse_map(AstParser *parser) {
    Token token = parser_advance(parser);

    AstExtra keys = {0};
    AstExtra values = {0};

    while (parser_peek(parser).tag != TOK_CBRACE) {
        AstNodeIdx key = parse_expr(parser, PR_LOWEST);

        if (key == INVALID_NODE_IDX) {
            return INVALID_NODE_IDX;
        }

        if (parser_peek(parser).tag != TOK_COLON) {
            diagnoser_error(source_location_of(parser->file_path,
                                               parser->lexer.buffer,
                                               parser_peek(parser).range),
                            "expected ':'\n");

            return INVALID_NODE_IDX;
        }

        parser_advance(parser);

        AstNodeIdx value = parse_expr(parser, PR_LOWEST);

        if (value == INVALID_NODE_IDX) {
            return INVALID_NODE_IDX;
        }

        if (parser_peek(parser).tag == TOK_COMMA) {
            parser_advance(parser);
        }

        if (parser_peek(parser).tag == TOK_EOF) {
            diagnoser_error(source_location_of(parser->file_path,
                                               parser->lexer.buffer,
                                               token.range),
                            "'{' did not get closed\n");

            return INVALID_NODE_IDX;
        }

        ARRAY_PUSH(&keys, key);
        ARRAY_PUSH(&values, value);
    }

    parser_advance(parser);

    if (keys.count == 0) {
        return ast_push(&parser->ast, NODE_MAP, INVALID_EXTRA_IDX,
                        INVALID_EXTRA_IDX, token.range.start);
    } else {
        uint32_t keys_start = parser->ast.extra.count;

        ARRAY_PUSH(&parser->ast.extra, keys.count);

        ARRAY_EXPAND(&parser->ast.extra, keys.items, keys.count);

        ARRAY_FREE(&keys);

        uint32_t values_start = parser->ast.extra.count;

        ARRAY_EXPAND(&parser->ast.extra, values.items, values.count);

        ARRAY_FREE(&values);

        return ast_push(&parser->ast, NODE_MAP, keys_start, values_start,
                        token.range.start);
    }
}

static AstNodeIdx parse_unary_op(AstParser *parser, AstNodeTag tag) {
    Token token = parser_advance(parser);

    AstNodeIdx value = parse_expr(parser, PR_PREFIX);

    if (value == INVALID_NODE_IDX) {
        return INVALID_NODE_IDX;
    }

    return ast_push(&parser->ast, tag, 0, value, token.range.start);
}

static AstNodeIdx parse_unary_expr(AstParser *parser) {
    switch (parser_peek(parser).tag) {
    case TOK_IDENTIFIER:
        return parse_identifier(parser);
    case TOK_STRING:
        return parse_string(parser);
    case TOK_INT:
        return parse_int(parser);
    case TOK_FLOAT:
        return parse_float(parser);
    case TOK_OPAREN:
        return parse_parentheses_expr(parser);
    case TOK_KEYWORD_FN:
        return parse_function(parser);
    case TOK_OBRACKET:
        return parse_array(parser);
    case TOK_OBRACE:
        return parse_map(parser);
    case TOK_MINUS:
        return parse_unary_op(parser, NODE_NEG);
    case TOK_LOGICAL_NOT:
        return parse_unary_op(parser, NODE_NOT);
    default:
        diagnoser_error(source_location_of(parser->file_path,
                                           parser->lexer.buffer,
                                           parser_peek(parser).range),
                        "unknown expression\n");

        return INVALID_NODE_IDX;
    }
}

static AstNodeIdx parse_expr(AstParser *parser,
                                 Precedence precedence) {
    AstNodeIdx lhs = parse_unary_expr(parser);

    while (precedence_of(parser_peek(parser).tag) > precedence) {
        if (lhs == INVALID_NODE_IDX) {
            return INVALID_NODE_IDX;
        }

        lhs = parse_binary_expr(parser, lhs);
    }

    return lhs;
}

static AstNodeIdx parse_while_loop(AstParser *parser) {
    Token token = parser_advance(parser);

    AstNodeIdx condition = parse_expr(parser, PR_LOWEST);

    if (condition == INVALID_NODE_IDX) {
        return INVALID_NODE_IDX;
    }

    if (parser_peek(parser).tag != TOK_OBRACE) {
        diagnoser_error(source_location_of(parser->file_path,
                                           parser->lexer.buffer,
                                           parser_peek(parser).range),
                        "expected '{'\n");

        return INVALID_NODE_IDX;
    }

    AstNodeIdx block = parse_block(parser);

    if (block == INVALID_NODE_IDX) {
        return INVALID_NODE_IDX;
    }

    return ast_push(&parser->ast, NODE_WHILE,

                    condition, block, token.range.start);
}

static AstNodeIdx parse_conditional(AstParser *parser) {
    Token token = parser_advance(parser);

    AstNodeIdx condition = parse_expr(parser, PR_LOWEST);

    if (condition == INVALID_NODE_IDX) {
        return INVALID_NODE_IDX;
    }

    if (parser_peek(parser).tag != TOK_OBRACE) {
        diagnoser_error(source_location_of(parser->file_path,
                                           parser->lexer.buffer,
                                           parser_peek(parser).range),
                        "expected '{'\n");

        return INVALID_NODE_IDX;
    }

    AstNodeIdx true_case = parse_block(parser);

    if (true_case == INVALID_NODE_IDX) {
        return INVALID_NODE_IDX;
    }

    AstNodeIdx false_case = INVALID_NODE_IDX;

    if (parser_peek(parser).tag == TOK_KEYWORD_ELSE) {
        parser_advance(parser);

        switch (parser_peek(parser).tag) {
        case TOK_KEYWORD_IF:
            false_case = parse_conditional(parser);

            if (false_case == INVALID_NODE_IDX) {
                return INVALID_NODE_IDX;
            }

            break;

        case TOK_OBRACE:
            false_case = parse_block(parser);

            if (false_case == INVALID_NODE_IDX) {
                return INVALID_NODE_IDX;
            }

            break;

        default: {
            diagnoser_error(source_location_of(parser->file_path,
                                               parser->lexer.buffer,
                                               parser_peek(parser).range),
                            "expected '{' or 'if'\n");

            return INVALID_NODE_IDX;
        }
        }
    }

    uint32_t rhs = parser->ast.extra.count;

    ARRAY_PUSH(&parser->ast.extra, true_case);
    ARRAY_PUSH(&parser->ast.extra, false_case);

    return ast_push(&parser->ast, NODE_IF, condition, rhs, token.range.start);
}

static AstNodeIdx parse_return(AstParser *parser) {
    Token token = parser_advance(parser);

    AstNodeIdx value = parse_expr(parser, PR_LOWEST);

    if (value == INVALID_NODE_IDX) {
        return INVALID_NODE_IDX;
    }

    return ast_push(&parser->ast, NODE_RETURN, 0, value, token.range.start);
}

static AstNodeIdx parse_stmt(AstParser *parser) {
    switch (parser_peek(parser).tag) {
    case TOK_OBRACE:
        return parse_block(parser);
    case TOK_KEYWORD_WHILE:
        return parse_while_loop(parser);
    case TOK_KEYWORD_IF:
        return parse_conditional(parser);
    case TOK_KEYWORD_RETURN:
        return parse_return(parser);
    case TOK_KEYWORD_BREAK:
        return ast_push(&parser->ast, NODE_BREAK, 0, 0,
                        parser_advance(parser).range.start);
    case TOK_KEYWORD_CONTINUE:
        return ast_push(&parser->ast, NODE_CONTINUE, 0, 0,
                        parser_advance(parser).range.start);
    default:
        return parse_expr(parser, PR_LOWEST);
    }
}

AstNodeIdx parse(AstParser *parser) {
    AstExtra stmts = {0};

    parser_advance(parser);

    while (parser_peek(parser).tag != TOK_EOF) {
        AstNodeIdx stmt = parse_stmt(parser);

        if (stmt == INVALID_NODE_IDX) {
            return INVALID_NODE_IDX;
        }

        ARRAY_PUSH(&stmts, stmt);
    }

    uint32_t stmts_start = parser->ast.extra.count;

    ARRAY_EXPAND(&parser->ast.extra, stmts.items, stmts.count);

    AstNodeIdx program =
        ast_push(&parser->ast, NODE_BLOCK, stmts_start, stmts.count, 0);

    ARRAY_FREE(&stmts);

    return program;
}

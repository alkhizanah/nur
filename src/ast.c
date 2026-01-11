#include <assert.h>
#include <ctype.h>
#include <errno.h>
#include <float.h>
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
    PR_POSTFIX,
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
    case TOK_OBRACKET:
    case TOK_DOT:
        return PR_POSTFIX;

    default:
        return PR_LOWEST;
    }
}

static inline Token advance(AstParser *parser) {
    parser->current_token = parser->next_token;
    parser->next_token = lexer_next(&parser->lexer);
    return parser->current_token;
}

static inline Token peek(const AstParser *parser) { return parser->next_token; }

static AstNodeIdx ast_push_node(AstParser *parser, AstNodeTag tag,
                                AstNodeIdx lhs, AstNodeIdx rhs,
                                uint32_t source) {
    AstNode node = {
        .lhs = lhs,
        .rhs = rhs,
        .tag = tag,
    };

    AstNodeIdx i = parser->ast.nodes.len;

    ARRAY_PUSH(&parser->ast.nodes, node);

    parser->ast.nodes.sources = realloc(parser->ast.nodes.sources,
                                        parser->ast.nodes.capacity *
                                            sizeof(*parser->ast.nodes.sources));

    if (parser->ast.nodes.sources == NULL) {
        fprintf(stderr, "error: out of memory\n");

        exit(1);
    }

    parser->ast.nodes.sources[i] = source;

    return i;
}

static AstNodeIdx ast_parse_stmt(AstParser *parser);

static AstNodeIdx ast_parse_expr(AstParser *parser,
                                 OperatorPrecedence precedence);

static AstNodeIdx ast_parse_block(AstParser *parser) {
    Token token = advance(parser);

    AstExtra stmts = {0};

    while (peek(parser).tag != TOK_CBRACE) {
        AstNodeIdx stmt = ast_parse_stmt(parser);

        if (stmt == INVALID_NODE_IDX) {
            return INVALID_NODE_IDX;
        }

        if (peek(parser).tag == TOK_EOF) {
            diagnoser_error(source_location_of(parser->file_path,
                                               parser->lexer.buffer,
                                               token.range),
                            "'{' did not get closed\n");

            return INVALID_NODE_IDX;
        }

        ARRAY_PUSH(&stmts, stmt);
    }

    advance(parser);

    uint32_t stmts_start = parser->ast.extra.len;

    ARRAY_EXPAND(&parser->ast.extra, stmts.items, stmts.len);

    AstNodeIdx block = ast_push_node(parser, NODE_BLOCK, stmts_start, stmts.len,
                                     token.range.start);

    ARRAY_FREE(&stmts);

    return block;
}

static AstNodeIdx ast_parse_assign(AstParser *parser, AstNodeIdx target,
                                   AstNodeTag tag) {
    Token token = advance(parser);

    AstNodeIdx value = ast_parse_expr(parser, PR_ASSIGN);

    if (value == INVALID_NODE_IDX) {
        return INVALID_NODE_IDX;
    }

    return ast_push_node(parser, tag, target, value, token.range.start);
}

static AstNodeIdx ast_parse_call(AstParser *parser, AstNodeIdx callee) {
    Token token = advance(parser);

    AstExtra arguments = {0};

    while (peek(parser).tag != TOK_CPAREN) {
        AstNodeIdx argument = ast_parse_expr(parser, PR_LOWEST);

        if (argument == INVALID_NODE_IDX) {
            return INVALID_NODE_IDX;
        }

        if (peek(parser).tag == TOK_COMMA) {
            advance(parser);
        }

        if (peek(parser).tag == TOK_EOF) {
            diagnoser_error(source_location_of(parser->file_path,
                                               parser->lexer.buffer,
                                               token.range),
                            "'(' did not get closed\n");

            return INVALID_NODE_IDX;
        }

        ARRAY_PUSH(&arguments, argument);
    }

    advance(parser);

    if (arguments.len == 0) {
        return ast_push_node(parser, NODE_CALL, callee, INVALID_EXTRA_IDX,
                             token.range.start);
    } else {
        uint32_t start = parser->ast.extra.len;

        ARRAY_PUSH(&parser->ast.extra, arguments.len);

        ARRAY_EXPAND(&parser->ast.extra, arguments.items, arguments.len);

        ARRAY_FREE(&arguments);

        return ast_push_node(parser, NODE_CALL, callee, start,
                             token.range.start);
    }
}

static AstNodeIdx ast_parse_identifier(AstParser *parser) {
    Token token = advance(parser);

    return ast_push_node(parser, NODE_IDENTIFIER, token.range.start,
                         token.range.end, token.range.start);
}

static AstNodeIdx ast_parse_member_access(AstParser *parser,
                                          AstNodeIdx target) {
    Token token = advance(parser);

    if (peek(parser).tag != TOK_IDENTIFIER) {
        diagnoser_error(source_location_of(parser->file_path,
                                           parser->lexer.buffer,
                                           peek(parser).range),
                        "expected an identifier after '.'\n");

        return INVALID_NODE_IDX;
    }

    AstNodeIdx identifier = ast_parse_identifier(parser);

    return ast_push_node(parser, NODE_MEMBER, target, identifier,
                         token.range.start);
}

static AstNodeIdx ast_parse_subscript_access(AstParser *parser,
                                             AstNodeIdx target) {
    Token token = advance(parser);

    AstNodeIdx start = ast_parse_expr(parser, PR_LOWEST);

    if (peek(parser).tag != TOK_CBRACKET) {
        diagnoser_error(source_location_of(parser->file_path,
                                           parser->lexer.buffer, token.range),
                        "'[' did not get closed\n");

        return INVALID_NODE_IDX;
    }

    advance(parser);

    return ast_push_node(parser, NODE_SUBSCRIPT, target, start,
                         token.range.start);
}

static AstNodeIdx ast_parse_binary_op(AstParser *parser, AstNodeIdx lhs,
                                      AstNodeTag tag,
                                      OperatorPrecedence precedence) {
    Token token = advance(parser);

    AstNodeIdx rhs = ast_parse_expr(parser, precedence);

    if (rhs == INVALID_NODE_IDX) {
        return INVALID_NODE_IDX;
    }

    return ast_push_node(parser, tag, lhs, rhs, token.range.start);
}

static AstNodeIdx ast_parse_binary_expr(AstParser *parser, AstNodeIdx lhs) {
    switch (peek(parser).tag) {
    case TOK_ASSIGN:
        return ast_parse_assign(parser, lhs, NODE_ASSIGN);
    case TOK_PLUS_ASSIGN:
        return ast_parse_assign(parser, lhs, NODE_ASSIGN_ADD);
    case TOK_MINUS_ASSIGN:
        return ast_parse_assign(parser, lhs, NODE_ASSIGN_SUB);
    case TOK_MULTIPLY_ASSIGN:
        return ast_parse_assign(parser, lhs, NODE_ASSIGN_MUL);
    case TOK_DIVIDE_ASSIGN:
        return ast_parse_assign(parser, lhs, NODE_ASSIGN_DIV);
    case TOK_EXPONENT_ASSIGN:
        return ast_parse_assign(parser, lhs, NODE_ASSIGN_POW);
    case TOK_MODULO_ASSIGN:
        return ast_parse_assign(parser, lhs, NODE_ASSIGN_MOD);
    case TOK_OPAREN:
        return ast_parse_call(parser, lhs);
    case TOK_DOT:
        return ast_parse_member_access(parser, lhs);
    case TOK_OBRACKET:
        return ast_parse_subscript_access(parser, lhs);
    case TOK_PLUS:
        return ast_parse_binary_op(parser, lhs, NODE_ADD, PR_SUM);
    case TOK_MINUS:
        return ast_parse_binary_op(parser, lhs, NODE_SUB, PR_SUM);
    case TOK_MULTIPLY:
        return ast_parse_binary_op(parser, lhs, NODE_MUL, PR_PRODUCT);
    case TOK_DIVIDE:
        return ast_parse_binary_op(parser, lhs, NODE_DIV, PR_PRODUCT);
    case TOK_EXPONENT:
        return ast_parse_binary_op(parser, lhs, NODE_POW, PR_EXPONENT);
    case TOK_MODULO:
        return ast_parse_binary_op(parser, lhs, NODE_MOD, PR_PRODUCT);
    case TOK_EQL:
        return ast_parse_binary_op(parser, lhs, NODE_EQL, PR_COMPARISON);
    case TOK_NOT_EQL:
        return ast_parse_binary_op(parser, lhs, NODE_NEQ, PR_COMPARISON);
    case TOK_LESS_THAN:
        return ast_parse_binary_op(parser, lhs, NODE_LT, PR_COMPARISON);
    case TOK_LESS_THAN_OR_EQL:
        return ast_parse_binary_op(parser, lhs, NODE_LTE, PR_COMPARISON);
    case TOK_GREATER_THAN:
        return ast_parse_binary_op(parser, lhs, NODE_GT, PR_COMPARISON);
    case TOK_GREATER_THAN_OR_EQL:
        return ast_parse_binary_op(parser, lhs, NODE_GTE, PR_COMPARISON);
    default:
        diagnoser_error(source_location_of(parser->file_path,
                                           parser->lexer.buffer,
                                           peek(parser).range),
                        "unknown binary operator\n");

        return INVALID_NODE_IDX;
    }
}

static AstNodeIdx ast_parse_string(AstParser *parser) {
    Token token = advance(parser);

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

    return ast_push_node(parser, NODE_STRING, unescaped_string_start,
                         unescaped_string_end - unescaped_string_start,
                         token.range.start);
}

static AstNodeIdx ast_parse_int(AstParser *parser) {
    Token token = advance(parser);

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

    return ast_push_node(parser, NODE_INT, v >> 32, v, token.range.start);
}

static AstNodeIdx ast_parse_float(AstParser *parser) {
    Token token = advance(parser);

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

    return ast_push_node(parser, NODE_FLOAT, vi >> 32, vi, token.range.start);
}

static AstNodeIdx ast_parse_parentheses_expr(AstParser *parser) {
    Token token = advance(parser);

    AstNodeIdx value = ast_parse_expr(parser, PR_LOWEST);

    if (value == INVALID_NODE_IDX) {
        return INVALID_NODE_IDX;
    }

    if (peek(parser).tag != TOK_CPAREN) {
        diagnoser_error(source_location_of(parser->file_path,
                                           parser->lexer.buffer, token.range),
                        "'(' did not get closed\n");

        return INVALID_NODE_IDX;
    }

    advance(parser);

    return value;
}

static AstNodeIdx ast_parse_function(AstParser *parser) {
    Token fn_token = advance(parser);

    if (peek(parser).tag != TOK_OPAREN) {
        diagnoser_error(source_location_of(parser->file_path,
                                           parser->lexer.buffer,
                                           peek(parser).range),
                        "expected '('\n");

        return INVALID_NODE_IDX;
    }

    Token oparen_token = advance(parser);

    AstExtra parameters = {0};

    while (peek(parser).tag != TOK_CPAREN) {
        if (peek(parser).tag != TOK_IDENTIFIER) {
            diagnoser_error(source_location_of(parser->file_path,
                                               parser->lexer.buffer,
                                               peek(parser).range),
                            "expected a parameter name to be an identifier\n");

            return INVALID_NODE_IDX;
        }

        AstNodeIdx parameter = ast_parse_identifier(parser);

        if (parameter == INVALID_NODE_IDX) {
            return INVALID_NODE_IDX;
        }

        if (peek(parser).tag == TOK_COMMA) {
            advance(parser);
        }

        if (peek(parser).tag == TOK_EOF) {
            diagnoser_error(source_location_of(parser->file_path,
                                               parser->lexer.buffer,
                                               oparen_token.range),
                            "'(' did not get closed\n");

            return INVALID_NODE_IDX;
        }

        ARRAY_PUSH(&parameters, parameter);
    }

    advance(parser);

    if (peek(parser).tag != TOK_OBRACE) {
        diagnoser_error(source_location_of(parser->file_path,
                                           parser->lexer.buffer,
                                           peek(parser).range),
                        "expected '{'\n");

        return INVALID_NODE_IDX;
    }

    AstNodeIdx block = ast_parse_block(parser);

    if (block == INVALID_NODE_IDX) {
        return INVALID_NODE_IDX;
    }

    if (parameters.len == 0) {
        return ast_push_node(parser, NODE_FUNCTION, INVALID_EXTRA_IDX, block,
                             fn_token.range.start);
    } else {
        uint32_t start = parser->ast.extra.len;

        ARRAY_PUSH(&parser->ast.extra, parameters.len);

        ARRAY_EXPAND(&parser->ast.extra, parameters.items, parameters.len);

        ARRAY_FREE(&parameters);

        return ast_push_node(parser, NODE_FUNCTION, start, block,
                             fn_token.range.start);
    }
}

static AstNodeIdx ast_parse_array(AstParser *parser) {
    Token token = advance(parser);

    AstExtra values = {0};

    while (peek(parser).tag != TOK_CBRACKET) {
        AstNodeIdx value = ast_parse_expr(parser, PR_LOWEST);

        if (value == INVALID_NODE_IDX) {
            return INVALID_NODE_IDX;
        }

        if (peek(parser).tag == TOK_COMMA) {
            advance(parser);
        }

        if (peek(parser).tag == TOK_EOF) {
            diagnoser_error(source_location_of(parser->file_path,
                                               parser->lexer.buffer,
                                               token.range),
                            "'[' did not get closed\n");

            return INVALID_NODE_IDX;
        }

        ARRAY_PUSH(&values, value);
    }

    advance(parser);

    uint32_t start = parser->ast.extra.len;

    ARRAY_EXPAND(&parser->ast.extra, values.items, values.len);

    uint32_t len = values.len;

    ARRAY_FREE(&values);

    return ast_push_node(parser, NODE_ARRAY, start, len, token.range.start);
}

static AstNodeIdx ast_parse_map(AstParser *parser) {
    Token token = advance(parser);

    AstExtra keys = {0};
    AstExtra values = {0};

    while (peek(parser).tag != TOK_CBRACE) {
        AstNodeIdx key = ast_parse_expr(parser, PR_LOWEST);

        if (key == INVALID_NODE_IDX) {
            return INVALID_NODE_IDX;
        }

        if (peek(parser).tag != TOK_COLON) {
            diagnoser_error(source_location_of(parser->file_path,
                                               parser->lexer.buffer,
                                               peek(parser).range),
                            "expected ':'\n");

            return INVALID_NODE_IDX;
        }

        advance(parser);

        AstNodeIdx value = ast_parse_expr(parser, PR_LOWEST);

        if (value == INVALID_NODE_IDX) {
            return INVALID_NODE_IDX;
        }

        if (peek(parser).tag == TOK_COMMA) {
            advance(parser);
        }

        if (peek(parser).tag == TOK_EOF) {
            diagnoser_error(source_location_of(parser->file_path,
                                               parser->lexer.buffer,
                                               token.range),
                            "'{' did not get closed\n");

            return INVALID_NODE_IDX;
        }

        ARRAY_PUSH(&keys, key);
        ARRAY_PUSH(&values, value);
    }

    advance(parser);

    if (keys.len == 0) {
        return ast_push_node(parser, NODE_MAP, INVALID_EXTRA_IDX,
                             INVALID_EXTRA_IDX, token.range.start);
    } else {
        uint32_t keys_start = parser->ast.extra.len;

        ARRAY_PUSH(&parser->ast.extra, keys.len);

        ARRAY_EXPAND(&parser->ast.extra, keys.items, keys.len);

        ARRAY_FREE(&keys);

        uint32_t values_start = parser->ast.extra.len;

        ARRAY_EXPAND(&parser->ast.extra, values.items, values.len);

        ARRAY_FREE(&values);

        return ast_push_node(parser, NODE_MAP, keys_start, values_start,
                             token.range.start);
    }
}

static AstNodeIdx ast_parse_unary_op(AstParser *parser, AstNodeTag tag) {
    Token token = advance(parser);

    AstNodeIdx value = ast_parse_expr(parser, PR_PREFIX);

    if (value == INVALID_NODE_IDX) {
        return INVALID_NODE_IDX;
    }

    return ast_push_node(parser, tag, 0, value, token.range.start);
}

static AstNodeIdx ast_parse_unary_expr(AstParser *parser) {
    switch (peek(parser).tag) {
    case TOK_IDENTIFIER:
        return ast_parse_identifier(parser);
    case TOK_STRING:
        return ast_parse_string(parser);
    case TOK_INT:
        return ast_parse_int(parser);
    case TOK_FLOAT:
        return ast_parse_float(parser);
    case TOK_OPAREN:
        return ast_parse_parentheses_expr(parser);
    case TOK_KEYWORD_FN:
        return ast_parse_function(parser);
    case TOK_OBRACKET:
        return ast_parse_array(parser);
    case TOK_OBRACE:
        return ast_parse_map(parser);
    case TOK_MINUS:
        return ast_parse_unary_op(parser, NODE_NEG);
    case TOK_LOGICAL_NOT:
        return ast_parse_unary_op(parser, NODE_NOT);
    default:
        diagnoser_error(source_location_of(parser->file_path,
                                           parser->lexer.buffer,
                                           peek(parser).range),
                        "unknown expression\n");

        return INVALID_NODE_IDX;
    }
}

static AstNodeIdx ast_parse_expr(AstParser *parser,
                                 OperatorPrecedence precedence) {
    AstNodeIdx lhs = ast_parse_unary_expr(parser);

    while (operator_precedence_of(peek(parser).tag) > precedence) {
        if (lhs == INVALID_NODE_IDX) {
            return INVALID_NODE_IDX;
        }

        lhs = ast_parse_binary_expr(parser, lhs);
    }

    return lhs;
}

static AstNodeIdx ast_parse_while_loop(AstParser *parser) {
    Token token = advance(parser);

    AstNodeIdx condition = ast_parse_expr(parser, PR_LOWEST);

    if (condition == INVALID_NODE_IDX) {
        return INVALID_NODE_IDX;
    }

    if (peek(parser).tag != TOK_OBRACE) {
        diagnoser_error(source_location_of(parser->file_path,
                                           parser->lexer.buffer,
                                           peek(parser).range),
                        "expected '{'\n");

        return INVALID_NODE_IDX;
    }

    AstNodeIdx block = ast_parse_block(parser);

    if (block == INVALID_NODE_IDX) {
        return INVALID_NODE_IDX;
    }

    return ast_push_node(parser, NODE_WHILE,

                         condition, block, token.range.start);
}

static AstNodeIdx ast_parse_conditional(AstParser *parser) {
    Token token = advance(parser);

    AstNodeIdx condition = ast_parse_expr(parser, PR_LOWEST);

    if (condition == INVALID_NODE_IDX) {
        return INVALID_NODE_IDX;
    }

    if (peek(parser).tag != TOK_OBRACE) {
        diagnoser_error(source_location_of(parser->file_path,
                                           parser->lexer.buffer,
                                           peek(parser).range),
                        "expected '{'\n");

        return INVALID_NODE_IDX;
    }

    AstNodeIdx true_case = ast_parse_block(parser);

    if (true_case == INVALID_NODE_IDX) {
        return INVALID_NODE_IDX;
    }

    AstNodeIdx false_case = INVALID_NODE_IDX;

    if (peek(parser).tag == TOK_KEYWORD_ELSE) {
        advance(parser);

        switch (peek(parser).tag) {
        case TOK_KEYWORD_IF:
            false_case = ast_parse_conditional(parser);

            if (false_case == INVALID_NODE_IDX) {
                return INVALID_NODE_IDX;
            }

            break;

        case TOK_OBRACE:
            false_case = ast_parse_block(parser);

            if (false_case == INVALID_NODE_IDX) {
                return INVALID_NODE_IDX;
            }

            break;

        default: {
            diagnoser_error(source_location_of(parser->file_path,
                                               parser->lexer.buffer,
                                               peek(parser).range),
                            "expected '{' or 'if'\n");

            return INVALID_NODE_IDX;
        }
        }
    }

    uint32_t rhs = parser->ast.extra.len;

    ARRAY_PUSH(&parser->ast.extra, true_case);
    ARRAY_PUSH(&parser->ast.extra, false_case);

    return ast_push_node(parser, NODE_IF, condition, rhs, token.range.start);
}

static AstNodeIdx ast_parse_return(AstParser *parser) {
    Token token = advance(parser);

    if (peek(parser).tag == TOK_CBRACE) {
        // For example at the end of a function:
        //
        // fn () {
        //     return
        // }
        //
        // or at the end of an if statement;
        //
        // if val {
        //     return
        // }
        return ast_push_node(parser, NODE_RETURN, false, 0, token.range.start);
    }

    if (peek(parser).tag == TOK_SEMICOLON) {
        // Maybe to skip other code:
        //
        // fn () {
        //     return;
        //
        //     println(...);
        // }
        advance(parser);

        return ast_push_node(parser, NODE_RETURN, false, 0, token.range.start);
    }

    AstNodeIdx value = ast_parse_expr(parser, PR_LOWEST);

    if (value == INVALID_NODE_IDX) {
        return INVALID_NODE_IDX;
    }

    return ast_push_node(parser, NODE_RETURN, true, value, token.range.start);
}

static AstNodeIdx ast_parse_stmt(AstParser *parser) {
    switch (peek(parser).tag) {
    case TOK_OBRACE:
        return ast_parse_block(parser);
    case TOK_KEYWORD_WHILE:
        return ast_parse_while_loop(parser);
    case TOK_KEYWORD_IF:
        return ast_parse_conditional(parser);
    case TOK_KEYWORD_RETURN:
        return ast_parse_return(parser);
    case TOK_KEYWORD_BREAK:
        return ast_push_node(parser, NODE_BREAK, 0, 0,
                             advance(parser).range.start);
    case TOK_KEYWORD_CONTINUE:
        return ast_push_node(parser, NODE_CONTINUE, 0, 0,
                             advance(parser).range.start);
    default:
        return ast_parse_expr(parser, PR_LOWEST);
    }
}

AstNodeIdx ast_parse(AstParser *parser) {
    AstExtra stmts = {0};

    advance(parser);

    while (peek(parser).tag != TOK_EOF) {
        AstNodeIdx stmt = ast_parse_stmt(parser);

        if (stmt == INVALID_NODE_IDX) {
            return INVALID_NODE_IDX;
        }

        ARRAY_PUSH(&stmts, stmt);
    }

    uint32_t stmts_start = parser->ast.extra.len;

    ARRAY_EXPAND(&parser->ast.extra, stmts.items, stmts.len);

    AstNodeIdx program =
        ast_push_node(parser, NODE_BLOCK, stmts_start, stmts.len, 0);

    ARRAY_FREE(&stmts);

    return program;
}

void print_string_escaped(const char *s, size_t len) {
    for (size_t i = 0; i < len; i++) {
        unsigned char c = (unsigned char)s[i];

        switch (c) {
        case '\n':
            printf("\\n");
            break;
        case '\r':
            printf("\\r");
            break;
        case '\t':
            printf("\\t");
            break;
        case '\b':
            printf("\\b");
            break;
        case '\f':
            printf("\\f");
            break;
        case '\\':
            printf("\\\\");
            break;
        case '\"':
            printf("\\\"");
            break;
        default:
            if (isprint(c))
                putchar(c);
            else
                printf("\\x%02X", c);
        }
    }
}

void ast_display(const Ast *ast, const char *buffer, AstNodeIdx node_idx) {
    AstNode node = ast->nodes.items[node_idx];

    static int ast_display_depth = 0;

    switch (node.tag) {
    case NODE_BLOCK:
        printf("{\n");

        ast_display_depth += 1;

        for (uint32_t i = 0; i < node.rhs; i++) {
            printf("%*.s", ast_display_depth * 4, "");

            ast_display(ast, buffer, ast->extra.items[node.lhs + i]);

            printf("\n");
        }

        ast_display_depth -= 1;

        if (ast_display_depth > 0) {
            printf("%*.s", ast_display_depth * 4, "");
        }

        printf("}");

        break;

    case NODE_INT:
        printf("%lu", ((uint64_t)node.lhs << 32) | node.rhs);
        break;

    case NODE_FLOAT: {
        uint64_t vi = ((uint64_t)node.lhs << 32) | node.rhs;
        double v;
        memcpy(&v, &vi, sizeof(double));
        printf("%lf", v);
        break;
    }

    case NODE_IDENTIFIER:
        printf("%.*s", (int)(node.rhs - node.lhs), buffer + node.lhs);
        break;

    case NODE_STRING:
        printf("\"");
        print_string_escaped(ast->strings.items + node.lhs, node.rhs);
        printf("\"");
        break;

    case NODE_ASSIGN:
        ast_display(ast, buffer, node.lhs);
        printf(" = ");
        ast_display(ast, buffer, node.rhs);
        break;

    case NODE_ASSIGN_ADD:
        ast_display(ast, buffer, node.lhs);
        printf(" += ");
        ast_display(ast, buffer, node.rhs);
        break;

    case NODE_ASSIGN_SUB:
        ast_display(ast, buffer, node.lhs);
        printf(" -= ");
        ast_display(ast, buffer, node.rhs);
        break;

    case NODE_ASSIGN_DIV:
        ast_display(ast, buffer, node.lhs);
        printf(" /= ");
        ast_display(ast, buffer, node.rhs);
        break;

    case NODE_ASSIGN_MUL:
        ast_display(ast, buffer, node.lhs);
        printf(" *= ");
        ast_display(ast, buffer, node.rhs);
        break;

    case NODE_ASSIGN_POW:
        ast_display(ast, buffer, node.lhs);
        printf(" **= ");
        ast_display(ast, buffer, node.rhs);
        break;

    case NODE_ASSIGN_MOD:
        ast_display(ast, buffer, node.lhs);
        printf(" %%= ");
        ast_display(ast, buffer, node.rhs);
        break;

    case NODE_NEG:
        printf("-(");
        ast_display(ast, buffer, node.rhs);
        printf(")");
        break;

    case NODE_NOT:
        printf("!(");
        ast_display(ast, buffer, node.rhs);
        printf(")");
        break;

    case NODE_ADD:
        printf("(");
        ast_display(ast, buffer, node.lhs);
        printf(" + ");
        ast_display(ast, buffer, node.rhs);
        printf(")");
        break;

    case NODE_SUB:
        printf("(");
        ast_display(ast, buffer, node.lhs);
        printf(" - ");
        ast_display(ast, buffer, node.rhs);
        printf(")");
        break;

    case NODE_DIV:
        printf("(");
        ast_display(ast, buffer, node.lhs);
        printf(" / ");
        ast_display(ast, buffer, node.rhs);
        printf(")");
        break;

    case NODE_MUL:
        printf("(");
        ast_display(ast, buffer, node.lhs);
        printf(" * ");
        ast_display(ast, buffer, node.rhs);
        printf(")");
        break;

    case NODE_POW:
        printf("(");
        ast_display(ast, buffer, node.lhs);
        printf(" ** ");
        ast_display(ast, buffer, node.rhs);
        printf(")");
        break;

    case NODE_MOD:
        printf("(");
        ast_display(ast, buffer, node.lhs);
        printf(" %% ");
        ast_display(ast, buffer, node.rhs);
        printf(")");
        break;

    case NODE_EQL:
        printf("(");
        ast_display(ast, buffer, node.lhs);
        printf(")");
        printf(" == ");
        printf("(");
        ast_display(ast, buffer, node.rhs);
        printf(")");
        break;

    case NODE_NEQ:
        printf("(");
        ast_display(ast, buffer, node.lhs);
        printf(")");
        printf(" != ");
        printf("(");
        ast_display(ast, buffer, node.rhs);
        printf(")");
        break;

    case NODE_LT:
        printf("(");
        ast_display(ast, buffer, node.lhs);
        printf(")");
        printf(" < ");
        printf("(");
        ast_display(ast, buffer, node.rhs);
        printf(")");
        break;

    case NODE_LTE:
        printf("(");
        ast_display(ast, buffer, node.lhs);
        printf(")");
        printf(" <= ");
        printf("(");
        ast_display(ast, buffer, node.rhs);
        printf(")");
        break;

    case NODE_GT:
        printf("(");
        ast_display(ast, buffer, node.lhs);
        printf(")");
        printf(" > ");
        printf("(");
        ast_display(ast, buffer, node.rhs);
        printf(")");
        break;

    case NODE_GTE:
        printf("(");
        ast_display(ast, buffer, node.lhs);
        printf(")");
        printf(" >= ");
        printf("(");
        ast_display(ast, buffer, node.rhs);
        printf(")");
        break;

    case NODE_RETURN:
        printf("return");

        if (node.lhs) {
            printf(" ");
            ast_display(ast, buffer, node.rhs);
        } else {
            printf(";");
        }

        break;

    case NODE_BREAK:
        printf("break");
        break;

    case NODE_CONTINUE:
        printf("continue");
        break;

    case NODE_ARRAY: {
        uint32_t start = node.lhs;
        uint32_t len = node.rhs;

        printf("[");

        ast_display(ast, buffer, ast->extra.items[start]);

        for (uint32_t i = 1; i < len; i++) {
            printf(", ");
            ast_display(ast, buffer, ast->extra.items[start + i]);
        }

        printf("]");
        break;
    }

    case NODE_MAP:
        printf("{");

        if (node.lhs != INVALID_EXTRA_IDX) {
            uint32_t len = ast->extra.items[node.lhs];

            uint32_t keys_start = node.lhs + 1;
            uint32_t values_start = node.rhs;

            ast_display(ast, buffer, ast->extra.items[keys_start]);
            printf(": ");
            ast_display(ast, buffer, ast->extra.items[values_start]);

            for (uint32_t i = 1; i < len; i++) {
                printf(", ");
                ast_display(ast, buffer, ast->extra.items[keys_start + i]);
                printf(": ");
                ast_display(ast, buffer, ast->extra.items[values_start + i]);
            }
        }

        printf("}");
        break;

    case NODE_CALL:
        ast_display(ast, buffer, node.lhs);

        printf("(");

        if (node.rhs != INVALID_EXTRA_IDX) {
            uint32_t start = node.rhs + 1;
            uint32_t len = ast->extra.items[node.rhs];

            ast_display(ast, buffer, ast->extra.items[start]);

            for (uint32_t i = 1; i < len; i++) {
                printf(", ");
                ast_display(ast, buffer, ast->extra.items[start + i]);
            }
        }

        printf(")");
        break;

    case NODE_FUNCTION:
        printf("fn (");

        if (node.lhs != INVALID_EXTRA_IDX) {
            uint32_t start = node.lhs + 1;
            uint32_t len = ast->extra.items[node.lhs];

            ast_display(ast, buffer, ast->extra.items[start]);

            for (uint32_t i = 1; i < len; i++) {
                printf(", ");
                ast_display(ast, buffer, ast->extra.items[start + i]);
            }
        }

        printf(") ");

        ast_display(ast, buffer, node.rhs);

        break;

    case NODE_MEMBER:
        ast_display(ast, buffer, node.lhs);
        printf(".");
        ast_display(ast, buffer, node.rhs);
        break;

    case NODE_SUBSCRIPT:
        ast_display(ast, buffer, node.lhs);
        printf("[");
        ast_display(ast, buffer, node.rhs);
        printf("]");
        break;

    case NODE_WHILE:
        printf("while ");
        ast_display(ast, buffer, node.lhs);
        printf(" ");
        ast_display(ast, buffer, node.rhs);
        break;

    case NODE_IF: {
        AstNodeIdx true_case = ast->extra.items[node.rhs];
        AstNodeIdx false_case = ast->extra.items[node.rhs + 1];
        printf("if ");
        ast_display(ast, buffer, node.lhs);
        printf(" ");
        ast_display(ast, buffer, true_case);
        if (false_case != INVALID_NODE_IDX) {
            printf(" else ");
            ast_display(ast, buffer, false_case);
        }
        break;
    }

    default:
        assert(false && "UNREACHABLE");
        break;
    }
}

#include <assert.h>
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
                                AstNodePayload payload, uint32_t source_start) {
    if (parser->ast.nodes.len + 1 > parser->ast.nodes.capacity) {
        size_t new_cap =
            parser->ast.nodes.capacity ? parser->ast.nodes.capacity * 2 : 4;

        parser->ast.nodes.tags = realloc(
            parser->ast.nodes.tags, sizeof(*parser->ast.nodes.tags) * new_cap);

        parser->ast.nodes.payloads =
            realloc(parser->ast.nodes.payloads,
                    sizeof(*parser->ast.nodes.payloads) * new_cap);

        parser->ast.nodes.source_starts =
            realloc(parser->ast.nodes.source_starts,
                    sizeof(*parser->ast.nodes.source_starts) * new_cap);

        if (parser->ast.nodes.tags == NULL ||
            parser->ast.nodes.payloads == NULL ||
            parser->ast.nodes.source_starts == NULL) {
            fprintf(stderr, "error: out of memory\n");

            // We shouldn't return INVALID_NODE_IDX as being out of memory is
            // irrecoverible (not sure)
            exit(1);
        }

        parser->ast.nodes.capacity = new_cap;
    }

    parser->ast.nodes.tags[parser->ast.nodes.len] = tag;
    parser->ast.nodes.payloads[parser->ast.nodes.len] = payload;
    parser->ast.nodes.source_starts[parser->ast.nodes.len] = source_start;

    return parser->ast.nodes.len++;
}

static AstNodeIdx ast_parse_expr(AstParser *parser,
                                 OperatorPrecedence precedence);

static AstNodeIdx ast_parse_assign(AstParser *parser, AstNodeIdx target,
                                   AstNodeTag tag) {
    Token token = lexer_next(&parser->lexer);

    AstNodeIdx value = ast_parse_expr(parser, PR_ASSIGN);

    if (value == INVALID_NODE_IDX) {
        return INVALID_NODE_IDX;
    }

    return ast_push_node(parser, tag,
                         (AstNodePayload){
                             .lhs = target,
                             .rhs = value,
                         },
                         token.range.start);
}

static AstNodeIdx ast_parse_call(AstParser *parser, AstNodeIdx callee) {
    Token token = lexer_next(&parser->lexer);

    AstExtra arguments = {0};

    while (lexer_peek(&parser->lexer).tag != TOK_CPAREN) {
        AstNodeIdx argument = ast_parse_expr(parser, PR_LOWEST);

        if (argument == INVALID_NODE_IDX) {
            return INVALID_NODE_IDX;
        }

        if (lexer_peek(&parser->lexer).tag == TOK_COMMA) {
            lexer_next(&parser->lexer);
        }

        if (lexer_peek(&parser->lexer).tag == TOK_EOF) {
            SourceLocation call_location = source_location_of(
                parser->file_path, parser->lexer.buffer, token.range);

            diagnoser_error(call_location, "'(' did not get closed\n");

            return INVALID_NODE_IDX;
        }

        ARRAY_PUSH(&arguments, argument);
    }

    lexer_next(&parser->lexer);

    if (arguments.len == 0) {
        return ast_push_node(
            parser, NODE_CALL,
            (AstNodePayload){.lhs = callee, .rhs = INVALID_EXTRA_IDX},
            token.range.start);
    } else {
        uint32_t index = parser->ast.extra.len;

        ARRAY_PUSH(&parser->ast.extra, arguments.len);

        ARRAY_EXPAND(&parser->ast.extra, arguments.items, arguments.len);

        ARRAY_FREE(&arguments);

        return ast_push_node(parser, NODE_CALL,
                             (AstNodePayload){.lhs = callee, .rhs = index},
                             token.range.start);
    }
}

static AstNodeIdx ast_parse_binary_op(AstParser *parser, AstNodeIdx lhs,
                                      AstNodeTag tag,
                                      OperatorPrecedence precedence) {
    Token token = lexer_next(&parser->lexer);

    AstNodeIdx rhs = ast_parse_expr(parser, precedence);

    if (rhs == INVALID_NODE_IDX) {
        return INVALID_NODE_IDX;
    }

    return ast_push_node(parser, tag, (AstNodePayload){.lhs = lhs, .rhs = rhs},
                         token.range.start);
}

static AstNodeIdx ast_parse_binary_expr(AstParser *parser, AstNodeIdx lhs) {
    switch (lexer_peek(&parser->lexer).tag) {
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
                                           lexer_peek(&parser->lexer).range),
                        "unknown binary operator\n");

        return INVALID_NODE_IDX;
    }
}

static AstNodeIdx ast_parse_identifier(AstParser *parser) {
    Token token = lexer_next(&parser->lexer);

    return ast_push_node(
        parser, NODE_IDENTIFIER,
        (AstNodePayload){.lhs = token.range.start, .rhs = token.range.end},
        token.range.start);
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
                         .rhs = unescaped_string_end - unescaped_string_start},
        token.range.start);
}

static AstNodeIdx ast_parse_int(AstParser *parser) {
    Token token = lexer_next(&parser->lexer);

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

    return ast_push_node(parser, NODE_INT,
                         (AstNodePayload){
                             .lhs = v >> 32,
                             .rhs = v,
                         },
                         token.range.start);
}

static AstNodeIdx ast_parse_float(AstParser *parser) {
    Token token = lexer_next(&parser->lexer);

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

    return ast_push_node(parser, NODE_FLOAT,
                         (AstNodePayload){
                             .lhs = vi >> 32,
                             .rhs = vi,
                         },
                         token.range.start);
}

static AstNodeIdx ast_parse_neg(AstParser *parser) {
    Token token = lexer_next(&parser->lexer);

    AstNodeIdx value = ast_parse_expr(parser, PR_PREFIX);

    if (value == INVALID_NODE_IDX) {
        return INVALID_NODE_IDX;
    }

    return ast_push_node(parser, NODE_NEG, (AstNodePayload){.rhs = value},
                         token.range.start);
}

static AstNodeIdx ast_parse_parentheses_expr(AstParser *parser) {
    Token token = lexer_next(&parser->lexer);

    AstNodeIdx value = ast_parse_expr(parser, PR_LOWEST);

    if (value == INVALID_NODE_IDX) {
        return INVALID_NODE_IDX;
    }

    if (lexer_peek(&parser->lexer).tag != TOK_CPAREN) {
        SourceLocation parentheses_location = source_location_of(
            parser->file_path, parser->lexer.buffer, token.range);

        diagnoser_error(parentheses_location, "'(' did not get closed\n");

        return INVALID_NODE_IDX;
    }

    lexer_next(&parser->lexer);

    return value;
}

static AstNodeIdx ast_parse_unary_expr(AstParser *parser) {
    switch (lexer_peek(&parser->lexer).tag) {
    case TOK_IDENTIFIER:
        return ast_parse_identifier(parser);
    case TOK_STRING:
        return ast_parse_string(parser);
    case TOK_INT:
        return ast_parse_int(parser);
    case TOK_FLOAT:
        return ast_parse_float(parser);
    case TOK_MINUS:
        return ast_parse_neg(parser);
    case TOK_OPAREN:
        return ast_parse_parentheses_expr(parser);
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

static AstNodeIdx ast_parse_stmt(AstParser *parser);

static AstNodeIdx ast_parse_block(AstParser *parser) {
    Token token = lexer_next(&parser->lexer);

    AstExtra stmts = {0};

    while (lexer_peek(&parser->lexer).tag != TOK_CBRACE) {
        AstNodeIdx stmt = ast_parse_stmt(parser);

        if (stmt == INVALID_NODE_IDX) {
            return INVALID_NODE_IDX;
        }

        if (lexer_peek(&parser->lexer).tag == TOK_EOF) {
            SourceLocation block_location = source_location_of(
                parser->file_path, parser->lexer.buffer, token.range);

            diagnoser_error(block_location, "'{' did not get closed\n");

            return INVALID_NODE_IDX;
        }

        ARRAY_PUSH(&stmts, stmt);
    }

    lexer_next(&parser->lexer);

    uint32_t stmts_start = parser->ast.extra.len;

    ARRAY_EXPAND(&parser->ast.extra, stmts.items, stmts.len);

    AstNodeIdx block =
        ast_push_node(parser, NODE_BLOCK,
                      (AstNodePayload){.lhs = stmts_start, .rhs = stmts.len},
                      token.range.start);

    ARRAY_FREE(&stmts);

    return block;
}

static AstNodeIdx ast_parse_while_loop(AstParser *parser) {
    Token token = lexer_next(&parser->lexer);

    AstNodeIdx condition = ast_parse_expr(parser, PR_LOWEST);

    if (condition == INVALID_NODE_IDX) {
        return INVALID_NODE_IDX;
    }

    if (lexer_peek(&parser->lexer).tag != TOK_OBRACE) {
        SourceLocation block_location =
            source_location_of(parser->file_path, parser->lexer.buffer,
                               lexer_peek(&parser->lexer).range);

        diagnoser_error(block_location, "expected '{'\n");

        return INVALID_NODE_IDX;
    }

    AstNodeIdx block = ast_parse_block(parser);

    if (block == INVALID_NODE_IDX) {
        return INVALID_NODE_IDX;
    }

    return ast_push_node(parser, NODE_WHILE,
                         (AstNodePayload){
                             .lhs = condition,
                             .rhs = block,
                         },
                         token.range.start);
}

static AstNodeIdx ast_parse_conditional(AstParser *parser) {
    Token token = lexer_next(&parser->lexer);

    AstNodeIdx condition = ast_parse_expr(parser, PR_LOWEST);

    if (condition == INVALID_NODE_IDX) {
        return INVALID_NODE_IDX;
    }

    if (lexer_peek(&parser->lexer).tag != TOK_OBRACE) {
        SourceLocation block_location =
            source_location_of(parser->file_path, parser->lexer.buffer,
                               lexer_peek(&parser->lexer).range);

        diagnoser_error(block_location, "expected '{'\n");

        return INVALID_NODE_IDX;
    }

    AstNodeIdx true_case = ast_parse_block(parser);

    if (true_case == INVALID_NODE_IDX) {
        return INVALID_NODE_IDX;
    }

    AstNodeIdx false_case = INVALID_NODE_IDX;

    if (lexer_peek(&parser->lexer).tag == TOK_KEYWORD_ELSE) {
        lexer_next(&parser->lexer);

        switch (lexer_peek(&parser->lexer).tag) {
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
            SourceLocation block_location =
                source_location_of(parser->file_path, parser->lexer.buffer,
                                   lexer_peek(&parser->lexer).range);

            diagnoser_error(block_location, "expected '{' or 'if'\n");

            return INVALID_NODE_IDX;
        }
        }
    }

    uint32_t rhs = parser->ast.extra.len;

    ARRAY_PUSH(&parser->ast.extra, true_case);
    ARRAY_PUSH(&parser->ast.extra, false_case);

    return ast_push_node(parser, NODE_IF,
                         (AstNodePayload){.lhs = condition, .rhs = rhs},
                         token.range.start);
}

static AstNodeIdx ast_parse_return(AstParser *parser) {
    Token token = lexer_next(&parser->lexer);

    if (lexer_peek(&parser->lexer).tag == TOK_CBRACE) {
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
        return ast_push_node(parser, NODE_RETURN,
                             (AstNodePayload){.lhs = false}, token.range.start);
    }

    if (lexer_peek(&parser->lexer).tag == TOK_SEMICOLON) {
        // Maybe to skip other code:
        //
        // fn () {
        //     return;
        //
        //     println(...);
        // }
        lexer_next(&parser->lexer);

        return ast_push_node(parser, NODE_RETURN,
                             (AstNodePayload){.lhs = false}, token.range.start);
    }

    AstNodeIdx value = ast_parse_expr(parser, PR_LOWEST);

    if (value == INVALID_NODE_IDX) {
        return INVALID_NODE_IDX;
    }

    return ast_push_node(parser, NODE_RETURN,
                         (AstNodePayload){.lhs = true, .rhs = value},
                         token.range.start);
}

static AstNodeIdx ast_parse_stmt(AstParser *parser) {
    switch (lexer_peek(&parser->lexer).tag) {
    case TOK_OBRACE:
        return ast_parse_block(parser);
    case TOK_KEYWORD_WHILE:
        return ast_parse_while_loop(parser);
    case TOK_KEYWORD_IF:
        return ast_parse_conditional(parser);
    case TOK_KEYWORD_RETURN:
        return ast_parse_return(parser);
    case TOK_KEYWORD_BREAK:
        return ast_push_node(parser, NODE_BREAK, (AstNodePayload){},
                             lexer_next(&parser->lexer).range.start);
    case TOK_KEYWORD_CONTINUE:
        return ast_push_node(parser, NODE_CONTINUE, (AstNodePayload){},
                             lexer_next(&parser->lexer).range.start);
    default:
        return ast_parse_expr(parser, PR_LOWEST);
    }
}

AstNodeIdx ast_parse(AstParser *parser) {
    AstExtra stmts = {0};

    while (lexer_peek(&parser->lexer).tag != TOK_EOF) {
        AstNodeIdx stmt = ast_parse_stmt(parser);

        if (stmt == INVALID_NODE_IDX) {
            return INVALID_NODE_IDX;
        }

        ARRAY_PUSH(&stmts, stmt);
    }

    uint32_t stmts_start = parser->ast.extra.len;

    ARRAY_EXPAND(&parser->ast.extra, stmts.items, stmts.len);

    AstNodeIdx program = ast_push_node(
        parser, NODE_BLOCK,
        (AstNodePayload){.lhs = stmts_start, .rhs = stmts.len}, 0);

    ARRAY_FREE(&stmts);

    return program;
}

void ast_display(const Ast *ast, const char *buffer, AstNodeIdx node) {
    AstNodeIdx lhs = ast->nodes.payloads[node].lhs;
    AstNodeIdx rhs = ast->nodes.payloads[node].rhs;

    static int ast_display_depth = 0;

    switch (ast->nodes.tags[node]) {
    case NODE_BLOCK:
        printf("{\n");

        ast_display_depth += 1;

        for (uint32_t i = 0; i < rhs; i++) {
            printf("%*.s", ast_display_depth * 4, "");

            ast_display(ast, buffer, ast->extra.items[lhs + i]);

            printf("\n");
        }

        ast_display_depth -= 1;

        if (ast_display_depth > 0) {
            printf("%*.s", ast_display_depth * 4, "");
        }

        printf("}");

        break;

    case NODE_INT:
        printf("%lu", ((uint64_t)lhs << 32) | rhs);
        break;

    case NODE_FLOAT: {
        uint64_t vi = ((uint64_t)lhs << 32) | rhs;
        double v;
        memcpy(&v, &vi, sizeof(double));
        printf("%lf", v);
        break;
    }

    case NODE_IDENTIFIER:
        printf("%.*s", (int)(rhs - lhs), buffer + lhs);
        break;

    case NODE_STRING:
        printf("\"%.*s\"", (int)rhs, ast->strings.items + lhs);
        break;

    case NODE_ASSIGN:
        ast_display(ast, buffer, lhs);
        printf(" = ");
        ast_display(ast, buffer, rhs);
        break;

    case NODE_ASSIGN_ADD:
        ast_display(ast, buffer, lhs);
        printf(" += ");
        ast_display(ast, buffer, rhs);
        break;

    case NODE_ASSIGN_SUB:
        ast_display(ast, buffer, lhs);
        printf(" -= ");
        ast_display(ast, buffer, rhs);
        break;

    case NODE_ASSIGN_DIV:
        ast_display(ast, buffer, lhs);
        printf(" /= ");
        ast_display(ast, buffer, rhs);
        break;

    case NODE_ASSIGN_MUL:
        ast_display(ast, buffer, lhs);
        printf(" *= ");
        ast_display(ast, buffer, rhs);
        break;

    case NODE_ASSIGN_POW:
        ast_display(ast, buffer, lhs);
        printf(" **= ");
        ast_display(ast, buffer, rhs);
        break;

    case NODE_ASSIGN_MOD:
        ast_display(ast, buffer, lhs);
        printf(" %%= ");
        ast_display(ast, buffer, rhs);
        break;

    case NODE_NEG:
        printf(" -(");
        ast_display(ast, buffer, rhs);
        printf(")");
        break;

    case NODE_ADD:
        printf("(");
        ast_display(ast, buffer, lhs);
        printf(" + ");
        ast_display(ast, buffer, rhs);
        printf(")");
        break;

    case NODE_SUB:
        printf("(");
        ast_display(ast, buffer, lhs);
        printf(" - ");
        ast_display(ast, buffer, rhs);
        printf(")");
        break;

    case NODE_DIV:
        printf("(");
        ast_display(ast, buffer, lhs);
        printf(" / ");
        ast_display(ast, buffer, rhs);
        printf(")");
        break;

    case NODE_MUL:
        printf("(");
        ast_display(ast, buffer, lhs);
        printf(" * ");
        ast_display(ast, buffer, rhs);
        printf(")");
        break;

    case NODE_POW:
        printf("(");
        ast_display(ast, buffer, lhs);
        printf(" ** ");
        ast_display(ast, buffer, rhs);
        printf(")");
        break;

    case NODE_MOD:
        printf("(");
        ast_display(ast, buffer, lhs);
        printf(" %% ");
        ast_display(ast, buffer, rhs);
        printf(")");
        break;

    case NODE_EQL:
        printf("(");
        ast_display(ast, buffer, lhs);
        printf(")");
        printf(" == ");
        printf("(");
        ast_display(ast, buffer, rhs);
        printf(")");
        break;

    case NODE_NEQ:
        printf("(");
        ast_display(ast, buffer, lhs);
        printf(")");
        printf(" != ");
        printf("(");
        ast_display(ast, buffer, rhs);
        printf(")");
        break;

    case NODE_LT:
        printf("(");
        ast_display(ast, buffer, lhs);
        printf(")");
        printf(" < ");
        printf("(");
        ast_display(ast, buffer, rhs);
        printf(")");
        break;

    case NODE_LTE:
        printf("(");
        ast_display(ast, buffer, lhs);
        printf(")");
        printf(" <= ");
        printf("(");
        ast_display(ast, buffer, rhs);
        printf(")");
        break;

    case NODE_GT:
        printf("(");
        ast_display(ast, buffer, lhs);
        printf(")");
        printf(" > ");
        printf("(");
        ast_display(ast, buffer, rhs);
        printf(")");
        break;

    case NODE_GTE:
        printf("(");
        ast_display(ast, buffer, lhs);
        printf(")");
        printf(" >= ");
        printf("(");
        ast_display(ast, buffer, rhs);
        printf(")");
        break;

    case NODE_RETURN:
        printf("return");

        if (lhs) {
            printf(" ");
            ast_display(ast, buffer, rhs);
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

    case NODE_CALL:
        ast_display(ast, buffer, lhs);

        printf("(");

        if (rhs != INVALID_EXTRA_IDX) {
            uint32_t start = rhs + 1;
            uint32_t len = ast->extra.items[rhs];

            ast_display(ast, buffer, ast->extra.items[start]);

            for (uint32_t i = 1; i < len; i++) {
                printf(", ");
                ast_display(ast, buffer, ast->extra.items[start + i]);
            }
        }

        printf(")");
        break;

    case NODE_WHILE:
        printf("while ");
        ast_display(ast, buffer, lhs);
        printf(" ");
        ast_display(ast, buffer, rhs);
        break;

    case NODE_IF: {
        AstNodeIdx true_case = ast->extra.items[rhs];
        AstNodeIdx false_case = ast->extra.items[rhs + 1];
        printf("if ");
        ast_display(ast, buffer, lhs);
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

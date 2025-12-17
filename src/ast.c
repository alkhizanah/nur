#include "ast.h"
#include "array.h"
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
                        "unknown operator\n");

        exit(1);
    }
}

static AstNodeIdx ast_parse_identifier(AstParser *parser) {
    Token identifier = lexer_next(&parser->lexer);

    return ast_push_node(parser, NODE_IDENTIFIER,
                         (AstNodePayload){.lhs = identifier.range.start,
                                          .rhs = identifier.range.end});
}

static AstNodeIdx ast_parse_unary_expr(AstParser *parser) {
    switch (lexer_peek(&parser->lexer).tag) {
    case TOK_IDENTIFIER:
        return ast_parse_identifier(parser);
    default:
        diagnoser_error(source_location_of(parser->file_path,
                                           parser->lexer.buffer,
                                           lexer_peek(&parser->lexer).range),
                        "unknown expression\n");

        exit(1);
    }
}

static AstNodeIdx ast_parse_expr(AstParser *parser,
                                 OperatorPrecedence precedence) {
    AstNodeIdx lhs = ast_parse_unary_expr(parser);

    while (operator_precedence_of(lexer_peek(&parser->lexer).tag) >
           precedence) {
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

void ast_parse(AstParser *parser) {
    // We intentionally make our own AstExtra so it won't be modified by other
    // "ast_parse_*" functions
    AstExtra stmts = {0};

    while (lexer_peek(&parser->lexer).tag != TOK_EOF) {
        AstNodeIdx stmt = ast_parse_stmt(parser);

        ARRAY_PUSH(&stmts, stmt);
    }

    uint32_t stmts_index = parser->ast.extra.len;

    ARRAY_EXPAND(&parser->ast.extra, stmts.items, stmts.len);

    ast_push_node(parser, NODE_BLOCK,
                  (AstNodePayload){.lhs = stmts_index, .rhs = stmts.len});

    ARRAY_FREE(&stmts);
}

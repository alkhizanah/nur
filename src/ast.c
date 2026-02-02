#include <assert.h>
#include <ctype.h>

#include "array.h"
#include "ast.h"

AstNodeIdx ast_push(Ast *ast, AstNodeTag tag, AstNodeIdx lhs, AstNodeIdx rhs,
                    uint32_t source) {
    AstNode node = {
        .lhs = lhs,
        .rhs = rhs,
        .tag = tag,
    };

    AstNodeIdx i = ast->nodes.count;

    ARRAY_PUSH(&ast->nodes, node);

    ast->nodes.sources = realloc(
        ast->nodes.sources, ast->nodes.capacity * sizeof(*ast->nodes.sources));

    if (ast->nodes.sources == NULL) {
        fprintf(stderr, "error: out of memory\n");

        exit(1);
    }

    ast->nodes.sources[i] = source;

    return i;
}

static void print_string_escaped(const char *s, size_t count) {
    for (size_t i = 0; i < count; i++) {
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
        uint32_t count = node.rhs;

        printf("[");

        ast_display(ast, buffer, ast->extra.items[start]);

        for (uint32_t i = 1; i < count; i++) {
            printf(", ");
            ast_display(ast, buffer, ast->extra.items[start + i]);
        }

        printf("]");
        break;
    }

    case NODE_MAP:
        printf("{");

        if (node.lhs != INVALID_EXTRA_IDX) {
            uint32_t count = ast->extra.items[node.lhs];

            uint32_t keys_start = node.lhs + 1;
            uint32_t values_start = node.rhs;

            ast_display(ast, buffer, ast->extra.items[keys_start]);
            printf(": ");
            ast_display(ast, buffer, ast->extra.items[values_start]);

            for (uint32_t i = 1; i < count; i++) {
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
            uint32_t count = ast->extra.items[node.rhs];

            ast_display(ast, buffer, ast->extra.items[start]);

            for (uint32_t i = 1; i < count; i++) {
                printf(", ");
                ast_display(ast, buffer, ast->extra.items[start + i]);
            }
        }

        printf(")");

        break;

    case NODE_FUNCTION:
        printf("fn ");

        if (node.lhs != INVALID_EXTRA_IDX) {
            uint32_t start = node.lhs + 1;
            uint32_t count = ast->extra.items[node.lhs];

            ast_display(ast, buffer, ast->extra.items[start]);

            for (uint32_t i = 1; i < count; i++) {
                printf(", ");
                ast_display(ast, buffer, ast->extra.items[start + i]);
            }
        }

        printf(" ");

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

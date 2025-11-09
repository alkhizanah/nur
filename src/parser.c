#include <malloc.h>
#include <stdlib.h>

#include "lexer.h"
#include "parser.h"

static void parser_push_node(Parser *parser, AstNodeTag tag,
                             AstNodePayload payload) {
    if (parser->ast.nodes.capacity == 0) {
        parser->ast.nodes.capacity = 4;

        parser->ast.nodes.tags =
            malloc(sizeof(tag) * parser->ast.nodes.capacity);

        if (parser->ast.nodes.tags == NULL) {
            fprintf(stderr, "error: out of memory\n");

            exit(1);
        }

        parser->ast.nodes.payloads =
            malloc(sizeof(payload) * parser->ast.nodes.capacity);

        if (parser->ast.nodes.payloads == NULL) {
            fprintf(stderr, "error: out of memory\n");

            exit(1);
        }
    }

    parser->ast.nodes.len++;

    if (parser->ast.nodes.capacity < parser->ast.nodes.len) {
        parser->ast.nodes.capacity *= 2;

        parser->ast.nodes.tags = realloc(
            parser->ast.nodes.tags, sizeof(tag) * parser->ast.nodes.capacity);

        if (parser->ast.nodes.tags == NULL) {
            fprintf(stderr, "error: out of memory\n");

            exit(1);
        }

        parser->ast.nodes.payloads =
            realloc(parser->ast.nodes.payloads,
                    sizeof(payload) * parser->ast.nodes.capacity);

        if (parser->ast.nodes.payloads == NULL) {
            fprintf(stderr, "error: out of memory\n");

            exit(1);
        }
    }

    parser->ast.nodes.tags[parser->ast.nodes.len - 1] = tag;
    parser->ast.nodes.payloads[parser->ast.nodes.len - 1] = payload;
}

void parser_parse(Parser *parser) {
    (void)parser;
    printf("todo: actually parse\n");
}

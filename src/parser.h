#pragma once

#include "ast.h"
#include "lexer.h"

typedef struct {
    Ast ast;
    const char *file_path;
    Lexer lexer;
    Token next_token;
    Token current_token;
} AstParser;

AstNodeIdx parse(AstParser *);

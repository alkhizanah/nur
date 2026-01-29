#pragma once

#include <limits.h>
#include <stdbool.h>
#include <stdint.h>

#include "lexer.h"

typedef enum : uint8_t {
    // Payload: lhs is the Range.start and rhs is the Range.end
    NODE_IDENTIFIER,
    // Payload: lhs is an index to a character list in strings.items, and rhs is
    // the amount of characters
    NODE_STRING,
    // Payload: lhs is the high bits (i >> 32) and rhs is the low bits
    // ((uint32_t)i)
    NODE_INT,
    // Payload: lhs is the high bits (bitcasted_f >> 32) and rhs is the low bits
    // ((uint32_t)bitcasted_f)
    NODE_FLOAT,
    // Payload: lhs is an index to a value list in extra.items, rhs is the
    // amount of values
    NODE_ARRAY,
    // Payload: lhs is an index to key-value pair list in extra.items in, rhs is
    // the amount of pairs
    NODE_MAP,
    // Payload: (op) rhs, no need to explain this
    NODE_NEG,
    NODE_NOT,
    // Payload: lhs (op) rhs, no need to explain this
    NODE_ADD,
    NODE_SUB,
    NODE_MUL,
    NODE_DIV,
    NODE_POW,
    NODE_MOD,
    NODE_EQL,
    NODE_NEQ,
    NODE_LT,
    NODE_GT,
    NODE_LTE,
    NODE_GTE,
    // Payload of ASSIGN_*: lhs is an index to a target node, rhs is an index to
    // a value
    NODE_ASSIGN,
    NODE_ASSIGN_ADD,
    NODE_ASSIGN_SUB,
    NODE_ASSIGN_MUL,
    NODE_ASSIGN_DIV,
    NODE_ASSIGN_POW,
    NODE_ASSIGN_MOD,
    // Payload: lhs is an index to an AstNodeIdx list in extra.items, and rhs is
    // the amount of nodes
    NODE_BLOCK,
    // Payload: lhs is an index to a parameter list in extra.items, first
    // element is the amount of parameters then after are indices to identifiers
    // representing the name of the parameters, if lhs is INVALID_EXTRA_IDX then
    // the function doesn't accept any parameter, and rhs is an index to a block
    // that is the body of the function
    NODE_FUNCTION,
    // Payload: rhs is the value being returend
    NODE_RETURN,
    // No payload is needed for NODE_BREAK and NODE_CONTINUE
    NODE_BREAK,
    NODE_CONTINUE,
    // Payload: lhs is the callee and rhs is an index to AstExtra, first element
    // at the index is the amount of arguments, and the elements after are an
    // indices to the arguments, if rhs is INVALID_EXTRA_IDX then there are no
    // arguments passed to the function
    NODE_CALL,
    // Payload: lhs is the condition of the while loop, and rhs is the block
    NODE_WHILE,
    // Payload: lhs is the condition and rhs is an index to 2 elements in
    // AstExtra where first element is the true case block and the second is the
    // false case block, if the second element is INVALID_NODE_IDX then the
    // false case block is empty
    NODE_IF,
    // Payload: lhs is the target and rhs is an identifier node
    NODE_MEMBER,
    // Payload: lhs is the target and rhs is an arbitrary expression
    NODE_SUBSCRIPT,
} AstNodeTag;

typedef uint32_t AstNodeIdx;

#define INVALID_NODE_IDX UINT32_MAX
#define INVALID_EXTRA_IDX UINT32_MAX

typedef struct {
    AstNodeIdx lhs;
    AstNodeIdx rhs;
    AstNodeTag tag;
} AstNode;

typedef struct {
    AstNode *items;
    uint32_t *sources;
    size_t count;
    size_t capacity;
} AstNodes;

typedef struct {
    char *items;
    size_t count;
    size_t capacity;
} AstStrings;

typedef struct {
    uint32_t *items;
    size_t count;
    size_t capacity;
} AstExtra;

typedef struct {
    AstNodes nodes;
    AstStrings strings;
    AstExtra extra;
} Ast;

AstNodeIdx ast_push(Ast *, AstNodeTag tag, AstNodeIdx lhs, AstNodeIdx rhs,
                    uint32_t source);

void ast_display(const Ast *, const char *buffer, AstNodeIdx node);

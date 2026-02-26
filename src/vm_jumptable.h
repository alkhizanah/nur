#pragma once

#undef vmdispatch
#undef vmcase
#undef vmbreak

#define vmdispatch() goto *dispatch_table[op];

#define vmcase(op) L_##op:

#define vmbreak()                                                              \
    vmfetch();                                                                 \
    vmdispatch()

static void *dispatch_table[] = {
    [OP_POP] = &&L_OP_POP,
    [OP_DUP] = &&L_OP_DUP,
    [OP_SWP] = &&L_OP_SWP,
    [OP_PUSH_NULL] = &&L_OP_PUSH_NULL,
    [OP_PUSH_TRUE] = &&L_OP_PUSH_TRUE,
    [OP_PUSH_FALSE] = &&L_OP_PUSH_FALSE,
    [OP_PUSH_CONST] = &&L_OP_PUSH_CONST,
    [OP_GET_LOCAL] = &&L_OP_GET_LOCAL,
    [OP_SET_LOCAL] = &&L_OP_SET_LOCAL,
    [OP_GET_UPVALUE] = &&L_OP_GET_UPVALUE,
    [OP_SET_UPVALUE] = &&L_OP_SET_UPVALUE,
    [OP_CLOSE_UPVALUE] = &&L_OP_CLOSE_UPVALUE,
    [OP_GET_GLOBAL] = &&L_OP_GET_GLOBAL,
    [OP_SET_GLOBAL] = &&L_OP_SET_GLOBAL,
    [OP_GET_SUBSCRIPT] = &&L_OP_GET_SUBSCRIPT,
    [OP_SET_SUBSCRIPT] = &&L_OP_SET_SUBSCRIPT,
    [OP_MAKE_ARRAY] = &&L_OP_MAKE_ARRAY,
    [OP_MAKE_MAP] = &&L_OP_MAKE_MAP,
    [OP_MAKE_CLOSURE] = &&L_OP_MAKE_CLOSURE,
    [OP_MAKE_SLICE] = &&L_OP_MAKE_SLICE,             // a[s:e]
    [OP_MAKE_SLICE_ABOVE] = &&L_OP_MAKE_SLICE_ABOVE, // a[s:]
    [OP_MAKE_SLICE_UNDER] = &&L_OP_MAKE_SLICE_UNDER, // a[:e]
    [OP_COPY_BY_SLICING] = &&L_OP_COPY_BY_SLICING,   // a[:]
    [OP_NEG] = &&L_OP_NEG,
    [OP_NOT] = &&L_OP_NOT,
    [OP_ADD] = &&L_OP_ADD,
    [OP_SUB] = &&L_OP_SUB,
    [OP_MUL] = &&L_OP_MUL,
    [OP_DIV] = &&L_OP_DIV,
    [OP_POW] = &&L_OP_POW,
    [OP_MOD] = &&L_OP_MOD,
    [OP_EQL] = &&L_OP_EQL,
    [OP_NEQ] = &&L_OP_NEQ,
    [OP_LT] = &&L_OP_LT,
    [OP_GT] = &&L_OP_GT,
    [OP_LTE] = &&L_OP_LTE,
    [OP_GTE] = &&L_OP_GTE,
    [OP_CALL] = &&L_OP_CALL,
    [OP_POP_JUMP_IF_FALSE] = &&L_OP_POP_JUMP_IF_FALSE,
    [OP_JUMP_IF_FALSE] = &&L_OP_JUMP_IF_FALSE,
    [OP_JUMP] = &&L_OP_JUMP,
    [OP_LOOP] = &&L_OP_LOOP,
    [OP_RETURN] = &&L_OP_RETURN,
};

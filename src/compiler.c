#include <assert.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "array.h"
#include "ast.h"
#include "compiler.h"
#include "parser.h"
#include "source_location.h"
#include "vm.h"

[[gnu::format(printf, 3, 4)]]
static void compiler_error(const Compiler *compiler, uint32_t start,
                           const char *format, ...) {
    va_list args;

    va_start(args, format);

    SourceLocation loc =
        source_location_of(compiler->file_path, compiler->file_buffer, start);

    fprintf(stderr, "%s:%u:%u: error: ", loc.file_path, loc.line, loc.column);
    vfprintf(stderr, format, args);

    va_end(args);
}

bool compile_block(Compiler *compiler, AstNode block) {
    uint32_t prev_locals_count = compiler->locals_count;

    for (uint32_t i = 0; i < block.rhs; i++) {
        if (!compile_stmt(compiler, compiler->ast.extra.items[block.lhs + i])) {
            return false;
        }
    }

    uint32_t amount_of_new_locals = compiler->locals_count - prev_locals_count;

    for (uint32_t i = 0; i < amount_of_new_locals; i++) {
        Local local = compiler->locals[i];

        if (local.is_captured) {
            chunk_add_byte(compiler->chunk, OP_CLOSE_UPVALUE, 0);
        } else {
            chunk_add_byte(compiler->chunk, OP_POP, 0);
        }
    }

    compiler->locals_count = prev_locals_count;

    return true;
}

static bool compile_return(Compiler *compiler, AstNode node, uint32_t source) {
    if (!compile_expr(compiler, node.rhs)) {
        return false;
    }

    chunk_add_byte(compiler->chunk, OP_RETURN, source);

    return true;
}

static void compiler_emit_short(Compiler *compiler, uint16_t c,
                                uint32_t source) {
    chunk_add_byte(compiler->chunk, c >> 8, source);
    chunk_add_byte(compiler->chunk, c, source);
}

static void compiler_emit_word(Compiler *compiler, uint32_t c,
                               uint32_t source) {
    chunk_add_byte(compiler->chunk, c >> 24, source);
    chunk_add_byte(compiler->chunk, c >> 16, source);
    chunk_add_byte(compiler->chunk, c >> 8, source);
    chunk_add_byte(compiler->chunk, c, source);
}

static void compiler_emit_constant(Compiler *compiler, Value value,
                                   uint32_t source) {
    uint16_t c = chunk_add_constant(compiler->chunk, value);
    compiler_emit_short(compiler, c, source);
}

static bool compiler_find_local(Compiler *compiler, const char *name,
                                size_t name_len, uint32_t *index) {
    for (*index = 0; *index < compiler->locals_count; (*index)++) {
        Local local = compiler->locals[*index];

        if (local.name_len != name_len) {
            continue;
        }

        if (local.name == name) {
            return true;
        }

        if (strncmp(local.name, name, name_len) == 0) {
            return true;
        }
    }

    return false;
}

static bool compiler_add_local(Compiler *compiler, const char *name,
                               uint32_t name_len) {
    Local local = {
        .name = name,
        .name_len = name_len,
    };

    if (compiler->locals_count == UINT8_MAX) {
        return false;
    }

    compiler->locals[compiler->locals_count++] = local;

    return true;
}

static uint32_t compiler_add_upvalue(Compiler *compiler, uint32_t index,
                                     bool is_local) {
    Upvalue upvalue = {
        .index = index,
        .is_local = is_local,
    };

    if (compiler->upvalues_count == UINT8_MAX) {
        printf("error: too much upvalues");
        exit(1);
    }

    for (uint8_t i = 0; i < compiler->upvalues_count; i++) {
        Upvalue upvalue = compiler->upvalues[i];

        if (upvalue.index == index && upvalue.is_local == is_local) {
            return i;
        }
    }

    compiler->upvalues[compiler->upvalues_count++] = upvalue;

    return compiler->upvalues_count - 1;
}

static bool compiler_find_upvalue(Compiler *compiler, const char *name,
                                  size_t name_len, uint32_t *upvalue_index) {
    if (compiler->parent == NULL) {
        return false;
    }

    uint32_t index;

    if (compiler_find_local(compiler->parent, name, name_len, &index)) {
        compiler->parent->locals[index].is_captured = true;

        *upvalue_index = compiler_add_upvalue(compiler, index, true);

        return true;
    }

    if (compiler_find_upvalue(compiler->parent, name, name_len, &index)) {

        *upvalue_index = compiler_add_upvalue(compiler, index, false);

        return true;
    }

    return false;
}

static bool compile_identifier(Compiler *compiler, AstNode node,
                               uint32_t source) {
    const char *name = compiler->file_buffer + node.lhs;
    size_t name_len = node.rhs - node.lhs;

    if (name_len == 4 && name[0] == 'n' && name[1] == 'u' && name[2] == 'l' &&
        name[3] == 'l') {
        chunk_add_byte(compiler->chunk, OP_PUSH_NULL, source);
    } else if (name_len == 4 && name[0] == 't' && name[1] == 'r' &&
               name[2] == 'u' && name[3] == 'e') {
        chunk_add_byte(compiler->chunk, OP_PUSH_TRUE, source);
    } else if (name_len == 5 && name[0] == 'f' && name[1] == 'a' &&
               name[2] == 'l' && name[3] == 's' && name[4] == 'e') {
        chunk_add_byte(compiler->chunk, OP_PUSH_FALSE, source);
    } else {
        uint32_t index;

        if (compiler_find_local(compiler, name, name_len, &index)) {
            chunk_add_byte(compiler->chunk, OP_GET_LOCAL, source);
            chunk_add_byte(compiler->chunk, index, source);
        } else if (compiler_find_upvalue(compiler, name, name_len, &index)) {
            chunk_add_byte(compiler->chunk, OP_GET_UPVALUE, source);
            chunk_add_byte(compiler->chunk, index, source);
        } else {
            ObjString *key = vm_copy_string(compiler->vm, name, name_len);
            chunk_add_byte(compiler->chunk, OP_GET_GLOBAL, source);
            compiler_emit_constant(compiler, OBJ_VAL(key), source);
        }
    }

    return true;
}

static void compile_string(Compiler *compiler, AstNode node, uint32_t source) {
    const char *sv = compiler->ast.strings.items + node.lhs;
    size_t len = node.rhs;
    ObjString *s = vm_copy_string(compiler->vm, sv, len);
    chunk_add_byte(compiler->chunk, OP_PUSH_CONST, source);
    compiler_emit_constant(compiler, OBJ_VAL(s), source);
}

static void compile_int(Compiler *compiler, AstNode node, uint32_t source) {
    uint64_t v = (uint64_t)node.lhs << 32 | node.rhs;
    chunk_add_byte(compiler->chunk, OP_PUSH_CONST, source);
    compiler_emit_constant(compiler, INT_VAL(v), source);
}

static void compile_float(Compiler *compiler, AstNode node, uint32_t source) {
    uint64_t v = (uint64_t)node.lhs << 32 | node.rhs;
    double f;
    memcpy(&f, &v, sizeof(double));
    chunk_add_byte(compiler->chunk, OP_PUSH_CONST, source);
    compiler_emit_constant(compiler, FLT_VAL(f), source);
}

static ObjMap dummy_map;

static bool compile_function(Compiler *compiler, AstNode node,
                             uint32_t source) {
    Compiler fc = {
        .parent = compiler,
        .file_buffer = compiler->file_buffer,
        .file_path = compiler->file_path,
        .ast = compiler->ast,
        .vm = compiler->vm,
        .locals_count = 0,
        .upvalues_count = 0,
    };

    if (node.lhs != INVALID_EXTRA_IDX) {
        uint32_t arity = compiler->ast.extra.items[node.lhs];

        if (arity > UINT8_MAX) {
            compiler_error(compiler, source,
                           "accepting %d paramters exceeds the limit of %d\n",
                           (int)arity, UINT8_MAX);

            return false;
        }

        for (uint32_t i = 0; i < arity; i++) {
            AstNode parameter =
                compiler->ast.nodes
                    .items[compiler->ast.extra.items[node.lhs + 1 + i]];

            compiler_add_local(&fc, fc.file_buffer + parameter.lhs,
                               parameter.rhs - parameter.lhs);
        }
    }

    CallFrame *frame = &fc.vm->frames[fc.vm->frame_count++];

    ObjFunction *fn = vm_new_function(fc.vm, &dummy_map,
                                      (Chunk){
                                          .file_path = fc.file_path,
                                          .file_content = fc.file_buffer,
                                      },
                                      fc.locals_count, 0);

    frame->closure = vm_new_closure(fc.vm, fn);

    frame->slots = compiler->vm->stack;

    fc.chunk = &frame->closure->fn->chunk;

    if (!compile_stmt(&fc, node.rhs)) {
        return false;
    }

    chunk_add_byte(fc.chunk, OP_PUSH_NULL, 0);
    chunk_add_byte(fc.chunk, OP_RETURN, 0);

    fc.vm->frame_count--;

    fn->upvalues_count = fc.upvalues_count;

    chunk_add_byte(compiler->chunk, OP_MAKE_CLOSURE, source);
    compiler_emit_constant(compiler, OBJ_VAL(frame->closure->fn), source);

    for (uint8_t i = 0; i < fc.upvalues_count; i++) {
        chunk_add_byte(compiler->chunk, fc.upvalues[i].is_local, source);
        chunk_add_byte(compiler->chunk, fc.upvalues[i].index, source);
    }

    return true;
};

static bool compile_array(Compiler *compiler, AstNode node, uint32_t source) {
    for (uint32_t i = 0; i < node.rhs; i++) {
        if (!compile_expr(compiler, compiler->ast.extra.items[node.lhs + i])) {
            return false;
        }
    }

    chunk_add_byte(compiler->chunk, OP_MAKE_ARRAY, source);
    compiler_emit_word(compiler, node.rhs, source);

    return true;
}

static bool compile_map(Compiler *compiler, AstNode node, uint32_t source) {
    for (uint32_t i = 0; i < node.rhs * 2; i++) {
        if (!compile_expr(compiler, compiler->ast.extra.items[node.lhs + i])) {
            return false;
        }
    }

    chunk_add_byte(compiler->chunk, OP_MAKE_MAP, source);
    compiler_emit_word(compiler, node.rhs, source);

    return true;
}

static bool compile_subscript(Compiler *compiler, AstNode node,
                              uint32_t source) {
    if (!compile_expr(compiler, node.rhs)) {
        return false;
    }

    if (!compile_expr(compiler, node.lhs)) {
        return false;
    }

    chunk_add_byte(compiler->chunk, OP_GET_SUBSCRIPT, source);

    return true;
}

static bool compile_member(Compiler *compiler, AstNode node, uint32_t source) {

    AstNode identifier = compiler->ast.nodes.items[node.rhs];

    const char *key = compiler->file_buffer + identifier.lhs;
    uint32_t key_len = identifier.rhs - identifier.lhs;

    chunk_add_byte(compiler->chunk, OP_PUSH_CONST, source);

    compiler_emit_constant(
        compiler, OBJ_VAL(vm_copy_string(compiler->vm, key, key_len)), source);

    if (!compile_expr(compiler, node.lhs)) {
        return false;
    }

    chunk_add_byte(compiler->chunk, OP_GET_SUBSCRIPT, source);

    return true;
}

static bool compile_assign(Compiler *compiler, AstNode node, uint32_t source,
                           bool has_op, OpCode op) {
    if (!compile_expr(compiler, node.rhs)) {
        return false;
    }

    AstNode target = compiler->ast.nodes.items[node.lhs];

    if (target.tag == NODE_IDENTIFIER) {
        const char *name = compiler->file_buffer + target.lhs;
        size_t name_len = target.rhs - target.lhs;

        uint32_t index;

        if (compiler_find_local(compiler, name, name_len, &index)) {
            if (has_op) {
                chunk_add_byte(compiler->chunk, OP_GET_LOCAL, source);
                chunk_add_byte(compiler->chunk, index, source);

                chunk_add_byte(compiler->chunk, OP_SWP, source);

                chunk_add_byte(compiler->chunk, op, source);
            }

            chunk_add_byte(compiler->chunk, OP_SET_LOCAL, source);
            chunk_add_byte(compiler->chunk, index, source);
        } else if (compiler_find_upvalue(compiler, name, name_len, &index)) {
            if (has_op) {
                chunk_add_byte(compiler->chunk, OP_GET_UPVALUE, source);
                chunk_add_byte(compiler->chunk, index, source);

                chunk_add_byte(compiler->chunk, OP_SWP, source);

                chunk_add_byte(compiler->chunk, op, source);
            }

            chunk_add_byte(compiler->chunk, OP_SET_UPVALUE, source);
            chunk_add_byte(compiler->chunk, index, source);
        } else if (has_op) {
            ObjString *key = vm_copy_string(compiler->vm, name, name_len);

            uint16_t c = chunk_add_constant(compiler->chunk, OBJ_VAL(key));

            chunk_add_byte(compiler->chunk, OP_GET_GLOBAL, source);
            compiler_emit_short(compiler, c, source);

            chunk_add_byte(compiler->chunk, OP_SWP, source);

            chunk_add_byte(compiler->chunk, op, source);

            chunk_add_byte(compiler->chunk, OP_SET_GLOBAL, source);
            compiler_emit_short(compiler, c, source);
        } else if (compiler->parent != NULL) {
            chunk_add_byte(compiler->chunk, OP_DUP, source);

            if (!compiler_add_local(compiler, name, name_len)) {
                compiler_error(
                    compiler, source,
                    "using %d local variables exceeds the limit of %d\n",
                    (int)compiler->locals_count + 1, UINT8_MAX);

                return false;
            }
        } else {
            ObjString *key = vm_copy_string(compiler->vm, name, name_len);

            chunk_add_byte(compiler->chunk, OP_SET_GLOBAL, source);
            compiler_emit_constant(compiler, OBJ_VAL(key), source);
        }
    } else if (target.tag == NODE_SUBSCRIPT) {
        if (has_op) {
            if (!compile_expr(compiler, target.rhs)) {
                return false;
            }

            if (!compile_expr(compiler, target.lhs)) {
                return false;
            }

            chunk_add_byte(compiler->chunk, OP_GET_SUBSCRIPT, source);

            chunk_add_byte(compiler->chunk, OP_SWP, source);

            chunk_add_byte(compiler->chunk, op, source);
        }

        if (!compile_expr(compiler, target.rhs)) {
            return false;
        }

        if (!compile_expr(compiler, target.lhs)) {
            return false;
        }

        chunk_add_byte(compiler->chunk, OP_SET_SUBSCRIPT, source);

    } else if (target.tag == NODE_MEMBER) {
        AstNode identifier = compiler->ast.nodes.items[target.rhs];

        const char *key = compiler->file_buffer + identifier.lhs;
        uint32_t key_len = identifier.rhs - identifier.lhs;

        if (has_op) {
            chunk_add_byte(compiler->chunk, OP_PUSH_CONST, source);

            compiler_emit_constant(
                compiler, OBJ_VAL(vm_copy_string(compiler->vm, key, key_len)),
                source);

            if (!compile_expr(compiler, target.lhs)) {
                return false;
            }

            chunk_add_byte(compiler->chunk, OP_GET_SUBSCRIPT, source);

            chunk_add_byte(compiler->chunk, OP_SWP, source);

            chunk_add_byte(compiler->chunk, op, source);
        }

        chunk_add_byte(compiler->chunk, OP_PUSH_CONST, source);

        compiler_emit_constant(
            compiler, OBJ_VAL(vm_copy_string(compiler->vm, key, key_len)),
            source);

        if (!compile_expr(compiler, target.lhs)) {
            return false;
        }

        chunk_add_byte(compiler->chunk, OP_SET_SUBSCRIPT, source);
    } else {
        compiler_error(
            compiler, source,
            "target of an assignment can either be an identifier or "
            "a subscript (i.e `a[b]`) or a member access (i.e `a.b`), "
            "other things are not allowed to be assigned to");

        return false;
    }

    return true;
}

static uint32_t compiler_emit_jump(Compiler *compiler, OpCode opcode,
                                   uint32_t source) {

    chunk_add_byte(compiler->chunk, opcode, source);

    chunk_add_byte(compiler->chunk, 0xff, source);
    chunk_add_byte(compiler->chunk, 0xff, source);

    return compiler->chunk->count - 2;
}

static void compiler_patch_jump(Compiler *compiler, uint32_t offset) {
    uint32_t jump = compiler->chunk->count - offset - 2;

    compiler->chunk->bytes[offset] = jump >> 8;
    compiler->chunk->bytes[offset + 1] = jump;
}

static void compiler_emit_loop(Compiler *compiler, uint32_t source) {
    chunk_add_byte(compiler->chunk, OP_LOOP, source);

    uint32_t back_offset = compiler->chunk->count - compiler->loop.start + 2;

    chunk_add_byte(compiler->chunk, back_offset >> 8, source);
    chunk_add_byte(compiler->chunk, back_offset, source);
}

static bool compile_while_loop(Compiler *compiler, AstNode node,
                               uint32_t source) {

    Loop prev_loop = compiler->loop;

    compiler->loop = (Loop){
        .breaks = {0},
        .start = compiler->chunk->count,
        .inside = true,
    };

    if (!compile_expr(compiler, node.lhs)) {
        return false;
    }

    uint32_t exit_jump =
        compiler_emit_jump(compiler, OP_POP_JUMP_IF_FALSE, source);

    if (!compile_stmt(compiler, node.rhs)) {
        return false;
    }

    compiler_emit_loop(compiler, source);

    compiler_patch_jump(compiler, exit_jump);

    for (size_t i = 0; i < compiler->loop.breaks.count; ++i) {
        compiler_patch_jump(compiler, compiler->loop.breaks.items[i]);
    }

    ARRAY_FREE(&compiler->loop.breaks);

    compiler->loop = prev_loop;

    return true;
}

static bool compile_conditional(Compiler *compiler, AstNode node,
                                uint32_t source) {
    AstNodeIdx true_case = compiler->ast.extra.items[node.rhs];
    AstNodeIdx false_case = compiler->ast.extra.items[node.rhs + 1];

    if (!compile_expr(compiler, node.lhs)) {
        return false;
    }

    uint32_t then_jump =
        compiler_emit_jump(compiler, OP_POP_JUMP_IF_FALSE, source);

    if (!compile_stmt(compiler, true_case)) {
        return false;
    }

    uint32_t else_jump = compiler_emit_jump(compiler, OP_JUMP, source);

    compiler_patch_jump(compiler, then_jump);

    if (false_case != INVALID_NODE_IDX) {
        if (!compile_stmt(compiler, false_case)) {
            return false;
        }
    }

    compiler_patch_jump(compiler, else_jump);

    return true;
}

static bool compile_unary(Compiler *compiler, AstNode node, uint32_t source,
                          OpCode opcode) {
    if (!compile_expr(compiler, node.rhs)) {
        return false;
    }

    chunk_add_byte(compiler->chunk, opcode, source);

    return true;
}

static bool compile_binary(Compiler *compiler, AstNode node, uint32_t source,
                           OpCode opcode) {
    if (!compile_expr(compiler, node.lhs)) {
        return false;
    }

    if (!compile_expr(compiler, node.rhs)) {
        return false;
    }

    chunk_add_byte(compiler->chunk, opcode, source);

    return true;
}

static bool compile_break(Compiler *compiler, uint32_t source) {
    if (!compiler->loop.inside) {
        compiler_error(
            compiler, source,
            "using a break statement outside of a loop is meaningless\n");

        return false;
    }

    uint32_t offset = compiler_emit_jump(compiler, OP_JUMP, source);

    ARRAY_PUSH(&compiler->loop.breaks, offset);

    return true;
}

static bool compile_continue(Compiler *compiler, uint32_t source) {
    if (!compiler->loop.inside) {
        compiler_error(
            compiler, source,
            "using a continue statement outside of a loop is meaningless\n");

        return false;
    }

    compiler_emit_loop(compiler, source);

    return true;
}

static bool compile_call(Compiler *compiler, AstNode node, uint32_t source) {
    uint32_t argc = 0;

    if (node.rhs != INVALID_EXTRA_IDX) {
        uint32_t start = node.rhs + 1;

        argc = compiler->ast.extra.items[node.rhs];

        if (argc > UINT8_MAX) {
            compiler_error(compiler, source,
                           "passing %d arguments exceeds the limit of %d\n",
                           (int)argc, UINT8_MAX);

            return false;
        }

        for (uint32_t i = 0; i < argc; i++) {
            if (!compile_expr(compiler, compiler->ast.extra.items[start + i])) {
                return false;
            }
        }
    }

    if (!compile_expr(compiler, node.lhs)) {
        return false;
    }

    chunk_add_byte(compiler->chunk, OP_CALL, source);
    chunk_add_byte(compiler->chunk, argc, source);

    return true;
}

bool compile_stmt(Compiler *compiler, AstNodeIdx node_idx) {
    AstNode node = compiler->ast.nodes.items[node_idx];
    uint32_t source = compiler->ast.nodes.sources[node_idx];

    switch (node.tag) {
    case NODE_BLOCK:
        return compile_block(compiler, node);

    case NODE_RETURN:
        return compile_return(compiler, node, source);

    case NODE_WHILE:
        return compile_while_loop(compiler, node, source);

    case NODE_IF:
        return compile_conditional(compiler, node, source);

    case NODE_BREAK:
        return compile_break(compiler, source);

    case NODE_CONTINUE:
        return compile_continue(compiler, source);

    default:
        if (!compile_expr(compiler, node_idx)) {
            return false;
        }

        chunk_add_byte(compiler->chunk, OP_POP, source);

        return true;
    }
}

bool compile_expr(Compiler *compiler, AstNodeIdx node_idx) {
    AstNode node = compiler->ast.nodes.items[node_idx];
    uint32_t source = compiler->ast.nodes.sources[node_idx];

    switch (node.tag) {
    case NODE_BLOCK:
    case NODE_RETURN:
    case NODE_WHILE:
    case NODE_IF:
    case NODE_BREAK:
    case NODE_CONTINUE:
        return false;

    case NODE_IDENTIFIER:
        return compile_identifier(compiler, node, source);

    case NODE_STRING:
        compile_string(compiler, node, source);
        return true;

    case NODE_INT:
        compile_int(compiler, node, source);
        return true;

    case NODE_FLOAT:
        compile_float(compiler, node, source);
        return true;

    case NODE_FUNCTION:
        return compile_function(compiler, node, source);

    case NODE_ARRAY:
        return compile_array(compiler, node, source);

    case NODE_MAP:
        return compile_map(compiler, node, source);

    case NODE_SUBSCRIPT:
        return compile_subscript(compiler, node, source);

    case NODE_MEMBER:
        return compile_member(compiler, node, source);

    case NODE_ASSIGN:
        return compile_assign(compiler, node, source, false, 0);

    case NODE_ASSIGN_ADD:
        return compile_assign(compiler, node, source, true, OP_ADD);

    case NODE_ASSIGN_SUB:
        return compile_assign(compiler, node, source, true, OP_SUB);

    case NODE_ASSIGN_MUL:
        return compile_assign(compiler, node, source, true, OP_MUL);

    case NODE_ASSIGN_DIV:
        return compile_assign(compiler, node, source, true, OP_DIV);

    case NODE_ASSIGN_MOD:
        return compile_assign(compiler, node, source, true, OP_MOD);

    case NODE_ASSIGN_POW:
        return compile_assign(compiler, node, source, true, OP_POW);

    case NODE_NOT:
        return compile_unary(compiler, node, source, OP_NOT);

    case NODE_NEG:
        return compile_unary(compiler, node, source, OP_NEG);

    case NODE_ADD:
        return compile_binary(compiler, node, source, OP_ADD);

    case NODE_SUB:
        return compile_binary(compiler, node, source, OP_SUB);

    case NODE_MUL:
        return compile_binary(compiler, node, source, OP_MUL);

    case NODE_DIV:
        return compile_binary(compiler, node, source, OP_DIV);

    case NODE_POW:
        return compile_binary(compiler, node, source, OP_POW);

    case NODE_MOD:
        return compile_binary(compiler, node, source, OP_MOD);

    case NODE_EQL:
        return compile_binary(compiler, node, source, OP_EQL);

    case NODE_NEQ:
        return compile_binary(compiler, node, source, OP_NEQ);

    case NODE_LT:
        return compile_binary(compiler, node, source, OP_LT);

    case NODE_GT:
        return compile_binary(compiler, node, source, OP_GT);

    case NODE_LTE:
        return compile_binary(compiler, node, source, OP_LTE);

    case NODE_GTE:
        return compile_binary(compiler, node, source, OP_GTE);

    case NODE_CALL:
        return compile_call(compiler, node, source);

    default:
        return false;
    }
}

#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef enum {
    OBJ_FUNCTION,
    OBJ_NATIVE,
    OBJ_ARRAY,
    OBJ_MAP,
    OBJ_STRING,
} ObjTag;

typedef struct Obj {
    ObjTag tag;
    bool marked;
    struct Obj *next;
} Obj;

typedef enum : uint8_t {
    VAL_NULL,
    VAL_BOOL,
    VAL_INT,
    VAL_FLT,
    VAL_OBJ,
} ValueTag;

typedef union {
    bool _bool;
    int64_t _int;
    double _flt;
    Obj *_obj;
} ValuePayload;

typedef struct {
    ValuePayload payload;
    ValueTag tag;
} Value;

#define IS_NULL(v) ((v).tag == VAL_NULL)
#define IS_BOOL(v) ((v).tag == VAL_BOOL)
#define IS_INT(v) ((v).tag == VAL_INT)
#define IS_FLT(v) ((v).tag == VAL_FLT)
#define IS_OBJ(v) ((v).tag == VAL_OBJ)

#define AS_BOOL(v) ((v).payload._bool)
#define AS_INT(v) ((v).payload._int)
#define AS_FLT(v) ((v).payload._flt)
#define AS_OBJ(v) ((v).payload._obj)

static inline bool is_obj_tag(Value v, ObjTag tag) {
    return IS_OBJ(v) && AS_OBJ(v)->tag == tag;
}

#define IS_FUNCTION(v) is_obj_tag(v, OBJ_FUNCTION)
#define IS_NATIVE(v) is_obj_tag(v, OBJ_NATIVE)
#define IS_ARRAY(v) is_obj_tag(v, OBJ_ARRAY)
#define IS_MAP(v) is_obj_tag(v, OBJ_MAP)
#define IS_STRING(v) is_obj_tag(v, OBJ_STRING)

#define NULL_VAL ((Value){.tag = VAL_NULL})
#define BOOL_VAL(v) ((Value){.tag = VAL_BOOL, .payload = {._bool = (v)}})
#define INT_VAL(v) ((Value){.tag = VAL_INT, .payload = {._int = (v)}})
#define FLT_VAL(v) ((Value){.tag = VAL_FLT, .payload = {._flt = (v)}})
#define OBJ_VAL(v) ((Value){.tag = VAL_OBJ, .payload = {._obj = (Obj *)(v)}})

typedef enum : uint8_t {
    OP_POP,
    OP_DUP,
    OP_SWP,
    OP_PUSH_NULL,
    OP_PUSH_TRUE,
    OP_PUSH_FALSE,
    OP_PUSH_CONST,
    OP_GET_LOCAL,
    OP_SET_LOCAL,
    OP_GET_GLOBAL,
    OP_SET_GLOBAL,
    OP_GET_SUBSCRIPT,
    OP_SET_SUBSCRIPT,
    OP_MAKE_ARRAY,
    OP_NEG,
    OP_NOT,
    OP_ADD,
    OP_SUB,
    OP_MUL,
    OP_DIV,
    OP_POW,
    OP_MOD,
    OP_EQL,
    OP_NEQ,
    OP_LT,
    OP_GT,
    OP_LTE,
    OP_GTE,
    OP_CALL,
    OP_POP_JUMP_IF_FALSE,
    OP_JUMP_IF_FALSE,
    OP_JUMP,
    OP_LOOP,
    OP_RETURN,
} OpCode;

typedef struct {
    Value *items;
    size_t count;
    size_t capacity;
} Constants;

typedef struct {
    const char *file_path;
    const char *file_content;
    Constants constants;
    uint8_t *bytes;
    uint32_t *sources;
    size_t count;
    size_t capacity;
} Chunk;

size_t chunk_add_constant(Chunk *, Value);
size_t chunk_add_byte(Chunk *, uint8_t byte, uint32_t source);

typedef struct {
    Obj obj;
    char *items;
    uint32_t count;
    uint32_t hash;
} ObjString;

typedef struct {
    Obj obj;
    Chunk chunk;
    uint8_t arity;
} ObjFunction;

typedef struct {
    Obj obj;
    Value *items;
    uint32_t count;
    uint32_t capacity;
} ObjArray;

typedef struct {
    ObjString *key;
    Value value;
} ObjMapEntry;

typedef struct {
    Obj obj;
    ObjMapEntry *entries;
    uint32_t count;
    uint32_t capacity;
} ObjMap;

#define AS_STRING(v) ((ObjString *)AS_OBJ(v))
#define AS_FUNCTION(v) ((ObjFunction *)AS_OBJ(v))
#define AS_ARRAY(v) ((ObjArray *)AS_OBJ(v))
#define AS_MAP(v) ((ObjMap *)AS_OBJ(v))

#define VM_FRAMES_MAX 64
#define VM_STACK_MAX (VM_FRAMES_MAX * 255)
#define VM_GC_GROW_FACTOR 2

typedef struct {
    ObjFunction *fn;
    uint8_t *ip;
    Value *slots;
} CallFrame;

typedef struct {
    CallFrame frames[VM_FRAMES_MAX];
    size_t frame_count;

    Value stack[VM_STACK_MAX];
    Value *sp;

    ObjMap *globals;

    Obj *objects;
    size_t bytes_allocated;
    size_t next_gc;
} Vm;

typedef bool (*NativeFn)(Vm *, uint16_t argc);

typedef struct {
    Obj obj;
    NativeFn fn;
} ObjNative;

#define AS_NATIVE(v) ((ObjNative *)AS_OBJ(v))

bool chunks_equal(Chunk, Chunk);
bool objects_equal(Obj *, Obj *);
bool values_exactly_equal(Value, Value); // 4 != 4.0
bool values_equal(Value, Value);         // 4 == 4.0

void value_display(Value);

void vm_stack_reset(Vm *);

void vm_init(Vm *);

bool vm_load_file(Vm *vm, const char *file_path, const char *file_buffer);

bool vm_run(Vm *, Value *result);

[[gnu::format(printf, 2, 3)]]
void vm_error(Vm *vm, const char *format, ...);

Obj *vm_alloc(Vm *, ObjTag, size_t);

#define OBJ_ALLOC(vm, tag, type) (type *)vm_alloc(vm, tag, sizeof(type))

ObjString *vm_new_string(Vm *vm, char *items, uint32_t count, uint32_t hash);
ObjString *vm_copy_string(Vm *vm, const char *items, uint32_t count);

ObjArray *vm_copy_array(Vm *vm, const Value *items, uint32_t count);

ObjFunction *vm_new_function(Vm *vm, Chunk chunk, uint8_t arity);

void vm_gc(Vm *);

void vm_push(Vm *, Value);

Value vm_pop(Vm *);

Value vm_peek(const Vm *, size_t distance);

void vm_poke(Vm *, size_t distance, Value);

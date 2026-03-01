#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef enum {
    OBJ_CLOSURE,
    OBJ_UPVALUE,
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

#define IS_CLOSURE(v) is_obj_tag(v, OBJ_CLOSURE)
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
    OP_GET_UPVALUE,
    OP_SET_UPVALUE,
    OP_CLOSE_UPVALUE,
    OP_GET_GLOBAL,
    OP_SET_GLOBAL,
    OP_GET_SUBSCRIPT,
    OP_SET_SUBSCRIPT,
    OP_MAKE_ARRAY,
    OP_MAKE_MAP,
    OP_MAKE_CLOSURE,
    OP_MAKE_SLICE,       // a[s:e]
    OP_MAKE_SLICE_ABOVE, // a[s:]
    OP_MAKE_SLICE_UNDER, // a[:e]
    OP_COPY_BY_SLICING,  // a[:]
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

typedef struct {
    Obj obj;
    ObjMap *globals;
    Chunk chunk;
    uint8_t arity;
    uint8_t upvalues_count;
} ObjFunction;

typedef struct ObjUpvalue {
    Obj obj;
    Value *location;
    Value closed;
    struct ObjUpvalue *next;
} ObjUpvalue;

typedef struct {
    Obj obj;
    ObjFunction *fn;
    ObjUpvalue **upvalues;
    uint8_t upvalues_count;
} ObjClosure;

#define AS_STRING(v) ((ObjString *)AS_OBJ(v))
#define AS_CLOSURE(v) ((ObjClosure *)AS_OBJ(v))
#define AS_FUNCTION(v) ((ObjFunction *)AS_OBJ(v))
#define AS_ARRAY(v) ((ObjArray *)AS_OBJ(v))
#define AS_MAP(v) ((ObjMap *)AS_OBJ(v))

#define VM_FRAMES_MAX 64
#define VM_STACK_MAX (VM_FRAMES_MAX * 255)
#define VM_GC_GROW_FACTOR 2

typedef struct {
    ObjClosure *closure;
    uint8_t *ip;
    Value *slots;
} CallFrame;

typedef struct {
    CallFrame frames[VM_FRAMES_MAX];
    size_t frame_count;

    Value stack[VM_STACK_MAX];
    Value *sp;

    ObjUpvalue *open_upvalues;

    ObjMap *strings;

    Obj *objects;
    size_t bytes_allocated;
    size_t next_gc;
} Vm;

typedef bool (*NativeFn)(Vm *, Value *argv, uint8_t argc, Value *result);

typedef struct {
    Obj obj;
    NativeFn fn;
} ObjNative;

#define AS_NATIVE(v) ((ObjNative *)AS_OBJ(v))

bool chunks_equal(Chunk, Chunk);
bool objects_equal(Obj *, Obj *);
bool values_exactly_equal(Value, Value); // 4 != 4.0
bool values_equal(Value, Value);         // 4 == 4.0

const char *value_description(Value value);
void value_display(Value);
bool value_is_falsey(Value);
int64_t value_to_int(Value);
double value_to_float(Value);
uint32_t string_hash(const char *key, uint32_t count);
uint32_t string_utf8_characters_count(const char *start, const char *end);
uint32_t string_utf8_encode_character(
    uint32_t rune, char *result); // result should be at least 4 bytes long
const char *string_utf8_skip_character(const char *start);
const char *string_utf8_decode_character(const char *start, uint32_t *rune);

ObjString *vm_find_string(Vm *vm, const char *key, uint32_t count,
                              uint32_t hash);

void vm_stack_reset(Vm *);
void vm_stack_trace(Vm *);

void vm_init(Vm *);

bool vm_load_file(Vm *vm, const char *file_path, const char *file_buffer);

bool vm_run(Vm *, Value *result);

[[gnu::format(printf, 2, 3)]]
void vm_error(Vm *vm, const char *format, ...);

ObjMap *vm_new_map(Vm *vm);
bool vm_map_insert(Vm *vm, ObjMap *map, ObjString *key, Value value);
bool vm_map_insert_by_cstr(Vm *vm, ObjMap *map, const char *key, Value value);
bool vm_map_insert_native_by_cstr(Vm *vm, ObjMap *map, const char *key,
                                  NativeFn call);
void vm_map_insert_builtins(Vm *vm, ObjMap *globals);
bool vm_map_lookup(const ObjMap *map, ObjString *key, Value *value);
bool vm_map_remove(ObjMap *map, ObjString *key);

static inline void vm_push(Vm *vm, Value value) {
    *vm->sp = value;
    vm->sp++;
}

static inline Value vm_pop(Vm *vm) {
    vm->sp--;
    return *vm->sp;
}

static inline Value vm_peek(const Vm *vm, size_t distance) {
    return vm->sp[-1 - distance];
}

static inline void vm_poke(Vm *vm, size_t distance, Value value) {
    vm->sp[-1 - distance] = value;
}

#define OBJ_ALLOC(vm, tag, type) (type *)vm_alloc(vm, tag, sizeof(type))

Obj *vm_alloc(Vm *, ObjTag, size_t);
void vm_free_object(Vm *vm, Obj *obj);
void vm_free_map(Vm *vm, ObjMap *map);
void vm_free_value(Vm *vm, Value);
ObjString *vm_new_string(Vm *vm, char *items, uint32_t count, uint32_t hash);
ObjString *vm_copy_string(Vm *vm, const char *items, uint32_t count);
ObjString *vm_concat_strings(Vm *vm, ObjString *lhs, ObjString *rhs);
ObjArray *vm_copy_array(Vm *vm, const Value *items, uint32_t count);
ObjArray *vm_concat_arrays(Vm *vm, ObjArray *lhs, ObjArray *rhs);
ObjFunction *vm_new_function(Vm *vm, ObjMap *globals, Chunk chunk,
                             uint8_t arity, uint8_t upvalues_count);
ObjClosure *vm_new_closure(Vm *vm, ObjFunction *);
ObjUpvalue *vm_new_upvalue(Vm *vm, Value *);
void vm_mark_object(Vm *vm, Obj *);
void vm_mark_value(Vm *vm, Value);
void vm_mark_roots(Vm *vm);
void vm_gc(Vm *);

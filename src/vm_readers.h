#pragma once

#define READ_BYTE() (*frame->ip++)

#define READ_SHORT()                                                           \
    (frame->ip += 2, ((uint16_t)frame->ip[-2] << 8) | frame->ip[-1])

#define READ_WORD()                                                            \
    (frame->ip += 4, ((uint32_t)frame->ip[-4] << 24) |                         \
                         ((uint32_t)frame->ip[-3] << 16) |                     \
                         ((uint32_t)frame->ip[-2] << 8) | frame->ip[-1])

#define READ_CONSTANT()                                                        \
    (frame->closure->fn->chunk.constants.items[READ_SHORT()])

#define READ_STRING() (AS_STRING(READ_CONSTANT()))

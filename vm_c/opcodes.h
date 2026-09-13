#ifndef CVM_OPCODES_H
#define CVM_OPCODES_H

#define CVM_OPCODE_TABLE_VERSION 3

typedef enum {
    OP_NOP = 0x00,
    OP_LOAD_CONST = 0x01,
    OP_LOAD_FAST = 0x02,
    OP_STORE_FAST = 0x03,
    OP_LOAD_GLOBAL = 0x04,
    OP_STORE_GLOBAL = 0x05,
    OP_POP_TOP = 0x06,
    OP_DUP_TOP = 0x07,
    OP_ROT_TWO = 0x08,
    OP_BINARY_ADD = 0x10,
    OP_BINARY_SUB = 0x11,
    OP_BINARY_MUL = 0x12,
    OP_BINARY_DIV = 0x13,
    OP_BINARY_MOD = 0x14,
    OP_BINARY_POW = 0x15,
    OP_BINARY_FLOORDIV = 0x16,
    OP_UNARY_NEG = 0x17,
    OP_UNARY_NOT = 0x18,
    OP_UNARY_POS = 0x19,
    OP_UNARY_INVERT = 0x1A,
    OP_COMPARE_EQ = 0x20,
    OP_COMPARE_NE = 0x21,
    OP_COMPARE_LT = 0x22,
    OP_COMPARE_LE = 0x23,
    OP_COMPARE_GT = 0x24,
    OP_COMPARE_GE = 0x25,
    OP_CONTAINS = 0x26,
    OP_JUMP = 0x30,
    OP_JUMP_IF_FALSE = 0x31,
    OP_JUMP_IF_TRUE = 0x32,
    OP_CALL = 0x40,
    OP_RETURN = 0x41,
    OP_CALL_KW = 0x42,
    OP_BUILD_LIST = 0x50,
    OP_BUILD_TUPLE = 0x51,
    OP_BUILD_MAP = 0x52,
    OP_BUILD_SET = 0x57,
    OP_GET_INDEX = 0x53,
    OP_SET_INDEX = 0x54,
    OP_GET_ITER_ITEM = 0x5B,
    OP_GET_SLICE = 0x5A,
    OP_LIST_APPEND = 0x55,
    OP_SET_ADD = 0x58,
    OP_MAP_ADD = 0x59,
    OP_LEN = 0x56,
    OP_PRINT = 0x60,
    OP_CALL_BUILTIN = 0x61,
    OP_CALL_METHOD = 0x62,
    OP_HALT = 0xFF,
} cvm_opcode_t;

static inline int cvm_operand_width(cvm_opcode_t op) {
    switch (op) {
        case OP_LOAD_CONST: return 4;
        case OP_LOAD_FAST: return 4;
        case OP_STORE_FAST: return 4;
        case OP_LOAD_GLOBAL: return 4;
        case OP_STORE_GLOBAL: return 4;
        case OP_JUMP: return 4;
        case OP_JUMP_IF_FALSE: return 4;
        case OP_JUMP_IF_TRUE: return 4;
        case OP_CALL: return 4;
        case OP_CALL_KW: return 4;
        case OP_BUILD_LIST: return 4;
        case OP_BUILD_TUPLE: return 4;
        case OP_BUILD_MAP: return 4;
        case OP_BUILD_SET: return 4;
        case OP_PRINT: return 4;
        case OP_CALL_BUILTIN: return 4;
        case OP_CALL_METHOD: return 4;
        default: return 0;
    }
}

#endif

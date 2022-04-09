#include <stdint.h>

#define MEMORY_MAX (1 << 16)
uint16_t memory[MEMORY_MAX];

enum {
  R_R0=0,
  R_R1,
  R_R2,
  R_R3,
  R_R4,
  R_R7,
  R_PC,
  R_COND,
  R_COUNT
};

uint16_t reg[R_COUNT];

enum {
  OP_BR=0,   // Branch
  OP_ADD,    // Add
  OP_LD,     // Load
  OP_ST,     // Store
  OP_JSR,    // Jump register
  OP_AND,    // Bitwise and
  OP_LDR,    // Load register
  OP_STR,    // Store register
  OP_RTI,    //
  OP_NOT,    // Bitwise not
  OP_LDI,    // Load indirect
  OP_STI,    // Store indirect
  OP_JMP,    // Jump
  OP_RES,    //
  OP_LEA,    // Load effective address
  OP_TRAP,   // Execute trap
};

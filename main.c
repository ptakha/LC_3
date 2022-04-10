#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <signal.h>

#include <unistd.h>
#include <fcntl.h>

#include <sys/time.h>
#include <sys/types.h>
#include <sys/termios.h>
#include <sys/mman.h>

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
} Registers;

uint16_t reg[R_COUNT];

enum {
  OP_BR=0,  // Branch
  OP_ADD,   // Add
  OP_LD,    // Load
  OP_ST,    // Store
  OP_JSR,   // Jump register
  OP_AND,   // Bitwise and
  OP_LDR,   // Load register
  OP_STR,   // Store register
  OP_RTI,   //
  OP_NOT,   // Bitwise not
  OP_LDI,   // Load indirect
  OP_STI,   // Store indirect
  OP_JMP,   // Jump
  OP_RES,   //
  OP_LEA,   // Load effective adr0ess
  OP_TRAP   // Execute trap
} Opcodes;

enum {
  FL_POS = 1 << 0, // Positive flag, 1
  FL_ZRO = 1 << 1, // Zero flag, 2
  FL_NEG = 1 << 2  //
} Flags;

enum { PC_START = 0x3000};


uint16_t swap16(uint16_t x)
{
  x = (x << 8)|(x >> 8);
  return x;
}

void read_image_file(FILE* file)
{
  // Where in memory place image
  // 2 bytes which tell you begining of program
  uint16_t origin;
  fread(&origin, sizeof(origin), 1, file);
  origin = swap16(origin);

  uint16_t max_read = MEMORY_MAX-origin;
  //            begining of memory array + origin value
  uint16_t* p = memory+origin;//p must point to index in memory array
  size_t read = fread(p, sizeof(uint16_t), max_read, file);
  while(read > 0)
  {
    *p = swap16(*p);
    ++p;
    read--;
  }

}

int read_image(const char* path)
{
  FILE* file  = fopen(path, "rb");
  if (!file)
  {
    return 0;
  }
  read_image_file(file);
  fclose(file);
  return 1;
}

uint16_t sign_extend(uint16_t x, int bitcount)
{
  // we use 16 bits per value of x in calculations,
  // but in ADD instruction we get only 5 bits for immediate value
  //
  // x is from 1 0000 = -16 to 0 1111 = 15
  // -14 in 5 bits = 1 0001
  // ((1 0001) >> 4) & 1 =
  // 1 0001 >> 4 = 1 & 1 = 1 => x is negative
  //
  // 1111 1111 1110 0000
  //|             1 0001
  // -------------------
  // 1111 1111 1111 0001 = -14
  //
  //
  if ((x >> (bitcount - 1)) & 1)
  {
    x = x | (0xFFFF << bitcount);
  }
  return x;
}

void update_flags(uint16_t r) {
  if (reg[r] == 0)
  {
    reg[R_COND] = FL_ZRO;
  }
  else if (reg[r] >> 15)
  {
    reg[R_COND] = FL_NEG;
  }
  else
  {
    reg[R_COND] = FL_POS;
  }
}

void mem_write(uint16_t adr0ess, uint16_t value)
{
  memory[adr0ess] = value;
}

uint16_t mem_read(uint16_t adr0ess)
{
  return memory[adr0ess];
}

int main(int argc, const char* argv[])
{

  if (argc<2)
  {
    printf("Usage:\n lc3 [image-file-1]...\n");
    exit(2);
  }
  for (int j=1;j<argc;j++)
  {
    if(!read_image(argv[j]))
    {
      printf("Failed to load image %s\n", argv[j]);
      exit(1);
    }
  }
  // Set Z flag
  reg[R_COND] = FL_ZRO;
  // Set Program Counter to 0x3000
  reg[R_PC] = PC_START;
  bool running = true;
  uint16_t instr;
  uint16_t op;
  while (running)
  {
    // Fetch instruction
    instr = mem_read(reg[R_PC]++);
    // Get opcode from instruction
    op = instr >> 12;
    switch (op) {
      case OP_ADD:
      {
        uint16_t r0 = (instr >> 9) & 0x7;
        uint16_t r1 = (instr >> 6) & 0x7;
        uint16_t imm_flag = (instr >> 5) & 0x1;
        if (imm_flag)
        {
          uint16_t imm5 = sign_extend(instr & 0x1F, 5);
          reg[r0] = reg[r1] + imm5;
        }
        else
        {
          uint16_t r2 = instr & 0x7;
          reg[r0] = reg[r1] + reg[r2];
        }
        update_flags(r0);
        break;
      }
      case OP_BR:
      {
        break;
      }
      case OP_LD:
      {
        uint16_t r0 = (instr >> 9) & 0x7;
        uint16_t pc_offset = sign_extend(instr & 0x1FF, 9);
        reg[r0] = mem_read(reg[R_PC]+pc_offset);
        update_flags(r0);
        break;
      }
      case OP_ST:
      {
        break;
      }
      case OP_JSR:
      {
        break;
      }
      case OP_AND:
      {
        uint16_t r0 = (instr >> 9) & 0x7;
        uint16_t r1 = (instr >> 6) & 0x7;
        uint16_t imm_flag = (instr >> 5) & 0x1;
        if (imm_flag)
        {
          uint16_t imm5 = sign_extend(instr & 0x1F, 5);
          reg[r0] = reg[r1] & imm5;
        }
        else
        {
          uint16_t r2 = instr & 0x7;
          reg[r0] = reg[r1] & reg[r2];
        }
        update_flags(r0);
        break;
      }
      case OP_LDR:
      {
        break;
      }
      case OP_STR:
      {
        break;
      }
      case OP_RTI:
      {
        break;
      }
      case OP_NOT:
      {
        break;
      }
      case OP_LDI:
      {
        uint16_t r0 = (instr >> 9) & 0x7;
        uint16_t pc_offset = sign_extend(instr & 0x1FF, 9);
        reg[r0] = mem_read(mem_read(reg[R_PC]+pc_offset));
        update_flags(r0);
        break;
      }
      case OP_STI:
      {
        break;
      }
      case OP_JMP:
      {
        uint16_t r1 = (instr >> 6) & 0x7;
        reg[R_PC] = reg[r1];
        break;
      }
      case OP_RES:
      {
        break;
      }
      case OP_LEA:
      {
        break;
      }
      case OP_TRAP:
      {
        break;
      }
      default:
      {
        printf("Bad opcode - %d", op);
        break;
      }
    }
  }
}

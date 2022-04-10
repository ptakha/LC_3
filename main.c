#include <stdio.h>
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
  R_R5,
  R_R6,
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
enum {
  TRAP_GETC = 0x20,  // Get character from keyboard, not echoed onto the terminal
  TRAP_OUT = 0x21,   // Output a character
  TRAP_PUTS = 0x22,  // Output a word string
  TRAP_IN = 0x23,    // Get character from keyboard, echoed onto terminal
  TRAP_PUTSP = 0x24, // Output a byte string
  TRAP_HALT = 0x25   // Halt the program
} Trap_codes;
enum
{
    MR_KBSR = 0xFE00, /* keyboard status */
    MR_KBDR = 0xFE02  /* keyboard data */
}Memory_Mapped_Registers;

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
  while(read-- > 0)
  {
    *p = swap16(*p);
    ++p;
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
    x |= (0xFFFF << bitcount);
  }
  return x;
}

void update_flags(uint16_t r)
{
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

uint16_t check_key()
{
    fd_set readfds;
    FD_ZERO(&readfds);
    FD_SET(STDIN_FILENO, &readfds);

    struct timeval timeout;
    timeout.tv_sec = 0;
    timeout.tv_usec = 0;
    return select(1, &readfds, NULL, NULL, &timeout) != 0;
}

void mem_write(uint16_t address, uint16_t value)
{
  memory[address] = value;
}

uint16_t mem_read(uint16_t address)
{
    if (address == MR_KBSR)
    {
        if (check_key())
        {
            memory[MR_KBSR] = (1 << 15);
            memory[MR_KBDR] = getchar();
        }
        else
        {
            memory[MR_KBSR] = 0;
        }
    }
    return memory[address];
}
struct termios original_tio;

void disable_input_buffering()
{
    tcgetattr(STDIN_FILENO, &original_tio);
    struct termios new_tio = original_tio;
    new_tio.c_lflag &= ~ICANON & ~ECHO;
    tcsetattr(STDIN_FILENO, TCSANOW, &new_tio);
}

void restore_input_buffering()
{
    tcsetattr(STDIN_FILENO, TCSANOW, &original_tio);
}
void handle_interrupt(int signal)
{
    restore_input_buffering();
    printf("\n");
    exit(-2);
}


int main(int argc, const char* argv[])
{
  // Setup
  // Check number of arguments
  if (argc<2)
  {
    printf("Usage:\n lc3 [image-file-1]...\n");
    exit(2);
  }
  // Load images
  for (int j=1;j<argc;++j)
  {
    if(!read_image(argv[j]))
    {
      printf("Failed to load image %s\n", argv[j]);
      exit(1);
    }
  }
  signal(SIGINT, handle_interrupt);
  disable_input_buffering();
  // Set Z flag
  reg[R_COND] = FL_ZRO;
  // Set Program Counter to 0x3000
  enum { PC_START = 0x3000};
  reg[R_PC] = PC_START;
  int running = 1;
  // uint16_t instr;
  // uint16_t op;
  while (running)
  {
    // Fetch instruction
    uint16_t instr = mem_read(reg[R_PC]++);
    // Get opcode from instruction
    uint16_t op = instr >> 12;
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
      }
      break;
      case OP_BR:
      {
        uint16_t pc_offset = sign_extend(instr & 0x1FF, 9);
        uint16_t cond_flag = (instr >> 9) & 0x7;
        if (reg[R_COND] & cond_flag)
        {
          reg[R_PC] += pc_offset;
        }
      }
      break;
      case OP_LD:
      {
        uint16_t r0 = (instr >> 9) & 0x7;
        uint16_t pc_offset = sign_extend(instr & 0x1FF, 9);
        reg[r0] = mem_read(reg[R_PC]+pc_offset);
        update_flags(r0);
      }
      break;
      case OP_ST:
      {
        uint16_t r0 = (instr >> 9) & 0x7;
        uint16_t pc_offset = sign_extend(instr & 0x1FF, 9);
        mem_write(reg[R_PC]+pc_offset, reg[r0]);
      }
      break;
      case OP_JSR:
      {
        reg[R_R7] = reg[R_PC];
        uint16_t flag = (instr >> 11) & 1;
        if (flag)
        {
          uint16_t pc_offset = sign_extend(instr & 0x7FF, 11);
          reg[R_PC] += pc_offset;
        }
        else
        {
          uint16_t r1 = (instr >> 6) & 0x7;
          reg[R_PC] = reg[r1];
        }
      }
      break;
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
      }
      break;
      case OP_LDR:
      {
        uint16_t r0 = (instr >> 9) & 0x7;
        uint16_t r1 = (instr >> 6) & 0x7;
        uint16_t offset = sign_extend(instr & 0x3F, 6);
        reg[r0] = mem_read(reg[r1]+offset);
        update_flags(r0);
      }
      break;
      case OP_STR:
      {
        uint16_t r0 = (instr >> 9) & 0x7;
        uint16_t r1 = (instr >> 6) & 0x7;
        uint16_t offset = sign_extend(instr & 0x3F, 6);
        mem_write(reg[r1]+offset, reg[r0]);
      }
      break;
      case OP_NOT:
      {
        uint16_t r0 = (instr >> 9) & 0x7;
        uint16_t r1 = (instr >> 6) & 0x7;
        reg[r0] = ~reg[r1];
        update_flags(r0);
      }
      break;
      case OP_LDI:
      {
        uint16_t r0 = (instr >> 9) & 0x7;
        uint16_t pc_offset = sign_extend(instr & 0x1FF, 9);
        reg[r0] = mem_read(mem_read(reg[R_PC]+pc_offset));
        update_flags(r0);
      }
      break;
      case OP_STI:
      {
        uint16_t r0 = (instr >> 9) & 0x7;
        uint16_t pc_offset = sign_extend(instr & 0x1FF, 9);
        mem_write(mem_read(reg[R_PC]+pc_offset), reg[r0]);
      }
      break;
      case OP_JMP:
      {
        uint16_t r1 = (instr >> 6) & 0x7;
        reg[R_PC] = reg[r1];
      }
      break;
      case OP_LEA:
      {
        uint16_t r0 = (instr >> 9) & 0x7;
        uint16_t pc_offset = sign_extend(instr & 0x1FF, 9);
        reg[r0] = reg[R_PC]+pc_offset;
        update_flags(r0);
      }
      break;
      case OP_TRAP:
      {
        reg[R_R7] = reg[R_PC];
        switch (instr & 0xFF) {
          case TRAP_IN:
          {
            printf("Enter a character: ");
            char c = getchar();
            putc(c, stdout);
            fflush(stdout);
            reg[R_R0] = (uint16_t)c;
            update_flags(R_R0);
          }
          break;
          case TRAP_OUT:
          {
            putc((char)reg[R_R0], stdout);
            fflush(stdout);
          }
          break;
          case TRAP_GETC:
          {
            reg[R_R0] = (uint16_t)getchar();
            update_flags(R_R0);
          }
          break;
          case TRAP_PUTS:
          {
            uint16_t* c = memory+reg[R_R0];
            while (*c)
            {
              putc((char)*c, stdout);
              ++c;
            }
            fflush(stdout);
          }
          break;
          case TRAP_PUTSP:
          {
            uint16_t* c = memory + reg[R_R0];
            while (*c)
            {
                char char1 = (*c) & 0xFF;
                putc(char1, stdout);
                char char2 = (*c) >> 8;
                if (char2) putc(char2, stdout);
                ++c;
            }
            fflush(stdout);
          }
          break;
          case TRAP_HALT:
          {
            puts("HALT");
            fflush(stdout);
            running = 0;
            break;
          }
        }
        break;
      }
      case OP_RES:
      case OP_RTI:
      default:
      {
        abort();
        break;
      }
    }
  }

  //Shutdown
  restore_input_buffering();
  return 0;
}

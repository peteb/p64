#include "asm.h"
#include "6502.h"
#include <string.h>
#include <stdio.h>

void print_instr(uint8_t *mem, uint16_t len, uint16_t start_ofs) {
  /* prints the instructions at pc, up to len bytes */
  uint16_t end = len;
  uint16_t pc = 0;
  
  while (pc < end) {
    uint16_t instr_pc = pc;
    uint8_t code = mem[pc++];
    opc_descr_t *opcode = instr_descr(code);
    char instr[64] = {0};
    char data[64] = {0};
    size_t num_bytes = 1;

    sprintf(data, ".%04X  ", instr_pc + start_ofs);

    if (opcode->cfun) {
      uint16_t val;
      uint8_t hi;

      switch (opcode->addr_m) {
      case ADR_IMM:
        val = mem[pc++];
        num_bytes = 2;
        sprintf(instr, "%s #$%02X", opcode->name, val);
        break;

      case ADR_ABS:
        num_bytes = 3;
        hi = mem[pc++];
        val = (uint16_t)hi << 8 | mem[pc++];
        sprintf(instr, "%s $%04X", opcode->name, val);
        break;

      default:
        strcpy(instr, opcode->name);
      }
    }

    while (num_bytes--) {
      sprintf(data + strlen(data), "%02X ", mem[instr_pc++]);
    }

    printf("%-16s %s\n", data, instr);
  }
}

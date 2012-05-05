#ifndef P64_ASM_H
#define P64_ASM_H

/*
 * A non-optimizing assembler & disassembler for 6502 asm.
 * Perhaps it would be best to move it into 6502.c/.h.
 */

#include <stdint.h>
#include <stdlib.h>
#include "6502.h"


#define SYM_GLOBAL         0x01
#define ASM_NO_LINK_ERROR  0x01

typedef struct sym_def {
  char *id;
  uint16_t address;
  uint8_t flags;
} sym_def_t;

typedef struct symtab {
  sym_def_t symbols[100];
  size_t num_symbols;
} symtab_t;

void print_instr(uint8_t *, uint16_t len, uint16_t start_ofs);
void parse_asm(const char *, struct cpu_state *, struct symtab *);
void sym_clear(struct symtab *syms);

#endif /* !P64_ASM_H */

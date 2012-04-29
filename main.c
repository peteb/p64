#include "6502.h"
#include "asm.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <assert.h>
#include <ctype.h>

void repl() {
  static cpu_state_t cpu = {.ps = 0x20, .sp = 0xFF};
  static symtab_t symbols;
  char line[256];

  while (1) {
    printf(".%04X: ", cpu.pc);
    if (fgets(line, 256, stdin)) {
      parse_asm(line, &cpu, &symbols);
      /* how about eval */
    }
    else {
      break;
    }
  }

  print_state(&cpu);
  print_instr(cpu.mem, 40, 0x100);
  sym_clear(&symbols);
}

int main() {
  static cpu_state_t cpu = {
    .ps = 0x20,
    .sp = 0xFF,
    .mem = {0xA9, 0xFA, 0x69, 0x06, 0x08, 0x18, 0x28, 0xBA,  /*  8 */
            0x8E, 0x13, 0x37, 0xEA, 0xEA, 0x20, 0x00, 0x18,  /* 16 */
            0xA2, 0x02, 0xB4, 0x01,    0,    0,    0,    0,  /* 24 */
            0x18, 0xA9, 0x02, 0x09, 0x08, 0x60,    0,    0}
  };

  /* run_machine(&cpu); */
  /* print_state(&cpu); */

  /* cpu.pc = 0; */
  /* print_instr(cpu.mem, 40, 0x0); */

  static const char code[] =
    "org $100\n"
    "     php\n"
    "     jmp main\n"
    "main lda #$FA            ; start of the stuff\n"
    "     adc $137\n"
    "     php\n"
    "     clc                 ; clear carry\n"
    "     plp\n"
    "     tsx\n"
    "     stx $1337\n"
    "     jmp main            ; loop back\n";

  static symtab_t object;

  parse_asm(code, &cpu, &object);
  print_state(&cpu);
  print_instr(cpu.mem, 40, 0x100);
  /*  repl(); */
  return 0;
}


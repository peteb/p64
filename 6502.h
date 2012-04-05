#ifndef P64_6502_H
#define P64_6502_H

#include <stdint.h>

#define PS_C 0x01 /* carry */
#define PS_Z 0x02 /* zero */
#define PS_I 0x04 /* interrupts disabled */
#define PS_D 0x08 /* decimal */
#define PS_B 0x10 /* break */
#define PS_V 0x40 /* overflow */
#define PS_N 0x80 /* negative */

#define ADR_IMP   0
#define ADR_IMM   1 /* immediate */
#define ADR_ABS   2 /* absolute */
#define ADR_ABX   3 /* absolute, X */
#define ADR_ABY   4 /* absolute, Y */
#define ADR_ZP    5 /* zeropage */
#define ADR_ZPX   6 /* zeropage, X */
#define ADR_ZPY   7 /* zeropage, Y */
#define ADR_IZX   8 /* indexed indirect, X */
#define ADR_IZY   9 /* indexed indirect, Y */
#define ADR_IND   10
#define ADR_REL   11

#define MEM_MAX 0x4000

#define PUSH8(cpu, val)  (cpu)->mem[0x100 + (cpu)->sp--] = (val)
#define PUSH16(cpu, val) (cpu)->mem[0x100 + (cpu)->sp] = (val) & 0xFF;  \
                              (cpu)->mem[0x0FF + (cpu)->sp] = (val) >> 8; \
                              (cpu)->sp -= 2
#define POP8(cpu)        (cpu)->mem[0x100 + ++(cpu)->sp]
#define POP16(cpu)       (cpu)->mem[0x101 + (cpu)->sp] << 8 |   \
                              (cpu)->mem[0x102 + (cpu)->sp];    \
                              (cpu)->sp += 2


typedef struct cpu_state {
  uint8_t a, x, y, ps, sp;
  uint16_t pc;
  uint8_t mem[MEM_MAX];
} cpu_state_t;

typedef void (*opcode_fun_t)(cpu_state_t *, uint8_t);

typedef struct opc_descr_t {
  uint8_t addr_m;
  opcode_fun_t cfun;
  const char *name;
} opc_descr_t;

void print_instr(uint8_t *, uint16_t len, uint16_t start_ofs);
void print_state(cpu_state_t *);
void run_machine(cpu_state_t *);
opc_descr_t *instr_descr(uint8_t opc);

#endif /* !P64_6502_H */

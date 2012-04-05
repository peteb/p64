#include <stdint.h>
#include <assert.h>
#include <string.h>
#include <stdio.h>

#include "6502.h"



typedef void (*opcode_fun_t)(cpu_state_t *, uint8_t);

typedef struct opc_descr_t {
  uint8_t addr_m;
  opcode_fun_t cfun;
  const char *name;
} opc_descr_t;

static void op_ld(cpu_state_t *, uint8_t);
static void op_adc(cpu_state_t *, uint8_t);
static void op_tog_i(cpu_state_t *, uint8_t);
static void op_tog_d(cpu_state_t *, uint8_t);
static void op_tog_c(cpu_state_t *, uint8_t);
static void op_clv(cpu_state_t *, uint8_t);
static void op_nop(cpu_state_t *, uint8_t);
static void op_st(cpu_state_t *, uint8_t);
static void op_trans(cpu_state_t *, uint8_t);
static void op_push(cpu_state_t *, uint8_t);
static void op_pull(cpu_state_t *, uint8_t);
static void op_jmp(cpu_state_t *, uint8_t);
static void op_rts(cpu_state_t *, uint8_t);
static void op_jsr(cpu_state_t *, uint8_t);
static void op_ora(cpu_state_t *, uint8_t);
static void op_and(cpu_state_t *, uint8_t);
static void op_eor(cpu_state_t *, uint8_t);
static void op_sbc(cpu_state_t *, uint8_t);
static void op_cmp(cpu_state_t *, uint8_t);
static void op_dec(cpu_state_t *, uint8_t);
static void op_inc(cpu_state_t *, uint8_t);
static void op_br(cpu_state_t *, uint8_t);

static uint16_t adr_fetch(uint8_t mode, cpu_state_t *cpu);


static opc_descr_t opcodes[256] = {
  /* jump/flag */
  [0x18] = {ADR_IMP, op_tog_c, "clc"},
  [0x38] = {ADR_IMP, op_tog_c, "sec"},
  [0x58] = {ADR_IMP, op_tog_i, "cli"},
  [0x78] = {ADR_IMP, op_tog_i, "sei"},
  [0xD8] = {ADR_IMP, op_tog_d, "cld"},
  [0xF8] = {ADR_IMP, op_tog_d, "sed"},
  [0xB8] = {ADR_IMP, op_clv,   "clv"},
  [0xEA] = {ADR_IMP, op_nop,   "nop"},
  [0x60] = {ADR_IMP, op_rts,   "rts"},
  [0x4C] = {ADR_ABS, op_jmp,   "jmp"},
  [0x6C] = {ADR_IND, op_jmp,   "jmp"},
  [0x20] = {ADR_ABS, op_jsr,   "jsr"},

  /* ld */
  [0xA9] = {ADR_IMM, op_ld, "lda"},
  [0xA2] = {ADR_IMM, op_ld, "ldx"},
  [0xA0] = {ADR_IMM, op_ld, "ldy"},
  [0xA5] = {ADR_ZP, op_ld,  "lda"},
  [0xA6] = {ADR_ZP, op_ld , "ldx"},
  [0xA4] = {ADR_ZP, op_ld,  "ldy"},
  [0xB5] = {ADR_ZPX, op_ld, "lda"},
  [0xB4] = {ADR_ZPX, op_ld, "ldy"},
  [0xB6] = {ADR_ZPY, op_ld, "ldx"},
  [0xA1] = {ADR_IZX, op_ld, "lda"},
  [0xB1] = {ADR_IZY, op_ld, "lda"},
  [0xAD] = {ADR_ABS, op_ld, "lda"},
  [0xAE] = {ADR_ABS, op_ld, "ldx"},
  [0xAC] = {ADR_ABS, op_ld, "ldy"},
  [0xBD] = {ADR_ABX, op_ld, "lda"},
  [0xBC] = {ADR_ABX, op_ld, "ldy"},
  [0xB9] = {ADR_ABY, op_ld, "lda"},
  [0xBE] = {ADR_ABY, op_ld, "ldx"},

  /* st */
  [0x85] = {ADR_ZP, op_st,  "sta"},
  [0x86] = {ADR_ZP, op_st,  "stx"},
  [0x84] = {ADR_ZP, op_st,  "sty"},
  [0x95] = {ADR_ZPX, op_st, "sta"},
  [0x94] = {ADR_ZPX, op_st, "sty"},
  [0x96] = {ADR_ZPY, op_st, "stx"},
  [0x81] = {ADR_IZX, op_st, "sta"},
  [0x91] = {ADR_IZY, op_st, "sta"},
  [0x8D] = {ADR_ABS, op_st, "sta"},
  [0x8E] = {ADR_ABS, op_st, "stx"},
  [0x8C] = {ADR_ABS, op_st, "sty"},
  [0x9D] = {ADR_ABX, op_st, "sta"},
  [0x99] = {ADR_ABY, op_st, "sta"},

  /* transform */
  [0xAA] = {ADR_IMP, op_trans, "tax"},
  [0x8A] = {ADR_IMP, op_trans, "txa"},
  [0xA8] = {ADR_IMP, op_trans, "tay"},
  [0x98] = {ADR_IMP, op_trans, "tya"},
  [0xBA] = {ADR_IMP, op_trans, "tsx"},
  [0x9A] = {ADR_IMP, op_trans, "txs"},

  /* push */
  [0x48] = {ADR_IMP, op_push, "pha"},
  [0x08] = {ADR_IMP, op_push, "php"},

  /* pull */
  [0x68] = {ADR_IMP, op_pull, "pla"},
  [0x28] = {ADR_IMP, op_pull, "plp"},

  /* logic, arithmetic */
  [0x09] = {ADR_IMM, op_ora, "ora"},
  [0x05] = {ADR_ZP, op_ora,  "ora"},
  [0x15] = {ADR_ZPX, op_ora, "ora"},
  [0x01] = {ADR_IZX, op_ora, "ora"},
  [0x11] = {ADR_IZY, op_ora, "ora"},
  [0x0D] = {ADR_ABS, op_ora, "ora"},
  [0x1D] = {ADR_ABX, op_ora, "ora"},
  [0x19] = {ADR_ABY, op_ora, "ora"},

  [0x29] = {ADR_IMM, op_and, "and"},
  [0x25] = {ADR_ZP, op_and,  "and"},
  [0x35] = {ADR_ZPX, op_and, "and"},
  [0x21] = {ADR_IZX, op_and, "and"},
  [0x31] = {ADR_IZY, op_and, "and"},
  [0x2D] = {ADR_ABS, op_and, "and"},
  [0x3D] = {ADR_ABX, op_and, "and"},
  [0x39] = {ADR_ABY, op_and, "and"},

  [0x49] = {ADR_IMM, op_eor, "eor"},
  [0x45] = {ADR_ZP, op_eor,  "eor"},
  [0x55] = {ADR_ZPX, op_eor, "eor"},
  [0x41] = {ADR_IZX, op_eor, "eor"},
  [0x51] = {ADR_IZY, op_eor, "eor"},
  [0x4D] = {ADR_ABS, op_eor, "eor"},
  [0x5D] = {ADR_ABX, op_eor, "eor"},
  [0x59] = {ADR_ABY, op_eor, "eor"},

  [0x69] = {ADR_IMM, op_adc, "adc"},
  [0x65] = {ADR_ZP, op_adc,  "adc"},
  [0x75] = {ADR_ZPX, op_adc, "adc"},
  [0x61] = {ADR_IZX, op_adc, "adc"},
  [0x71] = {ADR_IZY, op_adc, "adc"},
  [0x6D] = {ADR_ABS, op_adc, "adc"},
  [0x7D] = {ADR_ABX, op_adc, "adc"},
  [0x79] = {ADR_ABY, op_adc, "adc"},

  [0xE9] = {ADR_IMM, op_sbc, "sbc"},
  [0xE5] = {ADR_ZP, op_sbc,  "sbc"},
  [0xF5] = {ADR_ZPX, op_sbc, "sbc"},
  [0xE1] = {ADR_IZX, op_sbc, "sbc"},
  [0xF1] = {ADR_IZY, op_sbc, "sbc"},
  [0xED] = {ADR_ABS, op_sbc, "sbc"},
  [0xFD] = {ADR_ABX, op_sbc, "sbc"},
  [0xF9] = {ADR_ABY, op_sbc, "sbc"},

  [0xC9] = {ADR_IMM, op_cmp, "cmp"},
  [0xC5] = {ADR_ZP, op_cmp,  "cmp"},
  [0xD5] = {ADR_ZPX, op_cmp, "cmp"},
  [0xC1] = {ADR_IZX, op_cmp, "cmp"},
  [0xD1] = {ADR_IZY, op_cmp, "cmp"},
  [0xCD] = {ADR_ABS, op_cmp, "cmp"},
  [0xDD] = {ADR_ABX, op_cmp, "cmp"},
  [0xD9] = {ADR_ABY, op_cmp, "cmp"},






  [0xC6] = {ADR_ZP, op_dec, "dec"},
  [0xD6] = {ADR_ZPX, op_dec, "dec"},
  [0xCE] = {ADR_ABS, op_dec, "dec"},
  [0xDE] = {ADR_ABX, op_dec, "dec"},
  [0xCA] = {ADR_IMP, op_dec, "dex"},
  [0x88] = {ADR_IMP, op_dec, "dey"},

  [0xE6] = {ADR_ZP, op_inc, "inc"},
  [0xF6] = {ADR_ZPX, op_inc, "inc"},
  [0xEE] = {ADR_ABS, op_inc, "inc"},
  [0xFE] = {ADR_ABX, op_inc, "inc"},
  [0xE8] = {ADR_IMP, op_inc, "inx"},
  [0xC8] = {ADR_IMP, op_inc, "iny"},

  [0x10] = {ADR_REL, op_br, "bpl"},
  [0x30] = {ADR_REL, op_br, "bmi"},
  [0x50] = {ADR_REL, op_br, "bvc"},
  [0x70] = {ADR_REL, op_br, "bvs"},
  [0x90] = {ADR_REL, op_br, "bcc"},
  [0xB0] = {ADR_REL, op_br, "bcs"},
  [0xD0] = {ADR_REL, op_br, "bne"},
  [0xF0] = {ADR_REL, op_br, "beq"}
};

void run_machine(cpu_state_t *cpu) {
  uint8_t opcode;
  while ((opcode = cpu->mem[cpu->pc])) {
    opc_descr_t *op_handler = &opcodes[opcode];
    printf("exec %s (%x)\n", op_handler->name, opcode);
    assert(op_handler && "no handler for opcode type");
    op_handler->cfun(cpu, op_handler->addr_m);
  }
}

uint16_t adr_fetch(uint8_t mode, cpu_state_t *cpu) {
  /* pc should be after tehfocalpoin instruction */

  uint16_t base = 0;
  uint8_t hi;

  switch (mode) {
  case ADR_IMM: return cpu->pc++;
  case ADR_ABS:
  case ADR_ABX:
  case ADR_ABY:
    hi = cpu->mem[cpu->pc++];
    base = (uint16_t)hi << 8 | cpu->mem[cpu->pc++];
    if (mode == ADR_ABX)
      base += cpu->x;
    else if (mode == ADR_ABY)
      base += cpu->y;

    return base;

  case ADR_ZP:
  case ADR_ZPX:
  case ADR_IZY:
    base = cpu->mem[cpu->pc++];

    if (mode == ADR_ZPX)
      base += cpu->x;
    else if (mode == ADR_IZY)
      base += cpu->mem[base & 0xFF] + cpu->y;

    return base;

  case ADR_REL:
    base = cpu->pc - 2 + (int16_t)cpu->mem[cpu->pc++];
    return base;

  case ADR_IZX: assert(!"to be fixed");
  default: assert(!"invalid addressing mode");
  }
}

void op_ld(cpu_state_t *cpu, uint8_t mode) {
  uint8_t *reg = NULL;
  uint8_t code = cpu->mem[cpu->pc++];

  switch (code) {
  case 0xA9:
  case 0xA5:
  case 0xB5:
  case 0xA1:
  case 0xB1:
  case 0xAD:
  case 0xBD:
  case 0xB9: reg = &cpu->a; break;
  case 0xA2:
  case 0xA6:
  case 0xB6:
  case 0xAE:
  case 0xBE: reg = &cpu->x; break;
  case 0xA0:
  case 0xA4:
  case 0xB4:
  case 0xAC:
  case 0xBC: reg = &cpu->y; break;
  }

  uint8_t value = cpu->mem[adr_fetch(mode, cpu)];
  assert(reg);

  *reg = value;
  if (value & 0x80)
    cpu->ps |= PS_N;

  cpu->ps = (value == 0 ?
             cpu->ps | PS_Z :
             cpu->ps & ~PS_Z);
}

void op_adc(cpu_state_t *cpu, uint8_t mode) {
  cpu->pc++;
  uint16_t adr = adr_fetch(mode, cpu);
  uint16_t result = cpu->a + cpu->mem[adr];
  cpu->a = (uint8_t)(result & 0xFF);

  if (result > 0xFF)
    cpu->ps |= (PS_C|PS_V);

  if (cpu->a & 0x80)
    cpu->ps |= PS_N;

  if (cpu->a == 0)
    cpu->ps |= PS_Z;
  else
    cpu->ps &= ~PS_Z;
}

void op_st(cpu_state_t *cpu, uint8_t mode) {
  uint8_t code = cpu->mem[cpu->pc++];
  uint16_t adr = adr_fetch(mode, cpu);

  uint8_t* reg = NULL;
  switch (code) {
  case 0x85:
  case 0x95:
  case 0x81:
  case 0x91:
  case 0x8D:
  case 0x9D:
  case 0x99: reg = &cpu->a; break;
  case 0x86:
  case 0x96:
  case 0x8E: reg = &cpu->x; break;
  case 0x84:
  case 0x94:
  case 0x8C: reg = &cpu->y; break;
  }

  assert(reg);
  cpu->mem[adr] = *reg;
}

void op_trans(cpu_state_t *cpu, uint8_t mode) {
  uint8_t code = cpu->mem[cpu->pc++];
  uint8_t *source, *dest;

  switch (code) {
  case 0xAA: source = &cpu->a;  dest = &cpu->x;  break;
  case 0x8A: source = &cpu->x;  dest = &cpu->a;  break;
  case 0xA8: source = &cpu->a;  dest = &cpu->y;  break;
  case 0x98: source = &cpu->y;  dest = &cpu->a;  break;
  case 0xBA: source = &cpu->sp; dest = &cpu->x;  break;
  case 0x9A: source = &cpu->x;  dest = &cpu->sp; break;
  default: assert(!"strange opcode");
  }

  *dest = *source;
  if (*dest & 0x80)
    cpu->ps |= PS_N;
  cpu->ps = (*dest == 0 ?
             cpu->ps | PS_Z :
             cpu->ps & ~PS_Z);
}

void op_push(cpu_state_t *cpu, uint8_t mode) {
  uint8_t code = cpu->mem[cpu->pc++];
  uint8_t *source;
  switch (code) {
  case 0x48: source = &cpu->a; break;
  case 0x08: source = &cpu->ps; break;
  default: assert(!"invalid opcode");
  }

  PUSH8(cpu, *source);
}

void op_pull(cpu_state_t *cpu, uint8_t mode) {
  uint8_t code = cpu->mem[cpu->pc++];
  uint8_t *dest;
  switch (code) {
  case 0x68: dest = &cpu->a; break;
  case 0x28: dest = &cpu->ps; break;
  default: assert(!"invalid opcode");
  }

  *dest = POP8(cpu);
  if (code == 0x68) {
    if (*dest & 0x80)
      cpu->ps |= PS_N;
    cpu->ps = (*dest == 0 ?
               cpu->ps | PS_Z :
               cpu->ps & ~PS_Z);
  }

}

void op_tog_i(cpu_state_t *cpu, uint8_t mode) {
  switch (cpu->mem[cpu->pc++]) {
  case 0x58: cpu->ps &= ~PS_I; break; /* CLI */
  case 0x78: cpu->ps |= PS_I; break; /* SEI */
  default: assert(!"invalid opcode");
  }
}

void op_tog_d(cpu_state_t *cpu, uint8_t mode) {
  switch (cpu->mem[cpu->pc++]) {
  case 0xD8: cpu->ps &= ~PS_D; break; /* CLD */
  case 0xF8: cpu->ps |= PS_D; break; /* SED */
  default: assert(!"invalid opcode");
  }
}

void op_tog_c(cpu_state_t *cpu, uint8_t mode) {
  switch (cpu->mem[cpu->pc++]) {
  case 0x18: cpu->ps &= ~PS_C; break; /* CLC */
  case 0x38: cpu->ps |= PS_C; break; /* SEC */
  default: assert(!"invalid opcode");
  }
}

void op_clv(cpu_state_t *cpu, uint8_t mode) {
  (void)cpu;
  (void)mode;
  cpu->pc++;
  cpu->ps &= ~PS_V;
}

void op_nop(cpu_state_t *cpu, uint8_t mode) {
  cpu->pc++;
}

void op_jmp(cpu_state_t *cpu, uint8_t mode) {
  cpu->pc++;
  cpu->pc = cpu->mem[adr_fetch(mode, cpu)];
}

void op_rts(cpu_state_t *cpu, uint8_t mode) {
  uint16_t retadr = POP16(cpu);
  cpu->pc = retadr + 1;
}

void op_jsr(cpu_state_t *cpu, uint8_t mode) {
  uint16_t pc = cpu->pc + 2;
  cpu->pc++;
  uint16_t toadr = adr_fetch(mode, cpu);
  PUSH16(cpu, pc);
  cpu->pc = toadr;
}

void op_ora(cpu_state_t *cpu, uint8_t mode) {
  cpu->pc++;
  uint16_t adr = adr_fetch(mode, cpu);
  cpu->a |= cpu->mem[adr];

  if (cpu->a & 0x80)
    cpu->ps |= PS_N;

  cpu->ps = (cpu->a == 0 ?
             cpu->ps | PS_Z :
             cpu->ps & ~PS_Z);
}

void op_and(cpu_state_t *cpu, uint8_t mode) {
  cpu->pc++;
  uint16_t adr = adr_fetch(mode, cpu);
  cpu->a &= cpu->mem[adr];

  if (cpu->a & 0x80)
    cpu->ps |= PS_N;

  cpu->ps = (cpu->a == 0 ?
             cpu->ps | PS_Z :
             cpu->ps & ~PS_Z);
}

void op_eor(cpu_state_t *cpu, uint8_t mode) {
  cpu->pc++;
  uint16_t adr = adr_fetch(mode, cpu);
  cpu->a ^= cpu->mem[adr];

  if (cpu->a & 0x80)
    cpu->ps |= PS_N;

  cpu->ps = (cpu->a == 0 ?
             cpu->ps | PS_Z :
             cpu->ps & ~PS_Z);
}

void op_sbc(cpu_state_t *cpu, uint8_t mode) {
  cpu->pc++;
  uint16_t adr = adr_fetch(mode, cpu);
  /* TODO: fix borrow, carry flag */
  cpu->a -= cpu->mem[adr];

  if (cpu->a & 0x80)
    cpu->ps |= PS_N;

  /* TODO: fix proper flag setting */
  cpu->ps = (cpu->a == 0 ?
             cpu->ps | PS_Z :
             cpu->ps & ~PS_Z);
}

void op_cmp(cpu_state_t *cpu, uint8_t mode) {
  cpu->pc++;
  uint16_t adr = adr_fetch(mode, cpu);
  /* TODO: fix borrow, carry flag */
  uint8_t res = cpu->a - cpu->mem[adr];

  if (res & 0x80)
    cpu->ps |= PS_N;

  /* TODO: fix setting of carry */
  cpu->ps = (res == 0 ?
             cpu->ps | PS_Z :
             cpu->ps & ~PS_Z);
}

void op_dec(cpu_state_t *cpu, uint8_t mode) {
  uint8_t code = cpu->mem[cpu->pc++];
  uint8_t res;

  switch (code) {
  case 0xCA: res = --cpu->x; break;
  case 0x88: res = --cpu->y; break;
  default: res = --cpu->mem[adr_fetch(mode, cpu)];
  }

  if (res & 0x80)
    cpu->ps |= PS_N;

  cpu->ps = (res == 0 ?
             cpu->ps | PS_Z :
             cpu->ps & ~PS_Z);
}

void op_inc(cpu_state_t *cpu, uint8_t mode) {
  uint8_t code = cpu->mem[cpu->pc++];
  uint8_t res;

  switch (code) {
  case 0xE8: res = ++cpu->x; break;
  case 0xC8: res = ++cpu->y; break;
  default: res = ++cpu->mem[adr_fetch(mode, cpu)];
  }

  if (res & 0x80)
    cpu->ps |= PS_N;

  cpu->ps = (res == 0 ?
             cpu->ps | PS_Z :
             cpu->ps & ~PS_Z);
}

void op_br(cpu_state_t *cpu, uint8_t mode) {
  uint8_t code = cpu->mem[cpu->pc++];
  int exec_br = 0;

  switch (code) {
  case 0x10: exec_br = ~cpu->ps & PS_N; break; /* bpl */
  case 0x30: exec_br = cpu->ps & PS_N; break; /* bmi */
  case 0x50: exec_br = ~cpu->ps & PS_V; break; /* bvc */
  case 0x70: exec_br = cpu->ps & PS_V; break; /* bvs */
  case 0x90: exec_br = ~cpu->ps & PS_C; break; /* bcc */
  case 0xB0: exec_br = cpu->ps & PS_C; break; /* bcs */
  case 0xD0: exec_br = ~cpu->ps & PS_Z; break; /* bne */
  case 0xF0: exec_br = cpu->ps & PS_Z; break; /* beq */
  default: assert(!"invalid opcode");
  }

  if (exec_br)
    cpu->pc = adr_fetch(mode, cpu);
}

void print_state(cpu_state_t *state) {
  uint8_t ps = state->ps;

  printf("a=%02X x=%02X y=%02X sp=%02X pc=%04X ps=%c%c-%c%c%c%c%c (%02X)\n",
         state->a, state->x, state->y, state->sp, state->pc,
         (ps & PS_N) ? 'N' : 'x',
         (ps & PS_V) ? 'V' : 'x',
         (ps & PS_B) ? 'B' : 'x',
         (ps & PS_D) ? 'D' : 'x',
         (ps & PS_I) ? 'I' : 'x',
         (ps & PS_Z) ? 'Z' : 'x',
         (ps & PS_C) ? 'C' : 'x',
         ps
         );

  size_t i;
  for (i = 0; i < 0x400; ) {
    printf("%04X: ", i);
    size_t a = i + 0x20;
    for (; i < a; ++i)
      printf("%02X ", state->mem[i]);
    printf("\n");
  }
}

void print_instr(cpu_state_t *cpu, uint16_t len) {
  /* prints the instructions at pc, up to len bytes */
  uint16_t end = cpu->pc + len;
  while (cpu->pc < end) {
    uint16_t pc = cpu->pc;
    uint8_t code = cpu->mem[cpu->pc++];
    opc_descr_t *opcode = &opcodes[code];
    char instr[64] = {0};
    char data[64] = {0};
    size_t num_bytes = 1;

    sprintf(data, ".%04X  ", pc);

    if (opcode->cfun) {
      uint16_t val;
      uint8_t hi;

      switch (opcode->addr_m) {
      case ADR_IMM:
        val = cpu->mem[cpu->pc++];
        num_bytes = 2;
        sprintf(instr, "%s #$%02X", opcode->name, val);
        break;

      case ADR_ABS:
        num_bytes = 3;
        hi = cpu->mem[cpu->pc++];
        val = (uint16_t)hi << 8 | cpu->mem[cpu->pc++];
        sprintf(instr, "%s $%04X", opcode->name, val);
        break;

      default:
        strcpy(instr, opcode->name);
      }
    }

    while (num_bytes--) {
      sprintf(data + strlen(data), "%02X ", cpu->mem[pc++]);
    }

    printf("%-16s %s\n", data, instr);
  }
}

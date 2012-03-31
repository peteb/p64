#include <stdint.h>
#include <assert.h>
#include <string.h>
#include <stdio.h>

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

#define MEM_MAX 0x4000

typedef struct cpu_state {
  uint8_t a, x, y, ps, sp;
  uint16_t pc;
  uint8_t mem[MEM_MAX];
} cpu_state_t;

void print_state(cpu_state_t *);
void print_mem(cpu_state_t *);
uint16_t adr_fetch(uint8_t mode, cpu_state_t *cpu);
void print_instr(cpu_state_t *, uint16_t);

typedef void (*opcode_handler_fun_t)(cpu_state_t *, uint8_t);

typedef struct opc_descr_t {
  uint8_t addr_m;
  opcode_handler_fun_t cfun;
  const char *name;
} opc_descr_t;

void op_ld(cpu_state_t *, uint8_t);
void op_adc(cpu_state_t *, uint8_t);
void op_tog_i(cpu_state_t *, uint8_t);
void op_tog_d(cpu_state_t *, uint8_t);
void op_tog_c(cpu_state_t *, uint8_t);
void op_clv(cpu_state_t *, uint8_t);
void op_nop(cpu_state_t *, uint8_t);
void op_st(cpu_state_t *, uint8_t);
void op_trans(cpu_state_t *, uint8_t);
void op_push(cpu_state_t *, uint8_t);
void op_pull(cpu_state_t *, uint8_t);
void op_jmp(cpu_state_t *, uint8_t);
void op_rts(cpu_state_t *, uint8_t);
void op_jsr(cpu_state_t *, uint8_t);

void run_machine(cpu_state_t *);

static opc_descr_t opcodes[256] = {
  /* jump/flag */
  [0x18] = {ADR_IMP, op_tog_c, "clc"},
  [0x38] = {ADR_IMP, op_tog_c, "sec"},
  [0x58] = {ADR_IMP, op_tog_i, "cli"},
  [0x78] = {ADR_IMP, op_tog_i, "sei"},
  [0xD8] = {ADR_IMP, op_tog_d, "cld"},
  [0xF8] = {ADR_IMP, op_tog_d, "sed"},
  [0xB8] = {ADR_IMP, op_clv, "clv"},
  [0xEA] = {ADR_IMP, op_nop, "nop"},
  [0x60] = {ADR_IMP, op_rts, "rts"},
  [0x4C] = {ADR_ABS, op_jmp, "jmp"},
  [0x6C] = {ADR_IND, op_jmp, "jmp"},
  [0x20] = {ADR_ABS, op_jsr, "jsr"},

  /* ld */
  [0xA9] = {ADR_IMM, op_ld, "lda"},
  [0xA2] = {ADR_IMM, op_ld, "ldx"},
  [0xA0] = {ADR_IMM, op_ld, "ldy"},
  [0xA5] = {ADR_ZP, op_ld, "lda"},
  [0xA6] = {ADR_ZP, op_ld, "ldx"},
  [0xA4] = {ADR_ZP, op_ld, "ldy"},
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
  [0x85] = {ADR_ZP, op_st, "sta"},
  [0x86] = {ADR_ZP, op_st, "stx"},
  [0x84] = {ADR_ZP, op_st, "sty"},
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

  /* arithmetic */
  [0x69] = {ADR_IMM, op_adc, "adc"}
};

int main() {
  static cpu_state_t cpu = {
    0, 0, 0,
    .ps = 0x20,
    .sp = 0xFF,
    .mem = {0xA9, 0xFA, 0x69, 0x06, 0x08, 0x18, 0x28, 0xBA,  /* 8 */
            0x8E, 0x13, 0x37, 0xEA, 0xEA, 0x20, 0x00, 0x18,  /* 16 */
            0xEA, 0xEA, 0xEA, 0xEA,    0,    0,    0,    0,     /* 24 */
            0x18, 0x60,    0,    0,    0}
  };

  run_machine(&cpu);
  print_state(&cpu);

  cpu.pc = 0;
  print_instr(&cpu, 40);
  return 0;
}

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
  uint16_t result = cpu->a + cpu->mem[cpu->pc++];
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

  cpu->mem[0x100 + (cpu->sp--)] = *source;
}

void op_pull(cpu_state_t *cpu, uint8_t mode) {
  uint8_t code = cpu->mem[cpu->pc++];
  uint8_t *dest;
  switch (code) {
  case 0x68: dest = &cpu->a; break;
  case 0x28: dest = &cpu->ps; break;
  default: assert(!"invalid opcode");
  }

  *dest = cpu->mem[0x100 + (++cpu->sp)];
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
  uint16_t retadr = (uint16_t)cpu->mem[0x100 + (++cpu->sp)] << 8;
  retadr |= cpu->mem[0x100 + (++cpu->sp)];
  cpu->pc = retadr + 1;
}

void op_jsr(cpu_state_t *cpu, uint8_t mode) {
  uint16_t pc = cpu->pc + 2;
  cpu->pc++;
  uint16_t toadr = adr_fetch(mode, cpu);
  cpu->mem[0x100 + (cpu->sp--)] = pc & 0xFF;
  cpu->mem[0x100 + (cpu->sp--)] = pc >> 8;
  cpu->pc = toadr;
}


void print_state(cpu_state_t* state) {
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

void print_mem(cpu_state_t* cpu) {

}

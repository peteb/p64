#include "6502.h"
#include "asm.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <assert.h>

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

void clear_symtab(struct symtab *syms) {
  size_t i;
  for (i = 0; i < syms->num_symbols; ++i)
    free(syms->symbols[i].id);

  syms->num_symbols = 0;
}

sym_def_t *sym_lookup(struct symtab *syms, const char *name) {
  size_t i;
  for (i = 0; i < syms->num_symbols; ++i) {
    if (strcmp(syms->symbols[i].id, name) == 0)
      return &syms->symbols[i];
  }

  return NULL;
}

void parse_asm(const char *, struct cpu_state *, struct symtab *);

void repl() {
  static cpu_state_t cpu;
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
  print_instr(cpu.mem + 0x100, 40, 0x100);
  clear_symtab(&symbols);
}

int main() {
  static cpu_state_t cpu = {
    .ps = 0x20,
    .sp = 0xFF,
    .mem = {0xA9, 0xFA, 0x69, 0x06, 0x08, 0x18, 0x28, 0xBA,  /* 8 */
            0x8E, 0x13, 0x37, 0xEA, 0xEA, 0x20, 0x00, 0x18,  /* 16 */
            0xA2, 0x02, 0xB4, 0x01,    0,    0,    0,    0,     /* 24 */
            0x18, 0xA9, 0x02, 0x09, 0x08, 0x60,    0,    0,    0}
  };

  /* run_machine(&cpu); */
  /* print_state(&cpu); */

  /* cpu.pc = 0; */
  /* print_instr(cpu.mem, 40, 0x0); */

  static const char code[] =
    "org $100\n"
    "     php\n"
    "main lda #$FA            ; start of the stuff\n"
    "     adc #$06\n"
    "     php\n"
    "     clc                 ; clear carry\n"
    "     plp\n"
    "     tsx\n"
    "     stx $1337\n"
    "     jmp main            ; loop back\n";

  static symtab_t object;

  parse_asm(code, &cpu, &object);
  print_state(&cpu);
  print_instr(cpu.mem + 0x100, 40, 0x100);
  /*  repl(); */
  return 0;
}

int isws(char c) {
  return (c == ' ' || c == '\t' || c == '\r' || c == '\n');
}

int isnum(char c) {
  return (c >= '0' && c <= '9');
}

int isnotnl(char c) {
  return (c != '\n');
}

int ischars(char c) {
  return (!isws(c) && c != ';');
}

void eat_while(const char **p, int (*pred)(char)) {
  const char *pos = *p;
  while (*pos) {
    if (pred(*pos))
      pos++;
    else
      break;
  }

  *p = pos;
}

int parse_value(const char *text, uint8_t mode, uint16_t *res) {
  long val;
  char *end = 0;

  switch (mode) {
  case ADR_ABS:  /* 12342, $123F */
    if (text[0] == '$')
      val = strtol(text + 1, &end, 16);
    else
      val = strtol(text, &end, 10);
    if (end == text)
      return 0;
    *res = val;
    return 2;

  case ADR_IMM:  /* #255, #$FA */
    if (text[0] != '#')
      return 0;

    if (text[1] == '$')
      val = strtol(text + 2, &end, 16);
    else
      val = strtol(text + 1, &end, 10);
    if (end == text)
      return 0;
    *res = val;
    return 1;

  }

  return 0;
}


void parse_line(const char *grps[], size_t num_grps,
                struct cpu_state *cpu, struct symtab *sym) {
  /* 1: 3 label instr/keyword param*/
  /* 2: 2 label instr/keyword*/
  /* 3: 2 instr/keyword param */
  /* 4: 1 instr/keyword */
  /* 5: 1 label */

  uint16_t modes = 0;
  size_t instr_start = 0;

  if (num_grps > 0)
    modes = instr_modes(grps[0]);

  if (modes == 0) {
    /* first group isn't a valid instruction, maybe it's a keyword */

    if (strcmp(grps[0], "org") == 0) {
      assert(num_grps == 2);
      uint16_t val;
      if (parse_value(grps[1], ADR_ABS, &val)) {
        printf("jumped to $0x%04X\n", val);
        cpu->pc = val;
      }
      else {
        fprintf(stderr,
                "error: origin takes an absolute address - not %s\n", grps[1]);
        return;
      }
    }
    else {
      assert(sym->num_symbols < 100);
      uint8_t flags = 0;
      char sym_name[64] = {0};
      strncpy(sym_name, grps[0], sizeof(sym_name));
      size_t len = strlen(sym_name);
      if (sym_name[len - 1] == ':') {
        sym_name[len - 1] = '\0';
        flags |= SYM_GLOBAL;
      }

      if (sym_lookup(sym, sym_name)) {
        fprintf(stderr,
                "error: symbol '%s' already defined\n", sym_name);
        return;
      }

      printf("defining '%s' as $%04X\n", grps[0], cpu->pc);

      sym_def_t *symdef = &sym->symbols[sym->num_symbols++];
      symdef->id = strdup(sym_name);
      symdef->address = cpu->pc;
      symdef->flags = flags;

      if (num_grps > 1) {
        instr_start = 1;
        modes = instr_modes(grps[1]);
      }
    }
  }

  if (modes) {
    /* there's a valid instruction in there */
    if (modes == (1 << ADR_IMP) && (num_grps - instr_start) != 1) {
      fprintf(stderr,
              "error: instruction '%s' doesn't accept parameters (%zu were given)\n",
              grps[instr_start], num_grps - instr_start - 1);
      return;
    }
    else {
      /* try to parse instr_start+1 as any of the addressing modes in modes */
      if (!(modes & (1 << ADR_IMP)) && num_grps - instr_start == 1) {
        fprintf(stderr,
                "error: instruction '%s' takes parameters, none were given\n",
                grps[instr_start]);
        return;
      }

      /* TODO: all these instr_modes and instr_named calls are doing more work
       * than necessary. */

      /* TODO: the special handling of ADR_IMP here is ugly. should be moved
         into parse_value instead, so that it takes a list. */

      if (modes & (1 << ADR_IMP)) {
        printf("one-line %s\n", grps[instr_start]);
        uint8_t op = instr_named(grps[instr_start], ADR_IMP);
        assert(op);
        cpu->mem[cpu->pc++] = op;
      }
      else {
        size_t i;
        for (i = 0; i < ADR_MAX; ++i) {
          uint16_t val;
          int bytes;
          if ((modes & (1 << i)) && (bytes = parse_value(grps[instr_start + 1], i, &val))) {
            printf("%s with %s means mode %zu\n", grps[instr_start], grps[instr_start + 1], i);
            uint8_t op = instr_named(grps[instr_start], i);
            assert(op);
            cpu->mem[cpu->pc++] = op;
            if (bytes == 1) {
              cpu->mem[cpu->pc++] = val;
            }
            else if (bytes == 2) {
              cpu->mem[cpu->pc++] = val >> 8;
              cpu->mem[cpu->pc++] = val;
            }
            else {
              assert(!"some strange amount of bytes");
            }


            /* TODO: skriv ner värdet i minnet också. det kan ju vara både
               en byte och två bytes */
            break;
          }
        }

        if (i == ADR_MAX) {
          fprintf(stderr,
                  "error: the value given for '%s', %s, doesn't really work for me\n",
                  grps[instr_start],
                  grps[instr_start + 1]);
          return;
        }
      }
    }
  }

}


void parse_asm(const char *text, struct cpu_state *cpu, struct symtab *sym) {
  const char *pos = text;
  char value[64] = {0};
  char *val_ptr = value;
  const char *val_grps[8] = {0};
  size_t num_grps = 0;
  
  while (*pos) {
    eat_while(&pos, isws);
    const char *chars_start = pos;
    eat_while(&pos, ischars);
    size_t diff = pos - chars_start;

    if (diff) {
      if (val_ptr + diff >= value + sizeof value ||
          num_grps >= sizeof val_grps / sizeof val_grps[0]) {
        fprintf(stderr, "error: we ran out of space");
        abort();
      }

      strncpy(val_ptr, chars_start, diff);
      assert(num_grps < 8);
      val_grps[num_grps++] = val_ptr;
      val_ptr += diff;
      *val_ptr++ = '\0';
    }

    if (*pos == ';') {
      eat_while(&pos, isnotnl);
    }

    if (*pos == '\n') {
      parse_line(val_grps, num_grps, cpu, sym);
      num_grps = 0;
      val_ptr = value;
    }


  }
}

#include "asm.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <assert.h>


void sym_clear(struct symtab *syms) {
  size_t i;
  for (i = 0; i < syms->num_symbols; ++i)
    free(syms->symbols[i].id);

  syms->num_symbols = 0;
}

static sym_def_t *sym_lookup(struct symtab *syms, const char *name) {
  size_t i;
  for (i = 0; i < syms->num_symbols; ++i) {
    if (strcmp(syms->symbols[i].id, name) == 0)
      return &syms->symbols[i];
  }

  return NULL;
}

void print_instr(uint8_t *mem, uint16_t len, uint16_t start_ofs) {
  /* prints the instructions at pc, up to len bytes */
  uint16_t end = len + start_ofs;
  uint16_t pc = start_ofs;
  
  while (pc < end) {
    uint16_t instr_pc = pc;
    uint8_t code = mem[pc++];
    opc_descr_t *opcode = instr_descr(code);
    char instr[64] = {0};
    char data[64] = {0};
    size_t num_bytes = 1;
    const char *extra = "";
    
    sprintf(data, ".%04X  ", instr_pc);

    if (opcode->cfun) {
      uint16_t val;
      uint8_t hi;

      switch (opcode->addr_m) {
      case ADR_IMM:
        val = mem[pc++];
        num_bytes = 2;
        sprintf(instr, "%s #$%02X", opcode->name, val);
        break;

      case ADR_ABX:   extra = ",X";
      case ADR_ABY:   if (!*extra) extra = ",Y";
      case ADR_ABS:
        num_bytes = 3;
        hi = mem[pc++];
        val = (uint16_t)hi << 8 | mem[pc++];
        sprintf(instr, "%s $%04X%s", opcode->name, val, extra);
        break;

      case ADR_ZPX:   extra = ",X";
      case ADR_ZPY:   if (!*extra) extra = ",Y";
      case ADR_ZP:
        val = mem[pc++];
        num_bytes = 2;
        sprintf(instr, "%s $%02X%s", opcode->name, val, extra);
        break;

      case ADR_IZX:   extra = ",X)";
      case ADR_IZY:   if (!*extra) extra = "),Y";
        val = mem[pc++];
        num_bytes = 2;
        sprintf(instr, "%s ($%02X%s", opcode->name, val, extra);
        break;

      case ADR_IND:
        num_bytes = 3;
        hi = mem[pc++];
        val = (uint16_t)hi << 8 | mem[pc++];
        sprintf(instr, "%s ($%04X)", opcode->name, val);
        break;

      case ADR_REL:
        num_bytes = 3;
        hi = mem[pc++];
        val = (uint16_t)hi << 8 | mem[pc++];
        sprintf(instr, "%s $%04X,PC", opcode->name, val);
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

static int isnotnl(int c) {return (c != '\n');}
static int ischars(int c) {return (!isspace(c) && c != ';' && c != '\n');}

static void eat_while(const char **p, int (*pred)(int)) {
  const char *pos = *p;
  while (*pos && (*p = pos, pred(*pos++)));
}

static int parse_value(const char *text, uint8_t mode,
                       uint16_t *res, struct symtab *symbols) {
  /* if text == NULL, there is no value */
  if (mode != ADR_IMP && !text)
    return -1;
  
  long val;
  char *end = 0;

  switch (mode) {
  case ADR_IMP:
    return 0;

  case ADR_ABS:  /* 12342, $123F, LABEL */
    if (text[0] == '$')
      val = strtol(text + 1, &end, 16);
    else
      val = strtol(text, &end, 10);

    if (end != (text[0] == '$' ? text + 1 : text)) {
      *res = val;
      return 2;
    }

    /* maybe it's a label, let's try it */

    if (text[0] == '$')
      return -1;

    while (*end) {
      if (!isalpha(*end))
        return -1;
      end++;
    }
    /* note: we've ruined 'end' here */

    sym_def_t *symbol = sym_lookup(symbols, text);
    if (!symbol) {
      fprintf(stderr,
              "error: symbol '%s' not defined\n", text);
      return -1;
    }
    *res = symbol->address;
    return 2;    

  case ADR_IMM:  /* #255, #$FA */
    if (text[0] != '#')
      return -1;

    if (text[1] == '$')
      val = strtol(text + 2, &end, 16);
    else
      val = strtol(text + 1, &end, 10);

    if (end == (text[1] == '$' ? text + 2 : text + 1))
      return -1;

    if (val > 0xFF)
      return -1;

    *res = val;
    return 1;

    /* TODO: the rest of the modes */

  }

  return -1;
}


static void parse_line(const char *grps[], size_t num_grps,
                       struct cpu_state *cpu, struct symtab *sym) {
  /* 1: 3 label instr/keyword param*/
  /* 2: 2 label instr/keyword*/
  /* 3: 2 instr/keyword param */
  /* 4: 1 instr/keyword */
  /* 5: 1 label */

  uint16_t modes = 0;
  size_t instr_start = 0;

  modes = instr_modes(grps[0]);

  if (modes == 0) {
    /* first group isn't a valid instruction, maybe it's a keyword */

    if (strcmp(grps[0], "org") == 0) {
      assert(num_grps == 2);
      uint16_t val;
      if (parse_value(grps[1], ADR_ABS, &val, sym) != -1) {
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
      assert(sym->num_symbols < 100 && "can't define any more symbols");
      uint8_t flags = 0;
      char sym_name[64] = {0};
      strncpy(sym_name, grps[0], sizeof sym_name - 1);
      sym_name[sizeof sym_name - 1] = '\0';
      
      size_t len = strlen(sym_name);
      if (len > 0 && sym_name[len - 1] == ':') {
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
        grps++; /* grps will now point to the instruction */
      }
    }
  }

  if (modes) {
    /* there's a valid instruction in there */

    /* TODO: all these instr_modes and instr_named calls are doing more work
     * than necessary. */

    size_t i;
    for (i = 0; i < ADR_MAX; ++i) {
      uint16_t val;
      int bytes;
      const char *value = (num_grps - instr_start != 1 ?
                           grps[1] : NULL);
      if (modes & (1 << i) &&
          (bytes = parse_value(value, i, &val, sym)) != -1) {
        printf("%s with %s means mode %zu\n",
               grps[0], grps[1], i);

        uint8_t op = instr_named(grps[0], i);
        assert(op);
        cpu->mem[cpu->pc++] = op;

        switch (bytes) {
        case 2:  cpu->mem[cpu->pc++] = val >> 8;
        case 1:  cpu->mem[cpu->pc++] = val & 0xFF;
        case 0:  break;
        default: assert(!"some strange amount of bytes");            
        }

        break;
      }
    }

    if (i == ADR_MAX) {
      fprintf(stderr,
              "error: the value given for '%s', %s, doesn't really work for me\n",
              grps[0],
              grps[1]);
      return;
    }
  }

}


void parse_asm(const char *text, struct cpu_state *cpu, struct symtab *sym) {
  const char *pos = text;
  char value[64] = {0};
  char *val_ptr = value;
  const char *val_grps[8];
  size_t num_grps = 0;

  while (*pos) {
    eat_while(&pos, isspace);
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
      if (num_grps > 0)
        parse_line(val_grps, num_grps, cpu, sym);
      
      num_grps = 0;
      val_ptr = value;
      pos++;
    }
  }
}

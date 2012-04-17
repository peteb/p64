#include "prg.h"

#include <stdio.h>

int load_prg(cpu_state_t *cpu, const char *filename) {
    uint8_t buf[2];

    FILE *f = fopen(filename, "rb");
    if (f == NULL) {
        return -1;
    }

    size_t n;
    n = fread(buf, 1, 2, f);
    if (n != 2) {
        fclose(f);
        return -1;
    }
    uint16_t load_address = buf[0] | (buf[1]<<8);

    n = 1;
    while (n == 1) {
        uint8_t c;
        n = fread(&c, 1, 1, f);
        if (n == 1) {
            cpu->mem[load_address++] = c;
        }
    }
    fclose(f);
    return 0;
}


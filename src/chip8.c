#include <debug.h>
#include <limits.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#include "chip8.h"

bool chip8_exec(struct chip8 *chip8)
{
    const u16 op  = (chip8->Mem[chip8->PC] << 8) | (chip8->Mem[chip8->PC + 1]);

    const u16 nnn = op & 0x0FFF;
    const u8  kk  = op & 0x00FF;    
    const u8  n   = op & 0x000F;
    const u8  hi4 = (op & 0xF000) >> 12;
    const u8  x   = (op & 0x0F00) >> 8;
    const u8  y   = (op & 0x00F0) >> 4;

    bool key_pressed;
    int r, c, i, u, v;
    u8 new_pixels, new_pix, *cur_pix;

    if (op == 0x00E0) {
        dbg_sprintf(dbgout, "CLS\n");
        memset(chip8->FrmBuf, 0, sizeof(chip8->FrmBuf));
        chip8->PC += 2;
    } else if (op == 0x00EE) {
        dbg_sprintf(dbgout, "RET\n");
        chip8->PC = chip8->Stk[--chip8->SP];
    } else if (hi4 == 0x1) {
        dbg_sprintf(dbgout, "JP 0x%X\n", nnn);
        chip8->PC = nnn;
    } else if (hi4 == 0x2) {
        dbg_sprintf(dbgout, "CALL 0x%X\n", nnn);
        chip8->Stk[chip8->SP++] = chip8->PC + 2;
        chip8->PC = nnn;
    } else if (hi4 == 0x3) {
        dbg_sprintf(dbgout, "SE V%X, 0x%X\n", x, kk);
        if (chip8->V[x] == kk)
            chip8->PC += 2;
        chip8->PC += 2;
    } else if (hi4 == 0x4) {
        dbg_sprintf(dbgout, "SNE V%X, 0x%X\n", x, kk);
        if (chip8->V[x] != kk)
            chip8->PC += 2;
        chip8->PC += 2;
    } else if (hi4 == 0x5) {
        dbg_sprintf(dbgout, "SE V%X, V%X\n", x, y);
        if (chip8->V[x] == chip8->V[y])
            chip8->PC += 2;
        chip8->PC += 2;
    } else if (hi4 == 0x6) {
        dbg_sprintf(dbgout, "LD V%X, 0x%X\n", x, kk);
        chip8->V[x] = kk;
        chip8->PC += 2;
    } else if (hi4 == 0x7) {
        dbg_sprintf(dbgout, "ADD V%X, 0x%X\n", x, kk);
        chip8->V[x] += kk;
        chip8->PC += 2;
    } else if (hi4 == 0x8) {
        switch (n) {
        case 0:
            dbg_sprintf(dbgout, "LD V%X, V%X\n", x, y);
            chip8->V[x] = chip8->V[y];
            break;
        case 1:
            dbg_sprintf(dbgout, "OR V%X, V%X\n", x, y);
            chip8->V[x] |= chip8->V[y];
            break;
        case 2:
            dbg_sprintf(dbgout, "AND V%X, V%X\n", x, y);
            chip8->V[x] &= chip8->V[y];
            break;
        case 3:
            dbg_sprintf(dbgout, "XOR V%X, V%X\n", x, y);
            chip8->V[x] ^= chip8->V[y];
            break;
        case 4:
            dbg_sprintf(dbgout, "ADD V%X, V%X\n", x, y);
            i = chip8->V[x] + chip8->V[y];
            chip8->V[0xF] = (i > UCHAR_MAX) ? 1 : 0;
            chip8->V[x] = i;
            break;
        case 5:
            dbg_sprintf(dbgout, "SUB V%X, V%X\n", x, y);
            chip8->V[0xF] = (chip8->V[x] > chip8->V[y]) ? 1 : 0;
            chip8->V[x] -= chip8->V[y];
            break;
        case 6:
            dbg_sprintf(dbgout, "SHR V%X\n", x);
            chip8->V[0xF] = (chip8->V[x] & 0x01) ? 1 : 0;
            chip8->V[x] >>= 1;
            break;
        case 7:
            dbg_sprintf(dbgout, "SUBN V%X, V%X\n", x, y);
            chip8->V[0xF] = (chip8->V[y] > chip8->V[x]) ? 1 : 0;
            chip8->V[x] = chip8->V[y] - chip8->V[x];
            break;
        case 0xE:
            dbg_sprintf(dbgout, "SHL V%X\n", x, y);
            chip8->V[0xF] = (chip8->V[x] & 0x80) ? 1 : 0;
            chip8->V[x] <<= 1;
            break;
        default:
            return false;
        }
        chip8->PC += 2;
    } else if (hi4 == 0x9) {
        dbg_sprintf(dbgout, "SNE V%X, V%X\n", x, y);
        if (chip8->V[x] != chip8->V[y])
            chip8->PC += 2;
        chip8->PC += 2;
    } else if (hi4 == 0xA) {
        dbg_sprintf(dbgout, "LD I, 0x%X\n", nnn);
        chip8->I = nnn;
        chip8->PC += 2;
    } else if (hi4 == 0xB) {
        dbg_sprintf(dbgout, "JP V0, 0x%X\n", nnn);
        chip8->PC = nnn + chip8->V[0];
    } else if (hi4 == 0xC) {
        dbg_sprintf(dbgout, "RND V%X, 0x%X\n", x, kk);
        chip8->V[x] = (rand() % UCHAR_MAX) & kk;
        chip8->PC += 2;
    } else if (hi4 == 0xD) {
        dbg_sprintf(dbgout, "DRW V%X, V%X, 0x%X\n", x, y, n);
        chip8->V[0xF] = 0;
        for (r = 0; r < n; r++) {
            new_pixels = chip8->Mem[chip8->I + r];
            for (c = 0; c < CHIP8_SPRITEWIDTH; c++) {
                u = chip8->V[y] + r;
                v = chip8->V[x] + c;
                cur_pix = &chip8->FrmBuf[u % CHIP8_SCRHEIGHT][v % CHIP8_SCRWIDTH];
                new_pix = (new_pixels >> (CHIP8_SPRITEWIDTH - 1 - c)) & 0x01;
                if (new_pix && *cur_pix)
                    chip8->V[0xF] = 1;
                *cur_pix ^= new_pix;
            }
        }
        chip8->PC += 2;
    } else if (hi4 == 0xE) {
        switch (kk) {
        case 0x9E:
            dbg_sprintf(dbgout, "SKP V%X\n", x);
            if (chip8->Kbd[chip8->V[x]])
                chip8->PC += 2;
            break;
        case 0xA1:
            dbg_sprintf(dbgout, "SKNP V%X\n", x);
            if (!chip8->Kbd[chip8->V[x]])
                chip8->PC += 2;
            break;
        default:
            return false;
        }
        chip8->PC += 2;
    } else if (hi4 == 0xF) {
        switch (kk) {
        case 0x07:
            dbg_sprintf(dbgout, "LD V%X, DT\n", x);
            chip8->V[x] = chip8->DT;
            break;
        case 0x0A:
            dbg_sprintf(dbgout, "LD V%X, K\n", x);
            key_pressed = false;
            for (i = 0; i < CHIP8_KEYCOUNT; i++)
                if (chip8->Kbd[i]) {
                    key_pressed = true;
                    chip8->V[x] = i;
                    break;
                }
            if (!key_pressed)
                return true;
            break;
        case 0x15:
            dbg_sprintf(dbgout, "LD DT, V%X\n", x);
            chip8->DT = chip8->V[x];
            break;
        case 0x18:
            dbg_sprintf(dbgout, "LD ST, V%X\n", x);
            chip8->ST = chip8->V[x];
            break;
        case 0x1E:
            dbg_sprintf(dbgout, "ADD I, V%X\n", x);
            chip8->I += chip8->V[x];
            break;
        case 0x29:
            dbg_sprintf(dbgout, "LD F, V%X\n", x);
            if (chip8->V[x] > 0xF)
                return false;
            chip8->I = chip8->V[x] * CHIP8_FONTSIZE;
            break;
        case 0x33:
            dbg_sprintf(dbgout, "LD B, V%X\n", x);
            chip8->Mem[chip8->I]     = chip8->V[x] / 100;
            chip8->Mem[chip8->I + 1] = (chip8->V[x] / 10) % 10;
            chip8->Mem[chip8->I + 2] = chip8->V[x] % 10;
            break;
        case 0x55:
            dbg_sprintf(dbgout, "LD [I], V%X\n", x);
            for (i = 0; i <= x; i++)
                chip8->Mem[chip8->I + i] = chip8->V[i];
            break;
        case 0x65:
            dbg_sprintf(dbgout, "LD V%X, [I]\n", x);
            for (i = 0; i <= x; i++)
                chip8->V[i] = chip8->Mem[chip8->I + i];
            break;
        default:
            return false;
        }
        chip8->PC += 2;
    } else {
        return false;
    }

    return true;
}

bool chip8_init(struct chip8 *chip8)
{
    memset(chip8, 0, sizeof(struct chip8));
    memcpy(chip8->Mem, chip8_font, sizeof(chip8_font));

    chip8->PC = CHIP8_PROGRAMSTART;

    return true;
}

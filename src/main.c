#include <debug.h>
#include <fileioc.h>
#include <graphx.h>
#include <keypadc.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <tice.h>

#include "chip8.h"

#define CHIP8_BACKGROUNDCOLOR 0xBF
#define CHIP8_COLORBLACK      0
#define CHIP8_COLORWHITE      255
#define CHIP8_FRAMESPERSECOND 60
#define CHIP8_MAXTICKCOUNT    10000
#define CHIP8_PIXELHEIGHT     4
#define CHIP8_PIXELWIDTH      4
#define CHIP8_ROMNAMESIZE     20
#define CHIP8_SCRXOFFSET      32
#define CHIP8_SCRYOFFSET      20
#define CHIP8_TICKSPERFRAME   546

uint32_t prev_frame = 0;

void reset_timer1()
{
    timer_Control = TIMER1_DISABLE;
    timer_1_Counter = 0;
    timer_Control = TIMER1_ENABLE | TIMER1_32K | TIMER1_UP;
}

bool loop(struct chip8 *chip8)
{
    bool valid_opcode = true;
    float raw = (float) atomic_load_increasing_32(&timer_1_Counter);
    uint32_t this_frame = (uint32_t) (raw / CHIP8_TICKSPERFRAME);
    int frame_diff = this_frame - prev_frame;
    int r, c;

    chip8->DT -= ((chip8->DT - frame_diff) < 0) ? chip8->DT : frame_diff;
    chip8->ST -= ((chip8->ST - frame_diff) < 0) ? chip8->ST : frame_diff;

    kb_Scan();

    if (kb_IsDown(kb_Key2nd))
        return false;

    chip8->Kbd[0]   = kb_IsDown(kb_Key2);
    chip8->Kbd[1]   = kb_IsDown(kb_KeyComma);
    chip8->Kbd[2]   = kb_IsDown(kb_KeyLParen);
    chip8->Kbd[3]   = kb_IsDown(kb_KeyRParen);
    chip8->Kbd[4]   = kb_IsDown(kb_Key7);
    chip8->Kbd[5]   = kb_IsDown(kb_Key8);
    chip8->Kbd[6]   = kb_IsDown(kb_Key9);
    chip8->Kbd[7]   = kb_IsDown(kb_Key4);
    chip8->Kbd[8]   = kb_IsDown(kb_Key5);
    chip8->Kbd[9]   = kb_IsDown(kb_Key6);
    chip8->Kbd[0xA] = kb_IsDown(kb_Key1);
    chip8->Kbd[0xB] = kb_IsDown(kb_Key3);
    chip8->Kbd[0xC] = kb_IsDown(kb_KeyDiv);
    chip8->Kbd[0xD] = kb_IsDown(kb_KeyMul);
    chip8->Kbd[0xE] = kb_IsDown(kb_KeySub);
    chip8->Kbd[0xF] = kb_IsDown(kb_KeyAdd);

    valid_opcode = chip8_exec(chip8);
    if (!valid_opcode)
        return false;

    /* Reset the clock at (arbitrarily chosen) value to minimize frame count
       error due to raw variable's floating point imprecision.               */
    prev_frame = this_frame;
    if (this_frame > CHIP8_MAXTICKCOUNT)
    {
        prev_frame = 0;
        reset_timer1();
    }

    for (r = 0; r < CHIP8_SCRHEIGHT; r++)
        for (c = 0; c < CHIP8_SCRWIDTH; c++) {
            gfx_SetColor(chip8->FrmBuf[r][c] ? CHIP8_COLORWHITE : CHIP8_COLORBLACK);
            gfx_FillRectangle_NoClip(CHIP8_SCRXOFFSET + c * CHIP8_PIXELWIDTH,
                                     CHIP8_SCRYOFFSET + r * CHIP8_PIXELHEIGHT,
                                     CHIP8_PIXELWIDTH, CHIP8_PIXELHEIGHT);
        }

    gfx_SwapDraw();
    
    return true;
}

int main(void)
{
    bool exec_again = true;
    char rom_name[CHIP8_ROMNAMESIZE];
    int i, rom_size, chunks_read;
    struct chip8 *chip8 = malloc(sizeof(struct chip8));
    ti_var_t rom_slot; 

    ti_CloseAll();

    os_ClrHomeFull();
    os_GetStringInput("ROM AppVar Name: ", rom_name, CHIP8_ROMNAMESIZE);
    asm_NewLine();

    rom_slot = ti_Open(rom_name, "r");
    if (rom_slot == 0) {
        os_PutStrFull("Could not open AppVar.");
        while(!os_GetCSC());
        return EXIT_FAILURE;
    }

    rom_size = ti_GetSize(rom_slot);

    chip8_init(chip8);

    chunks_read = ti_Read(&chip8->Mem[CHIP8_PROGRAMSTART], rom_size, 1, rom_slot);
    if (chunks_read != 1) {
        os_PutStrFull("Could not read AppVar.");
        return EXIT_FAILURE;
    }
    
    gfx_Begin();

    gfx_SetMonospaceFont(8);
    gfx_SetTextFGColor(0);
    gfx_SetTextBGColor(255);
    gfx_SetDrawBuffer();

    /* Because swap draw is being used, must set the background color for both buffers. */
    for (i = 0; i < 2; i++) {
        gfx_SetColor(CHIP8_BACKGROUNDCOLOR);
        gfx_FillRectangle_NoClip(0, 0, LCD_WIDTH, LCD_HEIGHT);
        gfx_Rectangle_NoClip(CHIP8_SCRXOFFSET, CHIP8_SCRYOFFSET,
                             CHIP8_SCRWIDTH * CHIP8_PIXELWIDTH,
                             CHIP8_SCRHEIGHT * CHIP8_PIXELHEIGHT);
        gfx_SwapDraw();
    }

    reset_timer1();
    while (exec_again)
        exec_again = loop(chip8);

    gfx_End();
    free(chip8);

    os_ClrHomeFull();
    ti_CloseAll();

    return EXIT_SUCCESS;
}

/*
 * The Frontier Pass on the top screen, drawn natively over
 * the whole of it in the bottom screen's style instead of the game's
 * 240x160 picture. The bottom screen's code composes the picture
 * (3ds_bottom_pass.inc); this side keeps it in a texture and draws it
 * (3ds_pass_top.c, from 3ds_video.c's frame).
 */
#ifndef CTR_PASS_TOP_H
#define CTR_PASS_TOP_H

#include <stdbool.h>
#include <stdint.h>

#define CTR_PASS_TOP_WIDTH 400
#define CTR_PASS_TOP_HEIGHT 240

/* A new picture: RGB565 in the framebuffer's layout (column by column, each
 * from the bottom row up), CTR_PASS_TOP_WIDTH columns of CTR_PASS_TOP_HEIGHT. Kept
 * by pointer until the next frame has taken it in. */
void CtrPassTop_Show(const uint16_t *canvas);
/* The game's fade over it: its colour (GBA BGR555) and how far, 0..16. */
void CtrPassTop_SetFade(uint16_t colour, unsigned y);
void CtrPassTop_Hide(void);
bool CtrPassTop_Active(void);

/* 3ds_video.c: inside the frame, the picture into its texture; then drawn
 * over the top screen; its texture given back while it is not up. */
void CtrPassTop_Upload(void);
void CtrPassTop_Draw(void);
void CtrPassTop_Idle(void);

#endif

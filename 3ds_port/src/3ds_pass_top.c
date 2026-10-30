/*
 * The Frontier Pass on the top screen, video side
 * (3ds_pass_top.h): the picture the bottom screen's code composed, kept in
 * a texture and drawn over the whole top screen, the game's fade over it.
 *
 * As the Pokédex's device (3ds_dex_top.c), the texture is written only
 * inside the frame (CtrPassTop_Upload), once C3D_FrameBegin has waited for
 * the GPU to finish the last one, made when the pass is first drawn and
 * given back as soon as it is not (CtrPassTop_Idle).
 */
#include <3ds.h>
#include <citro2d.h>
#include <string.h>
#include "3ds_video.h"
#include "3ds_log.h"
#include "3ds_pass_top.h"

#define TEX_W 512
#define TEX_H 256
/* Frames to wait before trying again after linear memory ran short. */
#define TEX_RETRY_FRAMES 120

static bool sActive;
static const uint16_t *sCanvas;
static bool sDirty, sShown;
static uint16_t sFadeColour;
static unsigned sFadeY;

static C3D_Tex sTex;
static bool sTexAllocated, sTexFailLogged;
static unsigned sTexRetry;

void CtrPassTop_Show(const uint16_t *canvas)
{
    sActive = true;
    sCanvas = canvas;
    sDirty = true;
}

void CtrPassTop_SetFade(uint16_t colour, unsigned y)
{
    sFadeColour = colour;
    sFadeY = y > 16 ? 16 : y;
}

void CtrPassTop_Hide(void)
{
    sActive = false;
    sShown = false;
}

bool CtrPassTop_Active(void) { return sActive && sCanvas != NULL; }

static bool TextureReady(void)
{
    if (sTexAllocated) return true;
    if (sTexRetry != 0)
    {
        --sTexRetry;
        return false;
    }
    if (!C3D_TexInit(&sTex, TEX_W, TEX_H, GPU_RGB565))
    {
        sTexRetry = TEX_RETRY_FRAMES;
        if (!sTexFailLogged)
            CtrLog_Write(CTR_LOG_ERROR, "Frontier Pass top: no linear memory for its picture (free=%lu)",
                         (unsigned long)linearSpaceFree());
        sTexFailLogged = true;
        return false;
    }
    sTexFailLogged = false;
    C3D_TexSetFilter(&sTex, GPU_NEAREST, GPU_NEAREST);
    C3D_TexSetWrap(&sTex, GPU_CLAMP_TO_EDGE, GPU_CLAMP_TO_EDGE);
    sTexAllocated = true;
    sDirty = true;
    return true;
}

void CtrPassTop_Upload(void)
{
    if (!CtrPassTop_Active() || !TextureReady() || !sDirty) return;
    {
        /* An 8x8 block's texels in their Morton order (CtrVideo_Texel), as
         * the block's column and row. */
        static uint8_t order[64][2];
        static bool ordered;
        uint16_t *texels = sTex.data;

        if (!ordered)
        {
            for (unsigned y = 0; y < 8; ++y)
                for (unsigned x = 0; x < 8; ++x)
                {
                    unsigned m = CtrVideo_Texel(x, y, 8);

                    order[m][0] = (uint8_t)x;
                    order[m][1] = (uint8_t)y;
                }
            ordered = true;
        }
        /* The canvas runs down each column from the bottom row up. */
        for (unsigned ty = 0; ty < CTR_PASS_TOP_HEIGHT / 8; ++ty)
            for (unsigned tx = 0; tx < CTR_PASS_TOP_WIDTH / 8; ++tx)
            {
                uint16_t *block = texels + (ty * (TEX_W / 8) + tx) * 64;

                for (unsigned m = 0; m < 64; ++m)
                    block[m] = sCanvas[(tx * 8 + order[m][0]) * CTR_PASS_TOP_HEIGHT + CTR_PASS_TOP_HEIGHT - 1 - (ty * 8 + order[m][1])];
            }
    }
    C3D_TexFlush(&sTex);
    sDirty = false;
    sShown = true;
}

void CtrPassTop_Idle(void)
{
    sTexRetry = 0;
    if (!sTexAllocated) return;
    C3D_TexDelete(&sTex);
    sTexAllocated = false;
    sShown = false;
}

void CtrPassTop_Draw(void)
{
    const Tex3DS_SubTexture whole = {CTR_PASS_TOP_WIDTH, CTR_PASS_TOP_HEIGHT, 0.0f, 1.0f, CTR_PASS_TOP_WIDTH / (float)TEX_W,
                                     1.0f - CTR_PASS_TOP_HEIGHT / (float)TEX_H};

    if (!CtrPassTop_Active() || !sTexAllocated || !sShown) return;
    C2D_DrawImageAt((C2D_Image){&sTex, &whole}, 0, 0, 0, NULL, 1, 1);
    if (sFadeY != 0)
    {
        unsigned r = (sFadeColour & 31) * 255 / 31, g = ((sFadeColour >> 5) & 31) * 255 / 31,
                 b = ((sFadeColour >> 10) & 31) * 255 / 31;

        C2D_DrawRectSolid(0, 0, 0, CTR_PASS_TOP_WIDTH, CTR_PASS_TOP_HEIGHT, C2D_Color32(r, g, b, sFadeY * 255 / 16));
    }
}

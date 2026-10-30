/*
 * The Frontier Pass on the top screen (included by 3ds_bottom_ui.c): the
 * pass and its Frontier map drawn natively over the whole 400x240 top screen
 * in the bottom screen's style - its backdrop, plates, fonts and text styles
 * - instead of the game's 240x160 picture.
 *
 * The game still runs both screens and all their input (frontier_pass.c);
 * what is on show comes from it each frame (CtrPass_GetView). Its own art
 * the native screen keeps - the symbols, the hand cursor, the map and card
 * pictures, the Frontier map with its markers - is read from the game's
 * VRAM and palettes as it loaded them, never from the ROM image here, and
 * enlarged with a box filter (PassBlit). Texts are the game's strings; the
 * two it only has as pictures (the pass's title and its cancel button) are
 * CTR_TEXT labels.
 *
 * The game's cursor moves freely over the 240x160 picture and picks an area
 * by its tip (frontier_pass.c, sPassAreasLayout); the plates here stand
 * where those areas map to (PassX, PassY), so the tip drawn over a plate is
 * the area the game has chosen.
 *
 * The picture is composed into its own canvas, the bottom screen's
 * primitives pointed at it (PassTarget), and handed to the video side
 * (3ds_pass_top.c), which draws it and the game's fades over the top screen.
 * Everything but the cursor is composed again only when it changes; the
 * cursor alone goes over a copy of that.
 */

#include "3ds_pass_view.h"
#include "3ds_pass_top.h"
#include <stddef.h>
#include "gpu_regs.h"

#define PASS_W CTR_PASS_TOP_WIDTH
#define PASS_H CTR_PASS_TOP_HEIGHT
STATIC_ASSERT(PASS_H == H, passTopHeight);

/* The header row as the bottom screen's (3ds_bottom_shell.inc), across 400. */
#define PASS_TITLE_X 4
#define PASS_TITLE_W 244
#define PASS_CANCEL_X 254
#define PASS_CANCEL_W 142
/* The band along the bottom edge for the description, the backdrop's dark
 * strip there as the bottom screen's dock is. */
#define PASS_BAND_Y 204

/* The pass's columns: the symbols, record and points; the map and the card. */
#define PASS_LEFT_X 30
#define PASS_LEFT_W 152
#define PASS_RIGHT_X 220
#define PASS_RIGHT_W 148
#define PASS_SYMBOLS_Y 40
#define PASS_SYMBOLS_H 86
#define PASS_SYMBOLS_HEAD 22
#define PASS_RECORD_Y 130
#define PASS_RECORD_H 24
#define PASS_POINTS_Y 158
#define PASS_POINTS_H 40
#define PASS_MAP_Y 42
#define PASS_CARD_Y 122
#define PASS_THUMB_H 78
/* The map's and the card's pictures on the game's layer 1 (tiles 16,3 and
 * 16,10, 12x7 each) and their size here: 4/3. */
#define PASS_THUMB_GX 128
#define PASS_THUMB_MAP_GY 24
#define PASS_THUMB_CARD_GY 80
#define PASS_THUMB_GW 96
#define PASS_THUMB_GH 56
#define PASS_THUMB_W 128
#define PASS_THUMB_H_ART 75

/* The Frontier map: its picture on the game's layer 2 (in its frame), 1.5x
 * at (PASS_ART_X, PASS_ART_Y); the facilities' plates beside it. */
#define PASS_ART_GX 8
#define PASS_ART_GY 20
#define PASS_ART_GW 144
#define PASS_ART_GH 100
#define PASS_ART_X 10
#define PASS_ART_Y 44
#define PASS_NAMES_X 236
#define PASS_NAMES_W 160
#define PASS_NAMES_Y 42
#define PASS_NAME_H 20
#define PASS_NAME_STEP 22

/* A picture taken from the game: BGR555 with bit 15 set where it is drawn. */
#define PASS_OPAQUE 0x8000
typedef struct
{
    u8 w, h;
    u16 px[32 * 32];
} PassSprite;

static u16 sPassCanvas[PASS_W * PASS_H] __attribute__((aligned(32)));
/* The canvas without the cursor, and the backdrop it starts from. */
static u16 *sPassBase, *sPassBackdrop;
static u16 sPassThumbs[2][PASS_THUMB_GW * PASS_THUMB_GH];
static u16 sPassArt[PASS_ART_GW * PASS_ART_GH];

/* ------------------------------------------------------------------------ */
/* Where the game's areas are here                                          */
/* ------------------------------------------------------------------------ */

/*
 * A point of the game's 240x160 pass on the 400x240 screen: across, by
 * 5/3; down, piecewise, so each of the game's areas lands on its plate.
 * The two columns have their own rows (the left one's areas are the
 * symbols, the record and the points; the right one's the map and the
 * card), and between them, where the game has no area, one gives way to
 * the other. Above them both are the cancel button's row, the header.
 * The steps meet, so the cursor never jumps.
 */
typedef struct
{
    s16 game, screen;
} PassKnot;

static const PassKnot sPassRowsLeft[] = {
    {0, 12}, {16, 36}, {24, 40}, {48, 58}, {79, 120}, {80, 130}, {102, 154}, {108, 158}, {134, 198}, {160, 220},
};
static const PassKnot sPassRowsRight[] = {
    {0, 12}, {16, 36}, {28, 42}, {76, 118}, {84, 124}, {132, 198}, {160, 220},
};
/* The game's columns end and start here. */
#define PASS_LEFT_END 108
#define PASS_RIGHT_START 132

static int PassX(int x)
{
    return x * 5 / 3;
}

static int PassRow(const PassKnot *k, unsigned n, int y)
{
    unsigned i = 1;

    while (i < n - 1 && y > k[i].game)
        ++i;
    return k[i - 1].screen + (y - k[i - 1].game) * (k[i].screen - k[i - 1].screen) / (k[i].game - k[i - 1].game);
}

static int PassY(int x, int y)
{
    int l = PassRow(sPassRowsLeft, ARRAY_COUNT(sPassRowsLeft), y);
    int r = PassRow(sPassRowsRight, ARRAY_COUNT(sPassRowsRight), y);

    if (x <= PASS_LEFT_END)
        return l;
    if (x >= PASS_RIGHT_START)
        return r;
    return l + (r - l) * (x - PASS_LEFT_END) / (PASS_RIGHT_START - PASS_LEFT_END);
}

/* The symbols' cells, as frontier_pass.c lays them out (left, top). */
static const u8 sPassSymbolCells[CTR_PASS_FACILITIES][2] = {
    {20, 50}, {32, 66}, {44, 50}, {56, 66}, {68, 50}, {80, 66}, {92, 50},
};

/* ------------------------------------------------------------------------ */
/* Drawing                                                                  */
/* ------------------------------------------------------------------------ */

static inline u16 PassColour(u16 bgr)
{
    return PackRgb((bgr & 31) * 255 / 31, ((bgr >> 5) & 31) * 255 / 31, ((bgr >> 10) & 31) * 255 / 31);
}

/*
 * A picture from the game at (x, y), enlarged to w x h with a box filter:
 * every screen pixel the average of the game pixels it covers, weighted by
 * how much of it each covers, and as opaque as they cover it. At 2x that is
 * every pixel doubled; at 1.5x and 4/3 the pixels stay sharp but for a
 * blended row or column between some of them.
 */
static void PassBlit(const u16 *src, int sw, int sh, int x, int y, int w, int h, bool8 hflip)
{
    if (w <= 0 || h <= 0)
        return;
    for (int dx = 0; dx < w; ++dx)
    {
        int sx0 = dx * sw * 256 / w, sx1 = (dx + 1) * sw * 256 / w, cx = x + dx + sOX;

        if (cx < sClipX0 || cx >= sClipX1)
            continue;
        for (int dy = 0; dy < h; ++dy)
        {
            int sy0 = dy * sh * 256 / h, sy1 = (dy + 1) * sh * 256 / h, cy = y + dy;
            int area = (sx1 - sx0) * (sy1 - sy0), cover = 0, r = 0, g = 0, b = 0;

            if (cy < sClipY0 || cy >= sClipY1)
                continue;
            for (int sy = sy0 >> 8; sy * 256 < sy1; ++sy)
            {
                int wy = (sy * 256 + 256 < sy1 ? sy * 256 + 256 : sy1) - (sy * 256 > sy0 ? sy * 256 : sy0);

                for (int sx = sx0 >> 8; sx * 256 < sx1; ++sx)
                {
                    int wx = (sx * 256 + 256 < sx1 ? sx * 256 + 256 : sx1) - (sx * 256 > sx0 ? sx * 256 : sx0);
                    u16 p = src[sy * sw + (hflip ? sw - 1 - sx : sx)];
                    int k = wx * wy;

                    if (!(p & PASS_OPAQUE))
                        continue;
                    cover += k;
                    r += (p & 31) * k;
                    g += ((p >> 5) & 31) * k;
                    b += ((p >> 10) & 31) * k;
                }
            }
            if (cover == 0)
                continue;
            {
                u16 *dst = sDst + cx * H + (H - 1 - cy);
                int a = cover * 255 / area;
                Rgb c = {r / cover * 255 / 31, g / cover * 255 / 31, b / cover * 255 / 31};

                if (a >= 255)
                    *dst = PackRgb(c.r, c.g, c.b);
                else
                {
                    Rgb u = RgbOf(*dst);

                    *dst = PackRgb(Div255(u.r * (255 - a) + c.r * a), Div255(u.g * (255 - a) + c.g * a),
                                   Div255(u.b * (255 - a) + c.b * a));
                }
            }
        }
    }
}

static void PassSpriteAt(const PassSprite *s, int x, int y, int scale2, bool8 hflip)
{
    if (s->w)
        PassBlit(s->px, s->w, s->h, x, y, s->w * scale2 / 2, s->h * scale2 / 2, hflip);
}

/* The cursor, doubled, its tip at (tx, ty): the game's points down and
 * left from 3, 13 of its 16x16 (frontier_pass.c takes x - 5, y + 5 of its
 * middle), mirrored on the map. */
static void PassHand(const PassSprite *hand, int tx, int ty, bool8 hflip)
{
    if (hand->w)
        PassSpriteAt(hand, tx - 2 * (hflip ? hand->w - 1 - 3 : 3), ty - 2 * 13, 4, hflip);
}

/* The header: the title on the light plate, as every bottom screen's. */
static void PassHeader(const u8 *title, const u8 *cancel, bool8 cancelOn)
{
    int oy = PlateEx(PASS_TITLE_X, HEAD_Y, PASS_TITLE_W, HEAD_H, sLight, PLATE_STATE_NORMAL, PASS_TITLE_W - 10, 30,
                     PlateCut(HEAD_H), 0);

    TextIn(title, PASS_TITLE_X, oy, PASS_TITLE_W - 8, HEAD_H, &sNormal, 'l', STYLE_INK, 7);
    oy = Plate(PASS_CANCEL_X, HEAD_Y, PASS_CANCEL_W, HEAD_H, cancelOn ? sEmerald : sCancelGrey,
               cancelOn ? PLATE_STATE_FOCUS : PLATE_STATE_NORMAL);
    TextIn(cancel, PASS_CANCEL_X, oy, PASS_CANCEL_W, HEAD_H, &sNormal, 'c', STYLE_WHITE, 6);
}

/* The description along the bottom band: up to two lines, white. */
static void PassDescription(const u8 *text)
{
    if (text)
        TextLines(text, 10, PASS_BAND_Y + 3, PASS_W - 20, 16, 2, &sNormal, STYLE_WHITE, TRUE);
}

static const u8 *PassTitle(void)
{
    return Ascii(CTR_TEXT("FRONTIER PASS", "PASE DEL FRENTE", "PASSE ZONE COMBAT", "TESSERA LOTTA",
                          "PASSE DA BATALHA", "KAMPFZONENPASS", "フロンティアパス"));
}

static const u8 *PassCancel(void)
{
    return Ascii(CTR_TEXT("CANCEL", "SALIR", "RETOUR", "ESCI", "SAIR", "ZURÜCK", "やめる"));
}

typedef struct
{
    struct CtrPassView v;
    PassSprite symbols[CTR_PASS_FACILITIES], indicator, head;
    /* For a symbol not earned: its shape, from the sheet, drawn dim. */
    PassSprite empty[CTR_PASS_FACILITIES];
    bool8 thumbs, art;
    s16 indicatorX, indicatorY, headX, headY;
    /* The layers' pictures, told apart by their sums. */
    u32 sums[2];
    /* The cursor, last: everything before it is the base. */
    s16 handX, handY;
    PassSprite hand;
} PassScene;

/* The pass: its plates where the game's areas are, the art on them. */
static void PassDrawMain(const PassScene *sc)
{
    const struct CtrPassView *v = &sc->v;
    u8 area = v->cursorArea;
    bool8 symbolsOn = area == CTR_PASS_AREA_EARNED_SYMBOLS || area >= CTR_PASS_AREA_SYMBOL_TOWER;
    int oy;

    PassHeader(PassTitle(), PassCancel(), area == CTR_PASS_AREA_CANCEL);

    /* The symbols: a plate with its name in a band on top, the seven cells
     * on its face where the game has them, the ones not earned dim. */
    oy = PlateEx(PASS_LEFT_X, PASS_SYMBOLS_Y, PASS_LEFT_W, PASS_SYMBOLS_H, symbolsOn ? sEmerald : sSlate,
                 symbolsOn ? PLATE_STATE_FOCUS : PLATE_STATE_NORMAL, 0, 0, PlateCut(PASS_SYMBOLS_H), PASS_SYMBOLS_HEAD);
    TextIn(gText_SymbolsEarned, PASS_LEFT_X, oy, PASS_LEFT_W, PASS_SYMBOLS_HEAD, &sNormal, 'c', STYLE_WHITE, 6);
    for (int i = 0; i < CTR_PASS_FACILITIES; ++i)
    {
        int gx = sPassSymbolCells[i][0] + 8, gy = sPassSymbolCells[i][1] + 8;
        int cx = PassX(gx), cy = PassY(gx, gy) + (oy - PASS_SYMBOLS_Y);

        if (area == CTR_PASS_AREA_SYMBOL_TOWER + i)
            Plate(cx - 15, cy - 15, 30, 30, sEmerald, PLATE_STATE_FOCUS);
        if (v->symbols[i] && sc->symbols[i].w)
            PassSpriteAt(&sc->symbols[i], cx - 12, cy - 12, 3, FALSE);
        else if (sc->empty[i].w)
            PassSpriteAt(&sc->empty[i], cx - 12, cy - 12, 3, FALSE);
    }

    /* The record: a button, grey and never lit without one to watch. */
    {
        bool8 on = area == CTR_PASS_AREA_RECORD && v->hasRecord;

        oy = Plate(PASS_LEFT_X, PASS_RECORD_Y, PASS_LEFT_W, PASS_RECORD_H,
                   !v->hasRecord ? sOffGrey : on ? sEmerald : sSlate, on ? PLATE_STATE_FOCUS : PLATE_STATE_NORMAL);
        TextIn(gText_BattleRecord, PASS_LEFT_X, oy, PASS_LEFT_W, PASS_RECORD_H, &sNormal, 'c', STYLE_WHITE, 6);
    }

    /* The points: their name and the number, green, on cream. */
    {
        bool8 on = area == CTR_PASS_AREA_POINTS;

        oy = PlateEx(PASS_LEFT_X, PASS_POINTS_Y, PASS_LEFT_W, PASS_POINTS_H, on ? sEmerald : sLight,
                     on ? PLATE_STATE_FOCUS : PLATE_STATE_NORMAL, PASS_LEFT_W - 10, 30, PlateCut(PASS_POINTS_H), 0);
        TextIn(gText_BattlePoints, PASS_LEFT_X, oy, PASS_LEFT_W - 6, PASS_POINTS_H / 2, &sNormal, 'l', STYLE_INK, 8);
        TextIn(Number(v->battlePoints, 5, STR_CONV_MODE_LEFT_ALIGN), PASS_LEFT_X, oy + PASS_POINTS_H / 2 - 2,
               PASS_LEFT_W - 6, PASS_POINTS_H / 2, &sNormal, 'r', STYLE_GREEN, 10);
    }

    /* The map and the card: the game's pictures of them on their plates. */
    for (int k = 0; k < 2; ++k)
    {
        bool8 on = area == (k ? CTR_PASS_AREA_CARD : CTR_PASS_AREA_MAP);
        int y = k ? PASS_CARD_Y : PASS_MAP_Y;

        oy = Plate(PASS_RIGHT_X, y, PASS_RIGHT_W, PASS_THUMB_H, on ? sEmerald : sSlate,
                   on ? PLATE_STATE_FOCUS : PLATE_STATE_NORMAL);
        if (sc->thumbs)
            PassBlit(sPassThumbs[k], PASS_THUMB_GW, PASS_THUMB_GH, PASS_RIGHT_X + (PASS_RIGHT_W - PASS_THUMB_W) / 2,
                     oy + (PASS_THUMB_H - PASS_THUMB_H_ART) / 2, PASS_THUMB_W, PASS_THUMB_H_ART, FALSE);
    }

    /* Into the map or the card: its picture grows over the screen, as the
     * game's zooms, under the game's white fade. */
    if (v->zoomArea && v->zoomScale > Q_8_8(1) && sc->thumbs)
    {
        int k = v->zoomArea == CTR_PASS_AREA_CARD, w = PASS_THUMB_W * v->zoomScale / 256,
            h = PASS_THUMB_H_ART * v->zoomScale / 256;
        int cx = PASS_RIGHT_X + PASS_RIGHT_W / 2, cy = (k ? PASS_CARD_Y : PASS_MAP_Y) + PASS_THUMB_H / 2;

        PassBlit(sPassThumbs[k], PASS_THUMB_GW, PASS_THUMB_GH, cx - w / 2, cy - h / 2, w, h, FALSE);
    }
    PassDescription(v->description);
}

/* The Frontier map: its picture, the facilities as plates, the description. */
static void PassDrawMap(const PassScene *sc)
{
    const struct CtrPassView *v = &sc->v;
    u8 name[32];
    int oy;

    GetMapName(name, MAPSEC_BATTLE_FRONTIER, 0);
    PassHeader(name, PassCancel(), FALSE);
    oy = Plate(PASS_ART_X - 6, PASS_ART_Y - 4, PASS_ART_GW * 3 / 2 + 12, PASS_ART_GH * 3 / 2 + 8, sSlate,
               PLATE_STATE_NORMAL);
    if (sc->art)
    {
        int saved[4];

        PassBlit(sPassArt, PASS_ART_GW, PASS_ART_GH, PASS_ART_X, oy, PASS_ART_GW * 3 / 2, PASS_ART_GH * 3 / 2, FALSE);
        /* The facility's marker and the player's head on it, clipped to it. */
        ClipPush(saved, PASS_ART_X, oy, PASS_ART_X + PASS_ART_GW * 3 / 2, oy + PASS_ART_GH * 3 / 2);
        if (sc->indicator.w)
            PassSpriteAt(&sc->indicator, PASS_ART_X + (sc->indicatorX - PASS_ART_GX) * 3 / 2,
                         oy + (sc->indicatorY - PASS_ART_GY) * 3 / 2, 3, FALSE);
        if (v->mapHead && sc->head.w)
            PassSpriteAt(&sc->head, PASS_ART_X + (sc->headX - PASS_ART_GX) * 3 / 2,
                         oy + (sc->headY - PASS_ART_GY) * 3 / 2, 3, FALSE);
        ClipPop(saved);
    }
    for (int i = 0; i < CTR_PASS_FACILITIES; ++i)
    {
        bool8 on = i == v->mapCursor;
        int y = PASS_NAMES_Y + i * PASS_NAME_STEP;

        oy = Plate(PASS_NAMES_X, y, PASS_NAMES_W, PASS_NAME_H, on ? sEmerald : sSlate,
                   on ? PLATE_STATE_FOCUS : PLATE_STATE_NORMAL);
        if (v->names[i])
            TextIn(v->names[i], PASS_NAMES_X, oy, PASS_NAMES_W, PASS_NAME_H, &sNormal, 'l', STYLE_WHITE, 10);
    }
    PassDescription(v->mapDescription);
}

/* Draws into the pass's canvas instead of the bottom screen's (and back). */
static void PassTarget(bool8 on, int saved[5])
{
    if (on)
    {
        saved[0] = sClipX0;
        saved[1] = sClipY0;
        saved[2] = sClipX1;
        saved[3] = sClipY1;
        saved[4] = sOX;
        sDst = sPassCanvas;
        sOX = 0;
        sClipX0 = sClipY0 = 0;
        sClipX1 = PASS_W;
        sClipY1 = PASS_H;
    }
    else
    {
        sClipX0 = saved[0];
        sClipY0 = saved[1];
        sClipX1 = saved[2];
        sClipY1 = saved[3];
        sOX = saved[4];
        sDst = sCanvas;
    }
}

/* Everything but the cursor, into sPassBase. */
static void PassCompose(const PassScene *sc)
{
    int saved[5];

    PassTarget(TRUE, saved);
    if (sPassBackdrop)
        memcpy(sPassCanvas, sPassBackdrop, sizeof(sPassCanvas));
    else
        FillRect(0, 0, PASS_W, PASS_H, PackRgb(146, 212, 172));
    if (sc->v.view == CTR_PASS_MAP)
        PassDrawMap(sc);
    else
        PassDrawMain(sc);
    PassTarget(FALSE, saved);
    if (sPassBase)
        memcpy(sPassBase, sPassCanvas, sizeof(sPassCanvas));
}

/* The cursor over a copy of the rest. */
static void PassComposeCursor(const PassScene *sc)
{
    int saved[5];

    if (sPassBase)
        memcpy(sPassCanvas, sPassBase, sizeof(sPassCanvas));
    PassTarget(TRUE, saved);
    if (sc->v.view == CTR_PASS_MAP)
    {
        /* Beside the facility's plate, following the game's hand down the
         * list (it steps 16 a facility, here PASS_NAME_STEP). */
        int y = PASS_NAMES_Y + PASS_NAME_H / 2 + (sc->handY - 8) * PASS_NAME_STEP / 16;

        PassHand(&sc->hand, PASS_NAMES_X - 2, y + 4, TRUE);
    }
    else
        PassHand(&sc->hand, PassX(sc->handX - 5), PassY(sc->handX - 5, sc->handY + 5), FALSE);
    PassTarget(FALSE, saved);
}

/* ------------------------------------------------------------------------ */
/* From the game                                                            */
/* ------------------------------------------------------------------------ */

/* The dim of a symbol not earned yet, as the trainer card dims its badges. */
#define PASS_DIM_BGR (22 | 24 << 5 | 23 << 10)

/* Sprite tiles as the game has them in OBJ VRAM (1D), in palette `pal` of
 * its unfaded sprite palettes, or all in `dim` when it is not 0. */
static u32 PassTakeTiles(PassSprite *out, u16 tile, u8 pal, u8 w, u8 h, u16 dim)
{
    const u8 *obj = (const u8 *)VRAM_ + 0x10000;
    const u16 *plt = gPlttBufferUnfaded + 0x100 + (pal & 15) * 16;
    u32 sum = 0;

    out->w = w;
    out->h = h;
    for (int y = 0; y < h; ++y)
        for (int x = 0; x < w; ++x)
        {
            unsigned t = (tile + (y / 8) * (w / 8) + x / 8) & 1023;
            u8 b = obj[t * 32 + (y & 7) * 4 + (x & 7) / 2], c = (x & 1) ? b >> 4 : b & 15;
            u16 p = !c ? 0 : (dim ? dim : plt[c] & 0x7FFF) | PASS_OPAQUE;

            out->px[y * w + x] = p;
            sum = sum * 31 + p;
        }
    return sum;
}

/* A sprite's picture as it shows now (its animation's frame, its palette). */
static u32 PassTakeSprite(PassSprite *out, const struct Sprite *s)
{
    static const u8 sizes[3][4][2] = {
        {{8, 8}, {16, 16}, {32, 32}, {64, 64}},
        {{16, 8}, {32, 8}, {32, 16}, {64, 32}},
        {{8, 16}, {8, 32}, {16, 32}, {32, 64}},
    };
    u8 w, h;

    out->w = out->h = 0;
    if (s == NULL || s->oam.shape > 2)
        return 0;
    w = sizes[s->oam.shape][s->oam.size][0];
    h = sizes[s->oam.shape][s->oam.size][1];
    if (w > 32 || h > 32)
        return 0;
    return PassTakeTiles(out, s->oam.tileNum, s->oam.paletteNum, w, h, 0);
}

/* A part of a 4bpp text layer as it shows now, through its tilemap, in the
 * unfaded palettes; the backdrop's colour where the layer is clear. */
static u32 PassTakeLayer(u8 bg, int gx, int gy, int w, int h, u16 *out)
{
    u16 control = GetGpuReg(REG_OFFSET_BG0CNT + bg * 2);
    const u8 *chars = (const u8 *)VRAM_ + ((control >> 2) & 3) * BG_CHAR_SIZE;
    const u16 *map = (const u16 *)((const u8 *)VRAM_ + ((control >> 8) & 31) * BG_SCREEN_SIZE);
    u16 backdrop = (gPlttBufferUnfaded[0] & 0x7FFF) | PASS_OPAQUE;
    u32 sum = 0;

    if (control & 0x80)
        return 0;
    for (int y = 0; y < h; ++y)
        for (int x = 0; x < w; ++x)
        {
            int px = (gx + x) & 255, py = (gy + y) & 255;
            u16 e = map[(py / 8) * 32 + px / 8];
            int sx = (e & 0x400) ? 7 - (px & 7) : px & 7, sy = (e & 0x800) ? 7 - (py & 7) : py & 7;
            u8 b = chars[((e & 0x3FF) * 32 + sy * 4 + sx / 2) & 0xFFFF], c = (sx & 1) ? b >> 4 : b & 15;
            u16 p = c ? (gPlttBufferUnfaded[(e >> 12) * 16 + c] & 0x7FFF) | PASS_OPAQUE : backdrop;

            out[y * w + x] = p;
            sum = sum * 31 + p;
        }
    return sum;
}

/*
 * The backdrop, romfs/bottom/pass_top.bin (scripts/gen_pass_top_art.py):
 * the bottom screen's (gen_menu_art.py) made 400 wide, in the same format.
 * Without it, a plain one in its colours.
 */
static u16 *PassLoadBackdrop(void)
{
    FILE *f = fopen("romfs:/bottom/pass_top.bin", "rb");
    u16 *dst = malloc(sizeof(sPassCanvas));
    bool8 ok = FALSE;

    if (!dst)
    {
        if (f)
            fclose(f);
        return NULL;
    }
    if (f)
    {
        u8 head[20];

        if (fread(head, 1, sizeof(head), f) == sizeof(head) && memcmp(head, "EM3DMNU1", 8) == 0
         && (head[16] | (head[17] << 8)) == PASS_W && (head[18] | (head[19] << 8)) == PASS_H)
            ok = fread(dst, 1, sizeof(sPassCanvas), f) == sizeof(sPassCanvas);
        fclose(f);
    }
    if (!ok)
    {
        CtrLog_Write(CTR_LOG_ERROR, "bottom: romfs/bottom/pass_top.bin missing or bad, plain backdrop");
        for (int x = 0; x < PASS_W; ++x)
            for (int y = 0; y < PASS_H; ++y)
            {
                int t = y * 255 / (PASS_H - 1);

                dst[x * PASS_H + (PASS_H - 1 - y)] = y < 11 || y >= PASS_BAND_Y ? PackRgb(18, 52, 40)
                    : PackRgb(168 - 50 * t / 255, 226 - 34 * t / 255, 190 - 42 * t / 255);
            }
    }
    return dst;
}

/* What was last composed. */
static PassScene sPassScene, sPassShown;
static bool8 sPassUp;
static u8 sPassSettle;
/* Frames to compose again after a change: the game's tilemaps and palettes
 * reach its VRAM a frame after the state that asked for them. */
#define PASS_SETTLE_FRAMES 2

static void PassTopFrame(void)
{
    PassScene *sc = &sPassScene;
    struct CtrPassView v;
    bool8 base, cursor;

    memset(&v, 0, sizeof(v));
    if (!CtrPass_GetView(&v))
    {
        if (sPassUp)
        {
            CtrPassTop_Hide();
            free(sPassBase);
            free(sPassBackdrop);
            sPassBase = sPassBackdrop = NULL;
            sPassUp = FALSE;
        }
        return;
    }
    if (!sPassUp)
    {
        sPassBackdrop = PassLoadBackdrop();
        sPassBase = malloc(sizeof(sPassCanvas));
        if (!sPassBackdrop || !sPassBase)
        {
            free(sPassBase);
            free(sPassBackdrop);
            sPassBase = sPassBackdrop = NULL;
            return;
        }
    }
    memset(sc, 0, sizeof(*sc));
    memcpy(&sc->v, &v, sizeof(v));
    if (v.view == CTR_PASS_MAIN)
    {
        PassTakeSprite(&sc->hand, v.cursor);
        sc->handX = v.cursor->x;
        sc->handY = v.cursor->y;
        for (int i = 0; i < CTR_PASS_FACILITIES; ++i)
        {
            if (v.symbolSprites[i])
                PassTakeSprite(&sc->symbols[i], v.symbolSprites[i]);
            else if (v.medalTiles != 0xFFFF)
                PassTakeTiles(&sc->empty[i], v.medalTiles + i * 4, 0, 16, 16, PASS_DIM_BGR);
        }
        sc->sums[0] = PassTakeLayer(1, PASS_THUMB_GX, PASS_THUMB_MAP_GY, PASS_THUMB_GW, PASS_THUMB_GH, sPassThumbs[0]);
        sc->sums[1] = PassTakeLayer(1, PASS_THUMB_GX, PASS_THUMB_CARD_GY, PASS_THUMB_GW, PASS_THUMB_GH, sPassThumbs[1]);
        sc->thumbs = TRUE;
    }
    else
    {
        PassTakeSprite(&sc->hand, v.mapHand);
        sc->handX = v.mapHand->x;
        sc->handY = v.mapHand->y;
        PassTakeSprite(&sc->indicator, v.mapIndicator);
        sc->indicatorX = v.mapIndicator->x + v.mapIndicator->x2 + v.mapIndicator->centerToCornerVecX;
        sc->indicatorY = v.mapIndicator->y + v.mapIndicator->y2 + v.mapIndicator->centerToCornerVecY;
        if (v.mapHead)
        {
            PassTakeSprite(&sc->head, v.mapHead);
            sc->headX = v.mapHead->x + v.mapHead->x2 + v.mapHead->centerToCornerVecX;
            sc->headY = v.mapHead->y + v.mapHead->y2 + v.mapHead->centerToCornerVecY;
        }
        sc->sums[0] = PassTakeLayer(2, PASS_ART_GX, PASS_ART_GY, PASS_ART_GW, PASS_ART_GH, sPassArt);
        sc->art = TRUE;
    }
    /* The sprites' own pointers say nothing their pictures do not. */
    sc->v.cursor = sc->v.mapHand = NULL;
    sc->v.mapIndicator = sc->v.mapHead = NULL;
    memset(sc->v.symbolSprites, 0, sizeof(sc->v.symbolSprites));

    base = !sPassUp || memcmp(sc, &sPassShown, offsetof(PassScene, handX)) != 0;
    if (base)
        sPassSettle = PASS_SETTLE_FRAMES;
    else if (sPassSettle)
    {
        --sPassSettle;
        base = TRUE;
    }
    cursor = memcmp((const u8 *)sc + offsetof(PassScene, handX), (const u8 *)&sPassShown + offsetof(PassScene, handX),
                    sizeof(PassScene) - offsetof(PassScene, handX)) != 0;
    if (base)
        PassCompose(sc);
    if (base || cursor)
    {
        PassComposeCursor(sc);
        CtrPassTop_Show(sPassCanvas);
        memcpy(&sPassShown, sc, sizeof(*sc));
    }
    CtrPassTop_SetFade(gPaletteFade.blendColor, gPaletteFade.y);
    sPassUp = TRUE;
}

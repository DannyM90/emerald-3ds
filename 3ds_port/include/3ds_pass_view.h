/*
 * The Frontier Pass as the top screen draws it natively
 * (3ds_bottom_pass.inc), handed over by frontier_pass.c
 * (0084-frontier-pass-top.patch). The game still runs the pass, its map and
 * their input; this is only what is on show.
 */
#ifndef CTR_PASS_VIEW_H
#define CTR_PASS_VIEW_H

#include "global.h"

struct Sprite;

enum
{
    CTR_PASS_NONE,
    /* The pass: symbols, record, points, the map and the card. */
    CTR_PASS_MAIN,
    /* The Frontier map with its facilities. */
    CTR_PASS_MAP,
};

/* frontier_pass.c's cursor areas, the same values. */
enum
{
    CTR_PASS_AREA_NOTHING,
    CTR_PASS_AREA_MAP,
    CTR_PASS_AREA_CARD,
    CTR_PASS_AREA_RECORD,
    CTR_PASS_AREA_CANCEL,
    CTR_PASS_AREA_POINTS,
    CTR_PASS_AREA_EARNED_SYMBOLS,
    CTR_PASS_AREA_SYMBOL_TOWER,
    CTR_PASS_AREA_COUNT = CTR_PASS_AREA_SYMBOL_TOWER + 7,
};

#define CTR_PASS_FACILITIES 7

struct CtrPassView
{
    u8 view;                /* CTR_PASS_* */
    /* The pass. */
    u8 cursorArea;          /* CTR_PASS_AREA_* under the cursor */
    u8 stars;
    bool8 hasRecord;
    u16 battlePoints;
    u8 symbols[CTR_PASS_FACILITIES];        /* 0 none, 1 silver, 2 gold */
    /* Zooming into the map or the card (or out of it): which, and how far
     * as Q8.8 (256 = not at all). */
    u8 zoomArea;
    u16 zoomScale;
    const struct Sprite *cursor;
    const struct Sprite *symbolSprites[CTR_PASS_FACILITIES];
    /* The symbols' sheet in OBJ VRAM, every facility's (four tiles each,
     * TOWER first), for the ones not earned yet; 0xFFFF when not in. */
    u16 medalTiles;
    const u8 *description;  /* the cursor area's text, NULL for none */
    /* The map. */
    u8 mapCursor;           /* the facility chosen */
    const struct Sprite *mapHand, *mapIndicator, *mapHead;
    const u8 *names[CTR_PASS_FACILITIES];
    const u8 *mapDescription;
};

/* What is on show; FALSE (view CTR_PASS_NONE) while neither is up. */
bool8 CtrPass_GetView(struct CtrPassView *view);

#endif

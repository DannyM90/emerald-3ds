/* Part of 3ds_bottom_ui.c: Danny Edition, the Pokédex's top screen in HD (included after 3ds_bottom_factory_top.inc). */

/*
 * While the Pokédex is open the top screen showed the device (3ds_dex_top.c):
 * its red shell drawn of plain boxes, the Pokémon at 2x on its terrain and
 * its name in a 3x5 pixel font. This draws the same Pokémon as a panel in
 * the bottom screen's style instead, on the menu's backdrop: its picture at
 * exactly twice its size on the terrain it is met on, its number, name,
 * category, types, height and weight, and its Pokédex text once it is caught.
 * The device still chooses the Pokémon and decodes its terrain; this panel is
 * drawn over it (present.inc RenderDexTop).
 */
#include "3ds_dex_top.h"
#include "international_string_util.h"

#define DHD_AREA_W 176
#define DHD_AREA_H 186
#define DHD_RIGHT_X 200
#define DHD_RIGHT_W (FTOP_W - DHD_RIGHT_X - 6)

static struct
{
    u16 national;
    bool8 ready, backdrop, up;
} sDexHdShown;

/* "0,7 m" / "6,9 kg" from tenths, as the game's own Pokédex writes them. */
static const u8 *DexHdTenths(u16 tenths, const char *unit)
{
    static u8 text[16];
    u8 *end = ConvertIntToDecimalStringN(text, tenths / 10, STR_CONV_MODE_LEFT_ALIGN, 4);

    *end++ = *Ascii(CTR_TEXT(".", ".", ",", ",", ",", ",", "."));
    *end++ = CHAR_0 + tenths % 10;
    *end++ = CHAR_SPACE;
    StringCopy(end, Ascii(unit));
    return text;
}

static void DexHdRow(const u8 *label, const u8 *value, int y)
{
    TextIn(label, DHD_RIGHT_X + 10, y, 74, 16, &sSmall, 'l', STYLE_GRAY, 0);
    TextIn(value, DHD_RIGHT_X + 86, y, DHD_RIGHT_W - 96, 16, &sNormal, 'r', STYLE_INK, 0);
}

/* The panel, into the panel's canvas. FALSE while the picture is not in yet. */
static bool8 DrawDexHd(u16 national, const u16 *backdrop)
{
    u16 species = national ? NationalPokedexNumToSpecies(national) : SPECIES_NONE;
    bool8 caught = species != SPECIES_NONE && GetSetPokedexFlag(national, FLAG_GET_CAUGHT) != 0;
    int slot = species ? FrontPic(species, FALSE) : -1;
    int oy, ax, ay, moundX, moundY;
    u16 number = !national || IsNationalPokedexEnabled() ? national : NationalToHoennOrder(national);
    u8 text[32];

    FacTopPaint(sDst);
    /* The title bar, the number in a dark pill at its right. */
    oy = Plate(6, FTOP_BAR_Y, FTOP_W - 12, FTOP_BAR_H, sEmerald, PLATE_STATE_NORMAL);
    TextIn(gText_MenuPokedex, 14, oy, 200, 20, &sNormal, 'l', STYLE_WHITE, 0);
    StringCopy(StringCopy(text, Ascii(CTR_TEXT("No.", "Nº", "N°", "N.", "Nº", "Nr.", "No."))),
               Number(number, 3, STR_CONV_MODE_LEADING_ZEROS));
    FacTopRound(FTOP_W - 6 - 8 - 64, FTOP_BAR_Y + 4, 64, FTOP_BAR_H - 8, 3, PackRgb(40, 94, 70));
    TextIn(text, FTOP_W - 6 - 8 - 64, FTOP_BAR_Y + 4, 64, FTOP_BAR_H - 8, &sSmall, 'c', STYLE_WHITE, 0);

    /* The Pokémon on its terrain: the battle backdrop at 1x, its mound under
     * the picture's feet, the sky above it; the picture at exactly 2x. */
    oy = Plate(6, 36, DHD_AREA_W + 12, DHD_AREA_H + 12, sCreamPlate, PLATE_STATE_NORMAL);
    ax = 12;
    ay = oy + 4;
    moundX = ax + DHD_AREA_W / 2;
    moundY = ay + DHD_AREA_H - 34;
    if (backdrop)
    {
        u16 sky = backdrop[0];

        for (int py = 0; py < DHD_AREA_H; ++py)
            for (int px = 0; px < DHD_AREA_W; ++px)
            {
                int sx = px - DHD_AREA_W / 2 + 176, sy = py - (DHD_AREA_H - 34) + 76;
                u16 c = sx >= 0 && sx < CTR_DEX_TOP_BACKDROP_W && sy >= 0 && sy < CTR_DEX_TOP_BACKDROP_H
                      ? backdrop[sy * CTR_DEX_TOP_BACKDROP_W + sx] : sky;

                FillRect(ax + px, ay + py, 1, 1,
                         PackRgb(((c >> 11) & 31) * 255 / 31, ((c >> 6) & 31) * 255 / 31, ((c >> 1) & 31) * 255 / 31));
            }
    }
    else
    {
        FacTopRing(moundX, moundY - 64, 64, 10, PackRgb(234, 222, 206));
        FacTopEllipse(moundX, moundY, 52, 8, PackRgb(214, 200, 184));
    }
    if (slot >= 0)
        DrawSprite2x(sFronts[slot].tiles, 8, 8, moundX - 64, moundY + 6 - 128, sFronts[slot].pal.c);

    /* Its name and category. */
    oy = Plate(DHD_RIGHT_X, 36, DHD_RIGHT_W, 44, sCreamPlate, PLATE_STATE_NORMAL);
    TextIn(species ? gSpeciesNames[species] : Ascii("----------"), DHD_RIGHT_X + 10, oy + 1, DHD_RIGHT_W - 20, 18,
           &sNormal, 'l', STYLE_INK, 0);
    if (caught)
    {
        CopyMonCategoryText(national, text);
        TextIn(text, DHD_RIGHT_X + 10, oy + 21, DHD_RIGHT_W - 20, 14, &sSmall, 'l', STYLE_GRAY, 0);
    }
    else
        TextIn(gText_5MarksPokemon, DHD_RIGHT_X + 10, oy + 21, DHD_RIGHT_W - 20, 14, &sSmall, 'l', STYLE_GRAY, 0);

    /* Types, height, weight. */
    oy = Plate(DHD_RIGHT_X, 86, DHD_RIGHT_W, 58, sCreamPlate, PLATE_STATE_NORMAL);
    TextIn(Ascii(CTR_TEXT("TYPE", "TIPO", "TYPE", "TIPO", "TIPO", "TYP", "タイプ")), DHD_RIGHT_X + 10, oy + 2, 74, 16,
           &sSmall, 'l', STYLE_GRAY, 0);
    if (species && caught)
    {
        DrawTypeIcon(gSpeciesInfo[species].types[0], DHD_RIGHT_X + 86, oy + 3);
        if (gSpeciesInfo[species].types[1] != gSpeciesInfo[species].types[0])
            DrawTypeIcon(gSpeciesInfo[species].types[1], DHD_RIGHT_X + 124, oy + 3);
    }
    else
        TextIn(Ascii("???"), DHD_RIGHT_X + 86, oy + 2, DHD_RIGHT_W - 96, 16, &sNormal, 'r', STYLE_INK, 0);
    DexHdRow(Ascii(CTR_TEXT("HEIGHT", "ALTURA", "TAILLE", "ALTEZZA", "ALTURA", "GRÖSSE", "たかさ")),
             caught ? DexHdTenths(gPokedexEntries[national].height, "m") : Ascii("???"), oy + 19);
    DexHdRow(Ascii(CTR_TEXT("WEIGHT", "PESO", "POIDS", "PESO", "PESO", "GEWICHT", "おもさ")),
             caught ? DexHdTenths(gPokedexEntries[national].weight, "kg") : Ascii("???"), oy + 36);

    /* Its Pokédex text, once caught. */
    oy = Plate(DHD_RIGHT_X, 150, DHD_RIGHT_W, FTOP_H - 150 - 6, sCreamPlate, PLATE_STATE_NORMAL);
    if (caught && gPokedexEntries[national].description)
        TextLines(gPokedexEntries[national].description, DHD_RIGHT_X + 8, oy + 2, DHD_RIGHT_W - 16, 15, 5, &sSmall,
                  STYLE_INK, TRUE);
    return species == SPECIES_NONE || slot >= 0;
}

/* Every frame: the panel drawn again when its Pokémon changed, and kept up. */
static void DexHdFrame(void)
{
    u16 national = CtrDexTop_National();
    const u16 *backdrop = CtrDexTop_BackdropPixels();
    u16 *dst = sDst;
    int ox = sOX, cx0 = sClipX0, cy0 = sClipY0, cx1 = sClipX1, cy1 = sClipY1;

    if (!CtrDexTop_Active())
    {
        if (sDexHdShown.up)
            CtrTopPanel_Drop();
        sDexHdShown.up = FALSE;
        return;
    }
    /* The panel went (or another took it): drawn and handed over again. */
    if (sDexHdShown.up && !CtrTopPanel_Active())
        sDexHdShown.up = FALSE;
    if (!sDexHdShown.up || !sDexHdShown.ready || sDexHdShown.national != national
     || sDexHdShown.backdrop != (backdrop != NULL))
    {
        FactoryTopWarm();
        sDst = CtrTopPanel_Canvas();
        sOX = 0;
        sClipX0 = sClipY0 = 0;
        sClipX1 = FTOP_W;
        sClipY1 = FTOP_H;
        sDexHdShown.ready = DrawDexHd(national, backdrop) && sIconsReady;
        sDst = dst;
        sOX = ox;
        sClipX0 = cx0;
        sClipY0 = cy0;
        sClipX1 = cx1;
        sClipY1 = cy1;
        sDexHdShown.national = national;
        sDexHdShown.backdrop = backdrop != NULL;
        sDexHdShown.up = TRUE;
        CtrTopPanel_Show();
    }
    /* It follows the game's fade to black as the Pokédex comes and goes. */
    CtrTopPanel_Keep(gPaletteFade.mode == 0 && gPaletteFade.blendColor == RGB_BLACK
                     ? (gPaletteFade.y > 16 ? 16 : gPaletteFade.y) : 0);
}

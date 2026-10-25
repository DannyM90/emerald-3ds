/*
 * More CHEATS (included by 3ds_extras.c before the table; the lines are in
 * 3ds_cheats_plus_extras.inc). Each action works on the save in memory, like
 * the Pokedex lines: OPTIONS is only open in the field, and the player saves
 * to keep it.
 *
 *   ev_tickets      the Eon and Aurora Tickets, the Mystic Ticket and the Old
 *                   Sea Map, and the flags that let the Lilycove ferry sail
 *                   to the islands (the Nintendo events gave both)
 *   all_badges      the eight badges
 *   all_fly         every town and city as a Fly destination
 *   ignore_trainers no trainer walks up on sight (trainer_see.c); talking
 *                   to one still starts the battle
 *   heal_party      the party healed, as at a Pokemon Center
 *   max_friendship  the party's friendship at its most (Golbat, Azurill...)
 *   perfect_ivs     the party's IVs at 31, stats recalculated
 *   gm_species, gm_level, gm_shiny, gm_give
 *                   a Pokemon of any species and level, shiny or not, into
 *                   the party (the PC when it is full), seen and caught in
 *                   the Pokedex
 *   open_pc         the PC's boxes, from anywhere in the field
 *   move_relearner  the Move Relearner, without a Heart Scale
 * (ENHANCEMENTS, shown under EXTRAS too: the last two run a game script,
 * so they wait for the player to stand free in the field)
 */
#include "data.h"
#include "field_player_avatar.h"
#include "main.h"
#include "overworld.h"
#include "script.h"
#include "malloc.h"
#include "strings.h"
#include "random.h"
#include "script_pokemon_util.h"
#include "string_util.h"
#include "constants/species.h"

static const char *const sTapToSet[] = {CTR_TEXT("TAP TO SET", "TOCA PARA ACTIVAR", "ACTIVER", "ATTIVA", "TOQUE P/ ATIVAR", "ANTIPPEN")};
static const char *const sTapToToggle[] = {CTR_TEXT("TAP: ON / OFF", "TOCA: SÍ / NO", "OUI / NON", "SÌ / NO", "TOQUE: SIM/NÃO", "AN / AUS")};
static const char *const sTapToHeal[] = {CTR_TEXT("TAP TO HEAL", "TOCA PARA CURAR", "SOIGNER", "CURA", "TOQUE P/ CURAR", "HEILEN")};

static void GiveTickets(void)
{
    static const u16 sTickets[][2] = {
        {ITEM_EON_TICKET, FLAG_ENABLE_SHIP_SOUTHERN_ISLAND},
        {ITEM_AURORA_TICKET, FLAG_ENABLE_SHIP_BIRTH_ISLAND},
        {ITEM_OLD_SEA_MAP, FLAG_ENABLE_SHIP_FARAWAY_ISLAND},
        {ITEM_MYSTIC_TICKET, FLAG_ENABLE_SHIP_NAVEL_ROCK},
    };
    bool8 ok = TRUE;

    for (u32 i = 0; i < ARRAY_COUNT(sTickets); i++)
    {
        FlagSet(sTickets[i][1]);
        if (!CheckBagHasItem(sTickets[i][0], 1) && !AddBagItem(sTickets[i][0], 1))
            ok = FALSE;
    }
    PlaySE(ok ? SE_SUCCESS : SE_FAILURE);
}

static void AllBadges(void)
{
    for (u32 flag = FLAG_BADGE01_GET; flag <= FLAG_BADGE08_GET; flag++)
        FlagSet(flag);
    PlaySE(SE_SUCCESS);
}

/* The Frontier Pass, on and off again: to see its screen (the TRAINER CARD
 * button opens it while the player has it) without beating the League. */
static void ToggleFrontierPass(void)
{
    if (FlagGet(FLAG_SYS_FRONTIER_PASS))
    {
        FlagClear(FLAG_SYS_FRONTIER_PASS);
        PlaySE(SE_BALL_OPEN);
    }
    else
    {
        FlagSet(FLAG_SYS_FRONTIER_PASS);
        PlaySE(SE_SUCCESS);
    }
}

static void AllFlyDestinations(void)
{
    for (u32 flag = FLAG_VISITED_LITTLEROOT_TOWN; flag <= FLAG_VISITED_EVER_GRANDE_CITY; flag++)
        FlagSet(flag);
    PlaySE(SE_SUCCESS);
}

static void HealParty(void)
{
    HealPlayerParty();
    PlaySE(SE_USE_ITEM);
}

static void MaxFriendship(void)
{
    u8 most = MAX_FRIENDSHIP;

    for (u32 i = 0; i < PARTY_SIZE; i++)
        if (GetMonData(&gPlayerParty[i], MON_DATA_SPECIES) != SPECIES_NONE
         && !GetMonData(&gPlayerParty[i], MON_DATA_IS_EGG))
            SetMonData(&gPlayerParty[i], MON_DATA_FRIENDSHIP, &most);
    PlaySE(SE_SUCCESS);
}

static void PerfectIvs(void)
{
    u8 best = MAX_PER_STAT_IVS;

    for (u32 i = 0; i < PARTY_SIZE; i++)
    {
        if (GetMonData(&gPlayerParty[i], MON_DATA_SPECIES) == SPECIES_NONE)
            continue;
        for (u32 stat = MON_DATA_HP_IV; stat <= MON_DATA_SPDEF_IV; stat++)
            SetMonData(&gPlayerParty[i], stat, &best);
        CalculateMonStats(&gPlayerParty[i]);
    }
    PlaySE(SE_SUCCESS);
}

/* Give a Pokemon: the species steps through the National Pokedex's order of
 * ids, without the unused Unown slots. */
static bool8 GiveSpeciesUsable(u16 species)
{
    return species > SPECIES_NONE && species < NUM_SPECIES
        && !(species >= SPECIES_OLD_UNOWN_B && species <= SPECIES_OLD_UNOWN_Z);
}

static u16 GiveSpecies(void)
{
    u16 species = CtrSettings_GetInt("gm_species", SPECIES_TREECKO);

    return GiveSpeciesUsable(species) ? species : SPECIES_TREECKO;
}

static void GiveSpeciesStep(int direction)
{
    u16 species = GiveSpecies();

    do
        species = direction < 0 ? (species <= 1 ? NUM_SPECIES - 1 : species - 1)
                                : (species + 1 >= NUM_SPECIES ? 1 : species + 1);
    while (!GiveSpeciesUsable(species));
    CtrSettings_SetInt("gm_species", species);
}

static const u8 *GiveSpeciesText(void)
{
    static u8 name[POKEMON_NAME_LENGTH + 1];

    StringCopy(name, gSpeciesNames[GiveSpecies()]);
    return name;
}

static u8 GiveLevel(void)
{
    int level = CtrSettings_GetInt("gm_level", 5);

    return level >= 1 && level <= MAX_LEVEL ? level : 5;
}

/* Steps of 5 (1, 5, 10 ... 100), the way a cell is tapped. */
static void GiveLevelStep(int direction)
{
    int level = GiveLevel();

    if (direction < 0)
        level = level <= 1 ? MAX_LEVEL : level <= 5 ? 1 : (level - 1) / 5 * 5;
    else
        level = level >= MAX_LEVEL ? 1 : level < 5 ? 5 : level / 5 * 5 + 5;
    CtrSettings_SetInt("gm_level", level);
}

static const u8 *GiveLevelText(void)
{
    static u8 text[8];
    u8 *end = StringCopy(text, gText_LevelSymbol);

    ConvertIntToDecimalStringN(end, GiveLevel(), STR_CONV_MODE_LEFT_ALIGN, 3);
    return text;
}

static void GiveMonNow(void)
{
    struct Pokemon *mon = AllocZeroed(sizeof(struct Pokemon));
    u16 species = GiveSpecies();
    u32 otId = gSaveBlock2Ptr->playerTrainerId[0] | (gSaveBlock2Ptr->playerTrainerId[1] << 8)
             | (gSaveBlock2Ptr->playerTrainerId[2] << 16) | (gSaveBlock2Ptr->playerTrainerId[3] << 24);
    u32 personality;
    u8 given;

    if (mon == NULL)
    {
        PlaySE(SE_FAILURE);
        return;
    }
    personality = Random32();
    if (CtrSettings_GetInt("gm_shiny", 0) == 1)
        while (GET_SHINY_VALUE(otId, personality) >= SHINY_ODDS)
            personality = Random32();
    CreateMon(mon, species, GiveLevel(), USE_RANDOM_IVS, TRUE, personality, OT_ID_PLAYER_ID, 0);
    given = GiveMonToPlayer(mon);
    Free(mon);
    if (given == MON_CANT_GIVE)
    {
        PlaySE(SE_FAILURE);
        return;
    }
    GetSetPokedexFlag(SpeciesToNationalPokedexNum(species), FLAG_SET_SEEN);
    GetSetPokedexFlag(SpeciesToNationalPokedexNum(species), FLAG_SET_CAUGHT);
    PlaySE(SE_SUCCESS);
}

/* data/scripts/pc.inc */
extern const u8 EventScript_CtrOpenStorage[];
extern const u8 EventScript_CtrMoveRelearner[];

static const char *const sTapToOpen[] = {CTR_TEXT("TAP TO OPEN", "TOCA PARA ABRIR", "OUVRIR", "APRI", "TOQUE P/ ABRIR", "ÖFFNEN")};

static void RunFieldScript(const u8 *script)
{
    if (gMain.callback2 != CB2_Overworld || ArePlayerFieldControlsLocked() || ScriptContext_IsEnabled())
    {
        PlaySE(SE_FAILURE);
        return;
    }
    PlaySE(SE_SELECT);
    ScriptContext_SetupScript(script);
}

static void OpenPc(void)
{
    RunFieldScript(EventScript_CtrOpenStorage);
}

static void OpenMoveRelearner(void)
{
    RunFieldScript(EventScript_CtrMoveRelearner);
}

/* TIME OF DAY: each step sets the clock to that part of the day. */
static const char *const sTimesOfDay[] = {
    CTR_TEXT("MORNING", "MAÑANA", "MATIN", "MATTINA", "MANHÃ", "MORGEN"),
    CTR_TEXT("FORENOON", "MEDIA MAÑANA", "MATINÉE", "TARDA MATT.", "MEIO DA MANHÃ", "VORMITTAG"),
    CTR_TEXT("AFTERNOON", "TARDE", "APRÈS-MIDI", "POMERIGGIO", "TARDE", "NACHMITTAG"),
    CTR_TEXT("LATE AFTERN.", "TARDE TARDÍA", "FIN APRÈS-MIDI", "TARDO POM.", "FIM DA TARDE", "SPÄTER NACHM."),
    CTR_TEXT("EVENING", "ATARDECER", "SOIR", "SERA", "ANOITECER", "ABEND"),
    CTR_TEXT("NIGHT", "NOCHE", "NUIT", "NOTTE", "NOITE", "NACHT"),
};
/* 6:00 dawn, 10:00, 14:00, 16:30, 18:00 sunset, 22:00 (3ds_daynight.c's keys) */
static const unsigned short sTimeOfDayMinutes[] = {360, 600, 840, 990, 1080, 1320};

static void SetTimeOfDay(void)
{
    int value = CtrExtras_Get("time_of_day");

    if (value < 0 || value >= (int)ARRAY_COUNT(sTimeOfDayMinutes))
        value = 0;
    PlaySE(CtrDayNight_SetTime(sTimeOfDayMinutes[value] * 60u) ? SE_SELECT : SE_FAILURE);
}

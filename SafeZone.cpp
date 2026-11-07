#define _D2VARS_H

#include "DLLmain.h"
#include "D2Client.h"
#include "CoreArchive.h"
#include "ui.h"
#include "SafeZone.h"
#include "ModImports.h"

//  SafeZone.cpp -- mod, not part of the original D2Client.dll: the client side of D2MOO's town-like safe zones. See SafeZone.h.

using SafeZoneGetPresetIdFn = int(__stdcall *)(D2ActiveRoomStrc *pRoom);
using SafeZoneGetEntryNameFn = const Unicode *(__stdcall *)(int nLvlPrestId);
using SafeZoneGetEntryFileFn = const char *(__stdcall *)(int nLvlPrestId);

static bool sgbSafeZoneExportsResolved = false;
static SafeZoneGetPresetIdFn sgpfnGetPresetIdFromRoom = NULL;
static SafeZoneGetEntryNameFn sgpfnGetEntryName = NULL;
static SafeZoneGetEntryFileFn sgpfnGetEntryFile = NULL;

//  The entry title's state: the level and zone the player was in last frame, the frame count of the title being shown (120 = none) and its
//  image, when the zone has one.
static int32_t sgnLastLevelId = -1;
static int32_t sgnLastPresetId = -1;
static int32_t sgnTitlePresetId = -1;
static int32_t sgnTitleFrame = 120;
static D2CellFileStrc *sgpTitleImage = NULL;
static bool sgbTitleImageTried = false;

//  Looks the exports up once. They are all or nothing: a D2Common.dll with only some of them is treated as having none.
static bool SAFEZONE_ResolveExports()
{
    if (!sgbSafeZoneExportsResolved)
    {
        sgbSafeZoneExportsResolved = true;

        SafeZoneGetPresetIdFn pfnGetPresetId = (SafeZoneGetPresetIdFn)MODIMPORTS_FindD2CommonExport("DUNGEON_GetSafeZonePresetIdFromRoom");
        SafeZoneGetEntryNameFn pfnGetEntryName = (SafeZoneGetEntryNameFn)MODIMPORTS_FindD2CommonExport("DUNGEON_GetSafeZoneEntryName");
        SafeZoneGetEntryFileFn pfnGetEntryFile = (SafeZoneGetEntryFileFn)MODIMPORTS_FindD2CommonExport("DUNGEON_GetSafeZoneEntryFile");
        if (pfnGetPresetId && pfnGetEntryName && pfnGetEntryFile)
        {
            sgpfnGetPresetIdFromRoom = pfnGetPresetId;
            sgpfnGetEntryName = pfnGetEntryName;
            sgpfnGetEntryFile = pfnGetEntryFile;
        }
    }

    return sgpfnGetPresetIdFromRoom != NULL;
}

int32_t __fastcall SAFEZONE_GetPresetIdFromRoom(D2ActiveRoomStrc *pRoom)
{
    if (!pRoom || !SAFEZONE_ResolveExports())
        return -1;

    return sgpfnGetPresetIdFromRoom(pRoom);
}

const Unicode *__fastcall SAFEZONE_GetEntryName(int32_t nLvlPrestId)
{
    if (nLvlPrestId < 0 || !SAFEZONE_ResolveExports())
        return NULL;

    return sgpfnGetEntryName(nLvlPrestId);
}

static const char *SAFEZONE_GetEntryFile(int32_t nLvlPrestId)
{
    if (nLvlPrestId < 0 || !SAFEZONE_ResolveExports())
        return NULL;

    return sgpfnGetEntryFile(nLvlPrestId);
}

const Unicode *__fastcall SAFEZONE_GetAutomapName(D2ActiveRoomStrc *pRoom)
{
    return SAFEZONE_GetEntryName(SAFEZONE_GetPresetIdFromRoom(pRoom));
}

void __fastcall SAFEZONE_FreeEntryTitle()
{
    if (sgpTitleImage)
    {
        ARCHIVE_CellFileFree(sgpTitleImage);
        sgpTitleImage = NULL;
    }
    sgbTitleImageTried = false;
}

//  The title image, loaded on first use from DATA\LOCAL\UI\<language>\<ACTn or EXPANSION>\<EntryFile>, the folder the level titles of the
//  zone's level use. NULL when the zone has no EntryFile or the file cannot be loaded.
static D2CellFileStrc *SAFEZONE_GetTitleImage(int32_t nLevelId, int32_t nLvlPrestId)
{
    if (sgbTitleImageTried)
        return sgpTitleImage;

    sgbTitleImageTried = true;

    const char *szEntryFile = SAFEZONE_GetEntryFile(nLvlPrestId);
    if (!szEntryFile || !szEntryFile[0])
        return NULL;

    char szActFolder[32];
    if (nLevelId < LEVEL_HARROGATH)
        sprintf(szActFolder, "ACT%d", D2COMMON_DRLG_GetActNoFromLevelId((D2C_Levels)nLevelId) + 1);
    else
        sprintf(szActFolder, "%s", gszExpansion);

    //  A language code is three characters plus the NUL.
    char szLanguage[4] = {};
    D2LANG_10006(szLanguage, 0);

    char szPath[MAX_PATH];
    snprintf(szPath, sizeof(szPath), "DATA\\LOCAL\\UI\\%s\\%s\\%s", szLanguage, szActFolder, szEntryFile);

    sgpTitleImage = ARCHIVE_CellFileLoad(szPath, 0);
    return sgpTitleImage;
}

void __fastcall SAFEZONE_UpdateEntryTitle(D2ActiveRoomStrc *pPlayerRoom, int32_t bLevelTitleShowing)
{
    if (!pPlayerRoom)
        return;

    const int32_t nLevelId = D2COMMON_DUNGEON_GetLevelIdFromRoom(pPlayerRoom);
    const int32_t nPresetId = SAFEZONE_GetPresetIdFromRoom(pPlayerRoom);

    if (nLevelId != sgnLastLevelId)
    {
        //  A new level shows the level's own title. A zone the player arrives in directly (a town portal back into it) is not announced.
        sgnLastLevelId = nLevelId;
        sgnTitleFrame = 120;
        SAFEZONE_FreeEntryTitle();
    }
    else if (nPresetId != -1 && nPresetId != sgnLastPresetId)
    {
        sgnTitlePresetId = nPresetId;
        sgnTitleFrame = 0;
        SAFEZONE_FreeEntryTitle();
    }
    sgnLastPresetId = nPresetId;

    if (sgnTitleFrame >= 120 || bLevelTitleShowing)
        return;

    ++sgnTitleFrame;

    if (D2CellFileStrc *pImage = SAFEZONE_GetTitleImage(nLevelId, sgnTitlePresetId))
    {
        //  Where and how the level titles are drawn (UIPANELS_DrawInGamePanels).
        D2WIN_10134_DrawCellFile(pImage, gnScreenWidth / 2, gnScreenHeight / 2 - 140, 1, (DrawMode)5, 1);
    }
    else if (const Unicode *pwszName = SAFEZONE_GetEntryName(sgnTitlePresetId))
    {
        const Font nPreviousFont = D2WIN_10127_SetFont(D2FONT_FONT30);
        const int32_t nTextWidth = D2WIN_10121_GetTextWidth(pwszName);
        D2WIN_10117_DrawText(pwszName, (gnScreenWidth - nTextWidth) / 2, gnScreenHeight / 2 - 140, 4, 0);
        D2WIN_10127_SetFont(nPreviousFont);
    }

    if (sgnTitleFrame >= 120)
        SAFEZONE_FreeEntryTitle();
}

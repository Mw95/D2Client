#define _D2VARS_H

#include "DLLmain.h"
#include "D2Client.h"
#include "ItemAppearance.h"
#include "ItemAppearancePicker.h"

//  ItemAppearancePicker.cpp -- the look picker of the item appearance feature. Not part of the original D2Client.dll; see ItemAppearancePicker.h.
//  This half is the list: which looks the item on the anvil can take, deduplicated by graphic, with their icons.


//  Loads a row's inventory graphic, "DATA\GLOBAL\ITEMS\<invfile>", and measures its first frame. NULL when the row names no file.
static D2CellFileStrc *ITEMAPPEARANCE_LoadIcon(const D2ItemsTxt *pRecord, int32_t *pnWidth, int32_t *pnHeight)
{
    char szPath[MAX_PATH];
    D2CellFileStrc *pCellFile;
    D2GfxDataStrc tCelContext;
    D2GfxCellStrc *pCel;

    *pnWidth = 0;
    *pnHeight = 0;
    if (!pRecord || !pRecord->szInvFile[0])
        return NULL;

    wsprintfA(szPath, "DATA\\GLOBAL\\ITEMS\\%.32s", pRecord->szInvFile);
    pCellFile = ARCHIVE_CellFileLoad(szPath, 0);
    if (!pCellFile)
        return NULL;

    memset(&tCelContext, 0, sizeof(tCelContext));
    tCelContext.pCellFile = pCellFile;
    tCelContext.nDirection = 0;
    tCelContext.nFrame = 0;
    pCel = D2CMP_CELCMP_CelGetHandle(&tCelContext);
    if (pCel)
    {
        *pnWidth = D2CMP_CELCMP_CelGetWidth(pCel);
        *pnHeight = D2CMP_CELCMP_CelGetHeight(pCel);
    }
    return pCellFile;
}


static D2ItemAppearanceLookStrc *ITEMAPPEARANCE_AddLook(int32_t nClassId, BOOL bOriginal)
{
    D2ItemAppearanceLookStrc *pLook;

    if (gnItemAppearanceLookCount >= ITEMAPPEARANCE_MAX_LOOKS)
        return NULL;

    pLook = &gaItemAppearanceLooks[gnItemAppearanceLookCount++];
    memset(pLook, 0, sizeof(*pLook));
    pLook->nClassId = nClassId;
    pLook->nNameClassIds[0] = nClassId;
    pLook->nNameCount = 1;
    pLook->nRowCount = 1;
    pLook->bOriginal = bOriginal;
    pLook->pIcon = ITEMAPPEARANCE_LoadIcon(D2COMMON_DATATBLS_GetItemsTxtRecord(nClassId), &pLook->nIconWidth, &pLook->nIconHeight);
    return pLook;
}


void __cdecl ITEMAPPEARANCE_FreeLookList(void)
{
    int32_t i;

    for (i = 0; i < gnItemAppearanceLookCount; ++i)
    {
        if (gaItemAppearanceLooks[i].pIcon)
        {
            ARCHIVE_CellFileFree(gaItemAppearanceLooks[i].pIcon);
            gaItemAppearanceLooks[i].pIcon = NULL;
        }
    }
    gnItemAppearanceLookCount = 0;
    gnItemAppearancePickerItemGUID = 0;
}


//  Every look pItem may take, in items.txt order (normal, exceptional, elite within each kind), one entry per graphic: a row that draws like
//  an entry already listed only adds its name to it. First comes "Original look" when the item has a look of its own to go back to. Answers
//  the number of entries.
int32_t __fastcall ITEMAPPEARANCE_BuildLookList(D2UnitStrc *pPlayer, D2UnitStrc *pItem)
{
    D2ItemAppearanceLookStrc *pLook;
    D2ItemsTxt *pRecord;
    int32_t nRows;
    int32_t nRow;
    int32_t i;

    ITEMAPPEARANCE_FreeLookList();
    if (!pPlayer || !pItem)
        return 0;

    gnItemAppearancePickerItemGUID = pItem->dwUnitId;

    if (ITEMAPPEARANCE_CanApplyLook(pPlayer, pItem, (int32_t)pItem->dwClassId) == ITEMAPPEARANCE_OK)
        ITEMAPPEARANCE_AddLook((int32_t)pItem->dwClassId, TRUE);

    nRows = sgptDataTables->pItemDataTables.nItemsTxtRecordCount;
    for (nRow = 0; nRow < nRows; ++nRow)
    {
        if (nRow == (int32_t)pItem->dwClassId || ITEMAPPEARANCE_CanApplyLook(pPlayer, pItem, nRow) != ITEMAPPEARANCE_OK)
            continue;

        pRecord = D2COMMON_DATATBLS_GetItemsTxtRecord(nRow);
        pLook = NULL;
        for (i = 0; i < gnItemAppearanceLookCount; ++i)
        {
            if (!gaItemAppearanceLooks[i].bOriginal && ITEMAPPEARANCE_HaveSameGraphic(D2COMMON_DATATBLS_GetItemsTxtRecord(gaItemAppearanceLooks[i].nClassId), pRecord))
            {
                pLook = &gaItemAppearanceLooks[i];
                break;
            }
        }

        if (pLook)
        {
            if (pLook->nNameCount < ITEMAPPEARANCE_MAX_NAMES_PER_LOOK)
                pLook->nNameClassIds[pLook->nNameCount++] = nRow;
            ++pLook->nRowCount;
        }
        else if (!ITEMAPPEARANCE_AddLook(nRow, FALSE))
        {
            break;
        }
    }

    return gnItemAppearanceLookCount;
}


//  The entry's label: "Original look", or the names of the rows sharing the graphic, "A / B / C", with "..." when more share it.
void __fastcall ITEMAPPEARANCE_GetLookLabel(const D2ItemAppearanceLookStrc *pLook, Unicode *wszOut, int32_t nCapacity)
{
    Unicode wszScratch[16];
    const Unicode *pwszName;
    D2ItemsTxt *pRecord;
    int32_t i;

    wszOut[0].ch = 0;
    if (!pLook)
        return;

    if (pLook->bOriginal)
    {
        Unicode::win2Unicode(wszOut, "Original look", nCapacity - 1);
        return;
    }

    for (i = 0; i < pLook->nNameCount; ++i)
    {
        pRecord = D2COMMON_DATATBLS_GetItemsTxtRecord(pLook->nNameClassIds[i]);
        if (!pRecord)
            continue;
        pwszName = D2LANG_GetStringFromTblIndex((D2C_StringIndices)pRecord->wNameStr);
        if (!pwszName || Unicode::strlen(wszOut) + Unicode::strlen(pwszName) + 4 >= nCapacity)
            break;
        if (i)
        {
            Unicode::win2Unicode(wszScratch, " / ", 15);
            Unicode::strcat(wszOut, wszScratch);
        }
        Unicode::strcat(wszOut, pwszName);
    }

    if (pLook->nRowCount > pLook->nNameCount && Unicode::strlen(wszOut) + 4 < nCapacity)
    {
        Unicode::win2Unicode(wszScratch, " ...", 15);
        Unicode::strcat(wszOut, wszScratch);
    }
}

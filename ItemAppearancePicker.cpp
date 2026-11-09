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


//  ---- The panel ----
//  Layout, in the left panel's 640 x 480 coordinates: a black box with a title, the grid of looks, the label of the hovered (else the
//  selected) look, its price, a row with the page buttons and a row with "Transmogrify" and "Back".

static const int32_t PICKER_LEFT = 10;
static const int32_t PICKER_TOP = 50;
static const int32_t PICKER_RIGHT = 310;
static const int32_t PICKER_BOTTOM = 440;
static const int32_t PICKER_GRID_LEFT = 18;
static const int32_t PICKER_GRID_TOP = 84;
static const int32_t PICKER_GRID_WIDTH = 284;
static const int32_t PICKER_GRID_HEIGHT = 240;
static const int32_t PICKER_LABEL_Y = 344;
static const int32_t PICKER_PRICE_Y = 384;
static const int32_t PICKER_PAGE_TOP = 392;
static const int32_t PICKER_ACTION_TOP = 416;
static const int32_t PICKER_BUTTON_HEIGHT = 20;

struct D2ItemAppearancePickerRectStrc
{
    int32_t nLeft;
    int32_t nTop;
    int32_t nRight;
    int32_t nBottom;
};

static const D2ItemAppearancePickerRectStrc gPickerPrevButton = { 18, PICKER_PAGE_TOP, 58, PICKER_PAGE_TOP + PICKER_BUTTON_HEIGHT };
static const D2ItemAppearancePickerRectStrc gPickerNextButton = { 262, PICKER_PAGE_TOP, 302, PICKER_PAGE_TOP + PICKER_BUTTON_HEIGHT };
static const D2ItemAppearancePickerRectStrc gPickerConfirmButton = { 30, PICKER_ACTION_TOP, 150, PICKER_ACTION_TOP + PICKER_BUTTON_HEIGHT };
static const D2ItemAppearancePickerRectStrc gPickerBackButton = { 170, PICKER_ACTION_TOP, 290, PICKER_ACTION_TOP + PICKER_BUTTON_HEIGHT };


static BOOL PICKER_IsInside(const D2ItemAppearancePickerRectStrc *pRect, int32_t nX, int32_t nY)
{
    return nX >= pRect->nLeft && nX <= pRect->nRight && nY >= pRect->nTop && nY <= pRect->nBottom;
}


//  One slot fits the largest icon of the list, so a page of helms holds many more looks than a page of polearms.
static void PICKER_GetGrid(int32_t *pnSlotWidth, int32_t *pnSlotHeight, int32_t *pnColumns, int32_t *pnPerPage)
{
    int32_t nMaxWidth = 28;
    int32_t nMaxHeight = 28;
    int32_t nRows;
    int32_t i;

    for (i = 0; i < gnItemAppearanceLookCount; ++i)
    {
        if (gaItemAppearanceLooks[i].nIconWidth > nMaxWidth)
            nMaxWidth = gaItemAppearanceLooks[i].nIconWidth;
        if (gaItemAppearanceLooks[i].nIconHeight > nMaxHeight)
            nMaxHeight = gaItemAppearanceLooks[i].nIconHeight;
    }

    *pnSlotWidth = nMaxWidth + 8;
    *pnSlotHeight = nMaxHeight + 8;
    *pnColumns = PICKER_GRID_WIDTH / *pnSlotWidth;
    if (*pnColumns < 1)
        *pnColumns = 1;
    nRows = PICKER_GRID_HEIGHT / *pnSlotHeight;
    if (nRows < 1)
        nRows = 1;
    *pnPerPage = *pnColumns * nRows;
}


static int32_t PICKER_GetPageCount(void)
{
    int32_t nSlotWidth, nSlotHeight, nColumns, nPerPage;

    PICKER_GetGrid(&nSlotWidth, &nSlotHeight, &nColumns, &nPerPage);
    return gnItemAppearanceLookCount ? (gnItemAppearanceLookCount + nPerPage - 1) / nPerPage : 1;
}


//  The look under a point of the grid on the current page, or -1.
static int32_t PICKER_HitTestGrid(int32_t nX, int32_t nY)
{
    int32_t nSlotWidth, nSlotHeight, nColumns, nPerPage;
    int32_t nColumn;
    int32_t nRow;
    int32_t nIndex;

    if (nX < PICKER_GRID_LEFT || nY < PICKER_GRID_TOP || nX >= PICKER_GRID_LEFT + PICKER_GRID_WIDTH || nY >= PICKER_GRID_TOP + PICKER_GRID_HEIGHT)
        return -1;

    PICKER_GetGrid(&nSlotWidth, &nSlotHeight, &nColumns, &nPerPage);
    nColumn = (nX - PICKER_GRID_LEFT) / nSlotWidth;
    nRow = (nY - PICKER_GRID_TOP) / nSlotHeight;
    if (nColumn >= nColumns || nRow * nColumns + nColumn >= nPerPage)
        return -1;

    nIndex = gnItemAppearancePickerPage * nPerPage + nRow * nColumns + nColumn;
    return nIndex < gnItemAppearanceLookCount ? nIndex : -1;
}


BOOL __fastcall ITEMAPPEARANCE_PickerOpen(D2UnitStrc *pPlayer, D2UnitStrc *pItem)
{
    if (!ITEMAPPEARANCE_BuildLookList(pPlayer, pItem))
    {
        ITEMAPPEARANCE_FreeLookList();
        return FALSE;
    }

    gbItemAppearancePickerOpen = TRUE;
    gnItemAppearancePickerPage = 0;
    gnItemAppearancePickerSelected = -1;
    gnItemAppearancePickerHover = -1;
    return TRUE;
}


void __cdecl ITEMAPPEARANCE_PickerClose(void)
{
    gbItemAppearancePickerOpen = FALSE;
    gnItemAppearancePickerSelected = -1;
    gnItemAppearancePickerHover = -1;
    ITEMAPPEARANCE_FreeLookList();
}


int32_t __cdecl ITEMAPPEARANCE_PickerGetSelectedClassId(void)
{
    if (gnItemAppearancePickerSelected < 0 || gnItemAppearancePickerSelected >= gnItemAppearanceLookCount)
        return -1;
    return gaItemAppearanceLooks[gnItemAppearancePickerSelected].nClassId;
}


void __fastcall ITEMAPPEARANCE_PickerOnMouseMove(int32_t nX, int32_t nY)
{
    if (gbItemAppearancePickerOpen)
        gnItemAppearancePickerHover = PICKER_HitTestGrid(nX, nY);
}


D2ItemAppearancePickerClick __fastcall ITEMAPPEARANCE_PickerOnClick(int32_t nX, int32_t nY)
{
    int32_t nIndex;
    int32_t nPages;

    if (!gbItemAppearancePickerOpen)
        return ITEMAPPEARANCE_PICKER_NONE;

    nIndex = PICKER_HitTestGrid(nX, nY);
    if (nIndex >= 0)
    {
        gnItemAppearancePickerSelected = nIndex;
        SOUNDINST_PlaySound(SOUND_CURSOR_SELECT, NULL, 0, 0, 0);
        return ITEMAPPEARANCE_PICKER_HANDLED;
    }

    nPages = PICKER_GetPageCount();
    if (PICKER_IsInside(&gPickerPrevButton, nX, nY))
    {
        if (gnItemAppearancePickerPage > 0)
        {
            --gnItemAppearancePickerPage;
            SOUNDINST_PlaySound(SOUND_CURSOR_PASS, NULL, 0, 0, 0);
        }
        return ITEMAPPEARANCE_PICKER_HANDLED;
    }
    if (PICKER_IsInside(&gPickerNextButton, nX, nY))
    {
        if (gnItemAppearancePickerPage + 1 < nPages)
        {
            ++gnItemAppearancePickerPage;
            SOUNDINST_PlaySound(SOUND_CURSOR_PASS, NULL, 0, 0, 0);
        }
        return ITEMAPPEARANCE_PICKER_HANDLED;
    }
    if (PICKER_IsInside(&gPickerConfirmButton, nX, nY))
    {
        if (gnItemAppearancePickerSelected < 0)
        {
            SOUNDINST_PlaySound(SOUND_CURSOR_ERROR, NULL, 0, 0, 0);
            return ITEMAPPEARANCE_PICKER_HANDLED;
        }
        return ITEMAPPEARANCE_PICKER_CONFIRM;
    }
    if (PICKER_IsInside(&gPickerBackButton, nX, nY))
        return ITEMAPPEARANCE_PICKER_CANCEL;

    //  Inside the box the click is the panel's; outside it the caller decides.
    if (nX >= PICKER_LEFT && nX <= PICKER_RIGHT && nY >= PICKER_TOP && nY <= PICKER_BOTTOM)
        return ITEMAPPEARANCE_PICKER_HANDLED;
    return ITEMAPPEARANCE_PICKER_NONE;
}


static void PICKER_DrawButton(const D2ItemAppearancePickerRectStrc *pRect, const char *szText, BOOL bEnabled, int32_t nMouseX, int32_t nMouseY)
{
    Unicode wszText[32];
    const BOOL bHover = bEnabled && PICKER_IsInside(pRect, nMouseX, nMouseY);

    GFX_DrawFilledRect(pRect->nLeft, pRect->nTop, pRect->nRight - pRect->nLeft, pRect->nBottom - pRect->nTop,
                       D2WIN_10034_MixRGB(bHover ? 90 : 50, bHover ? 70 : 40, bHover ? 30 : 20), DRAWMODE_NORMAL);
    Unicode::win2Unicode(wszText, szText, 31);
    D2WIN_10117_DrawText(wszText, (pRect->nLeft + pRect->nRight) / 2 - D2WIN_10121_GetTextWidth(wszText) / 2, pRect->nBottom - 3,
                         bEnabled ? 0 : 5, 0);
}


void __cdecl ITEMAPPEARANCE_PickerDraw(void)
{
    int32_t nSlotWidth, nSlotHeight, nColumns, nPerPage;
    int32_t nPrevFont;
    int32_t nPages;
    int32_t nFirst;
    int32_t nIndex;
    int32_t nX;
    int32_t nY;
    int32_t nMouseX;
    int32_t nMouseY;
    int32_t nLabelIndex;
    int32_t nLines;
    const D2ItemAppearanceLookStrc *pLook;
    D2GfxDataStrc tCelContext;
    D2UnitStrc *pItem;
    D2SplittedTextStrc *pSplit;
    D2SplittedTextStrc *pNext;
    Unicode wszText[256];
    char szText[64];

    if (!gbItemAppearancePickerOpen)
        return;

    nPrevFont = D2WIN_10127_SetFont(D2FONT_FONT16);
    nMouseX = CURSOR_GetMouseX();
    nMouseY = CURSOR_GetMouseY();

    GFX_DrawFilledRect(PICKER_LEFT, PICKER_TOP, PICKER_RIGHT - PICKER_LEFT, PICKER_BOTTOM - PICKER_TOP, D2WIN_10034_MixRGB(0, 0, 0), DRAWMODE_NORMAL);

    Unicode::win2Unicode(wszText, "Choose a look", 255);
    D2WIN_10117_DrawText(wszText, 160 - D2WIN_10121_GetTextWidth(wszText) / 2, PICKER_TOP + 24, 4, 0);

    //  The grid: the selected slot filled, the hovered one lighter, every icon centred in its slot.
    PICKER_GetGrid(&nSlotWidth, &nSlotHeight, &nColumns, &nPerPage);
    nPages = PICKER_GetPageCount();
    if (gnItemAppearancePickerPage >= nPages)
        gnItemAppearancePickerPage = nPages - 1;
    nFirst = gnItemAppearancePickerPage * nPerPage;
    for (nIndex = nFirst; nIndex < gnItemAppearanceLookCount && nIndex < nFirst + nPerPage; ++nIndex)
    {
        pLook = &gaItemAppearanceLooks[nIndex];
        nX = PICKER_GRID_LEFT + ((nIndex - nFirst) % nColumns) * nSlotWidth;
        nY = PICKER_GRID_TOP + ((nIndex - nFirst) / nColumns) * nSlotHeight;

        if (nIndex == gnItemAppearancePickerSelected)
            GFX_DrawFilledRect(nX + 1, nY + 1, nSlotWidth - 2, nSlotHeight - 2, D2WIN_10034_MixRGB(40, 60, 120), DRAWMODE_NORMAL);
        else if (nIndex == gnItemAppearancePickerHover)
            GFX_DrawFilledRect(nX + 1, nY + 1, nSlotWidth - 2, nSlotHeight - 2, D2WIN_10034_MixRGB(40, 40, 40), DRAWMODE_NORMAL);
        else
            GFX_DrawFilledRect(nX + 1, nY + 1, nSlotWidth - 2, nSlotHeight - 2, D2WIN_10034_MixRGB(20, 20, 20), DRAWMODE_NORMAL);

        if (pLook->pIcon)
        {
            memset(&tCelContext, 0, sizeof(tCelContext));
            tCelContext.pCellFile = pLook->pIcon;
            tCelContext.nDirection = 0;
            tCelContext.nFrame = 0;
            //  A cel is drawn up from its Y: the bottom of the centred icon.
            D2GFX_TEXTURE_CelDraw(&tCelContext, nX + (nSlotWidth - pLook->nIconWidth) / 2, nY + (nSlotHeight + pLook->nIconHeight) / 2,
                                  0xFFFFFFFF, DRAWMODE_NORMAL, NULL);
        }
    }

    //  The label of the hovered look, else the selected one, split to the box; then the price.
    nLabelIndex = gnItemAppearancePickerHover >= 0 ? gnItemAppearancePickerHover : gnItemAppearancePickerSelected;
    if (nLabelIndex >= 0 && nLabelIndex < gnItemAppearanceLookCount)
    {
        pLook = &gaItemAppearanceLooks[nLabelIndex];
        ITEMAPPEARANCE_GetLookLabel(pLook, wszText, 256);
        pSplit = D2WIN_10199(wszText, &nLines, 280);
        nY = PICKER_LABEL_Y;
        while (pSplit)
        {
            if (nY <= PICKER_LABEL_Y + 2 * 16)
                D2WIN_10117_DrawText(pSplit->wszLine, 160 - D2WIN_10121_GetTextWidth(pSplit->wszLine) / 2, nY, 0, 0);
            pNext = pSplit->pNextLine;
            FOG_Free(pSplit->wszLine, __FILE__, __LINE__, 0);
            FOG_Free(pSplit, __FILE__, __LINE__, 0);
            nY += 16;
            pSplit = pNext;
        }

        pItem = CUNIT_FindUnitByGuidAndType(gnItemAppearancePickerItemGUID, UNIT_ITEM);
        if (pItem)
        {
            STORM_SStrPrintf(szText, sizeof(szText), "Cost: %d gold", ITEMAPPEARANCE_GetCost(pItem, pLook->bOriginal));
            Unicode::win2Unicode(wszText, szText, 255);
            D2WIN_10117_DrawText(wszText, 160 - D2WIN_10121_GetTextWidth(wszText) / 2, PICKER_PRICE_Y, 9, 0);
        }
    }

    PICKER_DrawButton(&gPickerPrevButton, "<", gnItemAppearancePickerPage > 0, nMouseX, nMouseY);
    PICKER_DrawButton(&gPickerNextButton, ">", gnItemAppearancePickerPage + 1 < nPages, nMouseX, nMouseY);
    STORM_SStrPrintf(szText, sizeof(szText), "%d / %d", gnItemAppearancePickerPage + 1, nPages);
    Unicode::win2Unicode(wszText, szText, 255);
    D2WIN_10117_DrawText(wszText, 160 - D2WIN_10121_GetTextWidth(wszText) / 2, PICKER_PAGE_TOP + PICKER_BUTTON_HEIGHT - 3, 0, 0);
    PICKER_DrawButton(&gPickerConfirmButton, "Transmogrify", gnItemAppearancePickerSelected >= 0, nMouseX, nMouseY);
    PICKER_DrawButton(&gPickerBackButton, "Back", TRUE, nMouseX, nMouseY);

    D2WIN_10127_SetFont((Font)nPrevFont);
}

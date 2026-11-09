//  ItemCompare.cpp -- the equipped item's description beside the hovered item's. Not a unit of the original D2Client.dll: a quality-of-life
//  addition, switched by gnQolItemCompareMode (Qol.h). With the mode QOLMODE_OFF nothing here draws anything.
//
//  When the description of an item that is not worn is drawn, the items worn where it would go are found on the player -- or on the
//  mercenary while the mercenary's panel is open -- and their descriptions are built by INV_DrawItemDescription itself, with the hover item
//  swapped for each and its one framed-text draw taken here instead (ITEMCOMPARE_OnDescriptionText). The texts get an "Equipped" header line
//  and are laid out beside the hovered tooltip.
//
//  D2Win keeps one framed text per frame (D2WIN_10129_DrawFramedText stores it, D2WIN_10124 draws it, once, near the end of the UI): a
//  second tooltip cannot simply be stored. ITEMCOMPARE_DrawPending runs right before that D2WIN_10124 and draws each equipped tooltip
//  through the same slot -- store, D2WIN_10124 -- then stores the hovered tooltip again, so it is drawn last, as it always is.
//
//  What is compared:
//      rings             both ring slots
//      a two-handed weapon     everything in both hands (weapon and shield)
//      a one-handed weapon     the weapons in either hand (a two-handed one, or both of a dual wielder's)
//      a shield          the shields in either hand
//      anything else     the slot its item type names (itemtypes.txt BodyLoc1/BodyLoc2)
//  The inactive weapon set is not compared. Nothing is compared for a worn item, an item that cannot be worn, or when the tooltips do not fit
//  the screen side by side or stacked on either side of the hovered one.

#include "DLLmain.h"
#include "D2Client.h"
#include "WINMAIN.h"
#include "CoreArchive.h"
#include "ui.h"
#include "npcmenu.h"
#include "panel.h"
#include "inv.h"
#include "ItemCompare.h"


constexpr int32_t ITEMCOMPARE_MAX_ITEMS = 2;
constexpr int32_t ITEMCOMPARE_TEXT_LENGTH = 1024;      //  what D2Win's framed text holds, with its terminator
constexpr int32_t ITEMCOMPARE_GAP = 4;                 //  pixels between two tooltips
constexpr uint32_t ITEMCOMPARE_HEADER_COLOR = 0;       //  white


struct D2ItemCompareTooltipStrc
{
    Unicode wszText[ITEMCOMPARE_TEXT_LENGTH];
    int32_t nX;             //  as D2WIN_10129_DrawFramedText takes it
    int32_t nY;
    int32_t nColor;
    int32_t nAlign;
    int32_t nWidth;         //  the box D2Win draws: the widest line + 8, the lines' height
    int32_t nHeight;
};

//  The hovered item's tooltip, as last drawn by the description code this frame; the one being captured; the ones to draw.
static D2ItemCompareTooltipStrc gtHoveredTooltip;
static BOOL gbHoveredDrawn = FALSE;
static BOOL gbCapturing = FALSE;
static D2ItemCompareTooltipStrc gtCapturedTooltip;
static BOOL gbCaptured = FALSE;
static D2ItemCompareTooltipStrc gaPendingTooltips[ITEMCOMPARE_MAX_ITEMS];
static int32_t gnPendingTooltips = 0;


BOOL __fastcall ITEMCOMPARE_OnDescriptionText(const Unicode *wszText, int32_t nX, int32_t nY, int32_t nColor, int32_t nAlign)
{
    D2ItemCompareTooltipStrc *pTooltip = gbCapturing ? &gtCapturedTooltip : &gtHoveredTooltip;

    if (wszText && Unicode::strlen(wszText) < ITEMCOMPARE_TEXT_LENGTH)
        Unicode::strcpy(pTooltip->wszText, wszText);
    else
        pTooltip->wszText[0].ch = 0;
    pTooltip->nX = nX;
    pTooltip->nY = nY;
    pTooltip->nColor = nColor;
    pTooltip->nAlign = nAlign;

    if (gbCapturing)
    {
        gbCaptured = TRUE;
        return TRUE;
    }

    gbHoveredDrawn = TRUE;
    return FALSE;
}


//  ---- what is worn where the item would go ----

static BOOL __fastcall ITEMCOMPARE_AddUnique(D2UnitStrc **ppItems, int32_t *pnCount, D2UnitStrc *pItem)
{
    if (!pItem || *pnCount >= ITEMCOMPARE_MAX_ITEMS)
        return FALSE;
    for (int32_t i = 0; i < *pnCount; ++i)
    {
        if (ppItems[i] == pItem)
            return FALSE;
    }
    ppItems[(*pnCount)++] = pItem;
    return TRUE;
}

static int32_t __fastcall ITEMCOMPARE_FindEquipped(D2UnitStrc *pOwner, D2UnitStrc *pItem, D2UnitStrc **ppItems)
{
    int32_t nCount = 0;
    D2InventoryStrc *pInventory = pOwner->pInventory;
    if (!pInventory)
        return 0;

    const BOOL bHandItem = D2COMMON_ITEMS_CheckBodyLocation(pItem, BODYLOC_RARM) || D2COMMON_ITEMS_CheckBodyLocation(pItem, BODYLOC_LARM);
    if (bHandItem)
    {
        const BOOL bWeapon = D2COMMON_ITEMS_CheckItemTypeId(pItem, ITEMTYPE_WEAPON);
        const BOOL bTwoHanded = bWeapon && D2COMMON_ITEMS_CheckWeaponIfTwoHanded(pItem)
            && !(pOwner->dwUnitType == UNIT_PLAYER && D2COMMON_ITEMS_Is1Or2HandedForBarbarian(pOwner, pItem));

        for (int32_t nBodyLoc = BODYLOC_RARM; nBodyLoc <= BODYLOC_LARM; ++nBodyLoc)
        {
            D2UnitStrc *pEquipped = D2COMMON_INVENTORY_GetItemFromBodyLoc(pInventory, (D2C_PlayerBodyLocs)nBodyLoc);
            if (!pEquipped)
                continue;

            const BOOL bEquippedWeapon = D2COMMON_ITEMS_CheckItemTypeId(pEquipped, ITEMTYPE_WEAPON);
            if (bTwoHanded || (bWeapon && bEquippedWeapon) || (!bWeapon && !bEquippedWeapon))
                ITEMCOMPARE_AddUnique(ppItems, &nCount, pEquipped);
        }
        return nCount;
    }

    for (int32_t nBodyLoc = BODYLOC_HEAD; nBodyLoc <= BODYLOC_GLOVES; ++nBodyLoc)
    {
        if (nBodyLoc == BODYLOC_RARM || nBodyLoc == BODYLOC_LARM)
            continue;
        if (!D2COMMON_ITEMS_CheckBodyLocation(pItem, (uint8_t)nBodyLoc))
            continue;
        ITEMCOMPARE_AddUnique(ppItems, &nCount, D2COMMON_INVENTORY_GetItemFromBodyLoc(pInventory, (D2C_PlayerBodyLocs)nBodyLoc));
    }
    return nCount;
}

//  The mercenary, while its panel is open; the player otherwise.
static D2UnitStrc *__fastcall ITEMCOMPARE_GetCompareOwner()
{
    D2UnitStrc *pPlayer = CUNIT_GetPlayer();
    if (!pPlayer)
        return NULL;

    if (UI_GetUiWindowState(UI_MERCINV) && GAME_IsExpansionGame())
    {
        const D2UnitGUID dwMercGUID = ROSTERPETS_GetAssociatedUnitGUID(pPlayer, 7, 0);
        if (dwMercGUID != (D2UnitGUID)-1)
        {
            if (D2UnitStrc *pMerc = CUNIT_FindUnitByGuidAndType(dwMercGUID, UNIT_MONSTER))
                return pMerc;
        }
    }
    return pPlayer;
}


//  ---- building an equipped item's text ----

static void __fastcall ITEMCOMPARE_AppendHeader(Unicode *wszText)
{
    static const char szHeader[] = "Equipped";

    Unicode wszHeader[64];
    Unicode wszDigit[2];

    wszHeader[0].ch = 0;
    Unicode::strcat(wszHeader, D2LANG_GetStringFromTblIndex(STR_IDX_3998_newline));
    Unicode::strcat(wszHeader, D2LANG_GetStringFromTblIndex(STR_IDX_3994_colorcode));
    wszDigit[0].ch = (uint16_t)('0' + ITEMCOMPARE_HEADER_COLOR);
    wszDigit[1].ch = 0;
    Unicode::strcat(wszHeader, wszDigit);

    Unicode wszWord[16];

    Unicode::toUnicode(wszWord, szHeader, 15);
    Unicode::strcat(wszHeader, wszWord);

    //  the text is drawn last line on top: the header goes at the end. Without room for it the tooltip goes without.
    if (Unicode::strlen(wszText) + Unicode::strlen(wszHeader) < ITEMCOMPARE_TEXT_LENGTH)
        Unicode::strcat(wszText, wszHeader);
}

//  The description INV_DrawItemDescription builds for pEquipped worn by pOwner, taken rather than drawn. No shop footer: the inventory mode
//  is 0 while it is built.
static BOOL __fastcall ITEMCOMPARE_BuildTooltip(D2UnitStrc *pOwner, D2UnitStrc *pEquipped, D2ItemCompareTooltipStrc *pTooltip)
{
    D2UnitStrc *pSavedHoverItem = gpHoverItem;
    const int32_t nSavedInventoryMode = sgInventoryMode;
    const BOOL bSavedShowItemDesc = gbShowItemDesc;
    const BOOL bSavedShowSetItemDesc = gbShowSetItemDesc;

    gpHoverItem = pEquipped;
    sgInventoryMode = 0;
    gbShowItemDesc = TRUE;
    gbCapturing = TRUE;
    gbCaptured = FALSE;

    INV_DrawItemDescription(pOwner, FALSE);

    gbCapturing = FALSE;
    gpHoverItem = pSavedHoverItem;
    sgInventoryMode = nSavedInventoryMode;
    gbShowItemDesc = bSavedShowItemDesc;
    gbShowSetItemDesc = bSavedShowSetItemDesc;

    if (!gbCaptured || !gtCapturedTooltip.wszText[0].ch)
        return FALSE;

    Unicode::strcpy(pTooltip->wszText, gtCapturedTooltip.wszText);
    pTooltip->nColor = gtCapturedTooltip.nColor;
    pTooltip->nAlign = 0;
    ITEMCOMPARE_AppendHeader(pTooltip->wszText);
    D2WIN_10131_GetTextDimensions(pTooltip->wszText, &pTooltip->nWidth, &pTooltip->nHeight);
    pTooltip->nWidth += 8;
    return TRUE;
}


//  ---- the layout ----

//  The box D2Win draws a framed text in (D2Win_10124): centred on nX for align 1, at nX otherwise; its bottom at nY + 2; both kept on screen.
static void __fastcall ITEMCOMPARE_GetBox(const D2ItemCompareTooltipStrc *pTooltip, int32_t *pnLeft, int32_t *pnBottom)
{
    int32_t nLeft = pTooltip->nAlign == 1 ? pTooltip->nX - pTooltip->nWidth / 2 : pTooltip->nX;
    nLeft = std::clamp(nLeft, 0, std::max(0, gnScreenWidth - pTooltip->nWidth));
    *pnLeft = nLeft;
    *pnBottom = std::clamp(pTooltip->nY + 2, pTooltip->nHeight - 5, gnScreenHeight - 5);
}

//  Puts the tooltip's box at (nLeft, nBottom): what D2Win_10124 makes of nX = nLeft, align 0, nY = nBottom - 2.
static void __fastcall ITEMCOMPARE_Place(D2ItemCompareTooltipStrc *pTooltip, int32_t nLeft, int32_t nBottom)
{
    pTooltip->nX = nLeft;
    pTooltip->nY = nBottom - 2;
}

//  The tooltips in a row on one side of the hovered box, their bottoms with its bottom (raised where a tall one would leave the screen).
static BOOL __fastcall ITEMCOMPARE_LayoutRow(int32_t nCount, BOOL bLeft, int32_t nHoverLeft, int32_t nHoverRight, int32_t nHoverBottom)
{
    int32_t nTotalWidth = 0;
    for (int32_t i = 0; i < nCount; ++i)
    {
        if (gaPendingTooltips[i].nHeight > gnScreenHeight - 10)
            return FALSE;
        nTotalWidth += gaPendingTooltips[i].nWidth + ITEMCOMPARE_GAP;
    }

    if (bLeft ? nHoverLeft - nTotalWidth < 0 : nHoverRight + nTotalWidth > gnScreenWidth)
        return FALSE;

    int32_t nEdge = bLeft ? nHoverLeft : nHoverRight;
    for (int32_t i = 0; i < nCount; ++i)
    {
        D2ItemCompareTooltipStrc *pTooltip = &gaPendingTooltips[i];
        const int32_t nBottom = std::clamp(nHoverBottom, pTooltip->nHeight - 5, gnScreenHeight - 5);
        if (bLeft)
        {
            nEdge -= ITEMCOMPARE_GAP + pTooltip->nWidth;
            ITEMCOMPARE_Place(pTooltip, nEdge, nBottom);
        }
        else
        {
            ITEMCOMPARE_Place(pTooltip, nEdge + ITEMCOMPARE_GAP, nBottom);
            nEdge += ITEMCOMPARE_GAP + pTooltip->nWidth;
        }
    }
    return TRUE;
}

//  The tooltips stacked on one side of the hovered box, the first lowest, the stack's bottom with the hovered bottom where the screen allows.
static BOOL __fastcall ITEMCOMPARE_LayoutStack(int32_t nCount, BOOL bLeft, int32_t nHoverLeft, int32_t nHoverRight, int32_t nHoverBottom)
{
    int32_t nMaxWidth = 0;
    int32_t nTotalHeight = 0;
    for (int32_t i = 0; i < nCount; ++i)
    {
        nMaxWidth = std::max(nMaxWidth, gaPendingTooltips[i].nWidth);
        nTotalHeight += gaPendingTooltips[i].nHeight + (i ? ITEMCOMPARE_GAP : 0);
    }

    if (nTotalHeight > gnScreenHeight - 10)
        return FALSE;
    if (bLeft ? nHoverLeft - ITEMCOMPARE_GAP - nMaxWidth < 0 : nHoverRight + ITEMCOMPARE_GAP + nMaxWidth > gnScreenWidth)
        return FALSE;

    int32_t nBottom = std::clamp(nHoverBottom, nTotalHeight - 5, gnScreenHeight - 5);
    for (int32_t i = 0; i < nCount; ++i)
    {
        D2ItemCompareTooltipStrc *pTooltip = &gaPendingTooltips[i];
        const int32_t nLeft = bLeft ? nHoverLeft - ITEMCOMPARE_GAP - pTooltip->nWidth : nHoverRight + ITEMCOMPARE_GAP;
        ITEMCOMPARE_Place(pTooltip, nLeft, nBottom);
        nBottom -= pTooltip->nHeight + ITEMCOMPARE_GAP;
    }
    return TRUE;
}

//  Side by side on the roomier side first, then the other, then stacked likewise; FALSE when none fits.
static BOOL __fastcall ITEMCOMPARE_Layout(int32_t nCount)
{
    int32_t nHoverLeft;
    int32_t nHoverBottom;

    D2WIN_10131_GetTextDimensions(gtHoveredTooltip.wszText, &gtHoveredTooltip.nWidth, &gtHoveredTooltip.nHeight);
    gtHoveredTooltip.nWidth += 8;
    ITEMCOMPARE_GetBox(&gtHoveredTooltip, &nHoverLeft, &nHoverBottom);
    const int32_t nHoverRight = nHoverLeft + gtHoveredTooltip.nWidth;

    const BOOL bLeftFirst = nHoverLeft >= gnScreenWidth - nHoverRight;
    return ITEMCOMPARE_LayoutRow(nCount, bLeftFirst, nHoverLeft, nHoverRight, nHoverBottom)
        || ITEMCOMPARE_LayoutRow(nCount, !bLeftFirst, nHoverLeft, nHoverRight, nHoverBottom)
        || ITEMCOMPARE_LayoutStack(nCount, bLeftFirst, nHoverLeft, nHoverRight, nHoverBottom)
        || ITEMCOMPARE_LayoutStack(nCount, !bLeftFirst, nHoverLeft, nHoverRight, nHoverBottom);
}


void __fastcall ITEMCOMPARE_AfterDescription(D2UnitStrc *pDescUnit)
{
    UNREFERENCED_PARAMETER(pDescUnit);

    const BOOL bHoveredDrawn = gbHoveredDrawn;
    gbHoveredDrawn = FALSE;
    if (!bHoveredDrawn || gbCapturing || !gtHoveredTooltip.wszText[0].ch)
        return;
    if (!QOL_IsModeActive(gnQolItemCompareMode, gnQolItemCompareKey))
        return;

    D2UnitStrc *pItem = gpHoverItem;
    if (!pItem || pItem->dwUnitType != UNIT_ITEM || pItem->dwItemMode == IMODE_EQUIP)
        return;

    D2UnitStrc *pOwner = ITEMCOMPARE_GetCompareOwner();
    if (!pOwner)
        return;

    D2UnitStrc *apEquipped[ITEMCOMPARE_MAX_ITEMS] = {};
    const int32_t nEquipped = ITEMCOMPARE_FindEquipped(pOwner, pItem, apEquipped);

    int32_t nCount = 0;
    for (int32_t i = 0; i < nEquipped; ++i)
    {
        if (apEquipped[i] != pItem && ITEMCOMPARE_BuildTooltip(pOwner, apEquipped[i], &gaPendingTooltips[nCount]))
            ++nCount;
    }

    gnPendingTooltips = (nCount && ITEMCOMPARE_Layout(nCount)) ? nCount : 0;
}

void __cdecl ITEMCOMPARE_DrawPending()
{
    const int32_t nCount = gnPendingTooltips;
    gnPendingTooltips = 0;
    if (!nCount)
        return;

    for (int32_t i = 0; i < nCount; ++i)
    {
        const D2ItemCompareTooltipStrc *pTooltip = &gaPendingTooltips[i];
        D2WIN_10129_DrawFramedText(pTooltip->wszText, pTooltip->nX, pTooltip->nY, pTooltip->nColor, 0);
        D2WIN_10124();
    }

    //  back in the slot for the frame's own D2WIN_10124: the hovered tooltip is drawn last, over the others
    D2WIN_10129_DrawFramedText(gtHoveredTooltip.wszText, gtHoveredTooltip.nX, gtHoveredTooltip.nY, gtHoveredTooltip.nColor, gtHoveredTooltip.nAlign);
}

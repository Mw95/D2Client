#define _D2VARS_H

#include "DLLmain.h"
#include "D2Client.h"
#include "npcmenu.h"
#include "ItemAppearance.h"

//  ItemAppearance.cpp -- item appearance ("transmog"), the client's side. Not part of the original D2Client.dll; see ItemAppearance.h.
//  The checks mirror D2MOO's D2ItemAppearance.h one for one, so a request the anvil panel lets through is one the server accepts.


//  The donor row stored on the item, or -1. The stat sits in the item's base stat list (no state, flag 0x40), the first list
//  ITEMS_SerializeItemComplete writes and the one the client's decoder fills.
int32_t __fastcall ITEMAPPEARANCE_GetStoredClassId(D2UnitStrc *pItem)
{
    D2StatListStrc *pStatList;
    int32_t nValue;

    if (!pItem || pItem->dwUnitType != UNIT_ITEM)
        return -1;

    pStatList = D2COMMON_STATLIST_GetStatListFromUnitStateOrFlag(pItem, STATE_NONE, 0x40);
    if (!pStatList)
        return -1;

    nValue = D2COMMON_STATLIST_GetStatValue(pStatList, (D2C_ItemStats)STAT_ITEM_APPEARANCE, 0);
    return nValue > 0 ? nValue - 1 : -1;
}


//  Is the row drawn on the character as a composit layer? Circlets have a head component and no head graphic.
BOOL __fastcall ITEMAPPEARANCE_RecordHasGraphic(const D2ItemsTxt *pRecord)
{
    if (!pRecord || pRecord->nComponent >= NUM_COMPONENTS)
        return FALSE;

    return !ITEMS_CheckType(pRecord->wType[0], ITEMTYPE_CIRCLET);
}


//  May the row pDonor be drawn in place of the row pItem? The same composit layer, and for weapons the same weapon class one- and
//  two-handed: the class picks the COF and AnimData record, and the donor's DCCs only exist for its own classes.
D2ItemAppearanceResult __fastcall ITEMAPPEARANCE_CheckRecords(const D2ItemsTxt *pItem, const D2ItemsTxt *pDonor)
{
    BOOL bItemIsWeapon;
    BOOL bDonorIsWeapon;

    if (!pItem || !pDonor)
        return ITEMAPPEARANCE_ERR_INVALID;

    if (!ITEMAPPEARANCE_RecordHasGraphic(pItem) || !ITEMAPPEARANCE_RecordHasGraphic(pDonor))
        return ITEMAPPEARANCE_ERR_NO_GRAPHIC;

    if (pItem->nComponent != pDonor->nComponent)
        return ITEMAPPEARANCE_ERR_SLOT_MISMATCH;

    bItemIsWeapon = ITEMS_CheckType(pItem->wType[0], ITEMTYPE_WEAPON) != 0;
    bDonorIsWeapon = ITEMS_CheckType(pDonor->wType[0], ITEMTYPE_WEAPON) != 0;
    if (bItemIsWeapon != bDonorIsWeapon)
        return ITEMAPPEARANCE_ERR_SLOT_MISMATCH;

    if (bItemIsWeapon && (pItem->dwWeapClass != pDonor->dwWeapClass || pItem->dwWeapClass2Hand != pDonor->dwWeapClass2Hand))
        return ITEMAPPEARANCE_ERR_WEAPON_CLASS;

    return ITEMAPPEARANCE_OK;
}


//  The row the item is drawn with: the stored donor while it still fits the item (the txt files may have changed), else its own.
int32_t __fastcall ITEMAPPEARANCE_GetDisplayClassId(D2UnitStrc *pItem)
{
    int32_t nDonorClassId;

    if (!pItem || pItem->dwUnitType != UNIT_ITEM)
        return -1;

    nDonorClassId = ITEMAPPEARANCE_GetStoredClassId(pItem);
    if (nDonorClassId >= 0 && nDonorClassId != (int32_t)pItem->dwClassId
        && ITEMAPPEARANCE_CheckRecords(D2COMMON_DATATBLS_GetItemsTxtRecord((int)pItem->dwClassId), D2COMMON_DATATBLS_GetItemsTxtRecord(nDonorClassId)) == ITEMAPPEARANCE_OK)
    {
        return nDonorClassId;
    }

    return (int32_t)pItem->dwClassId;
}


D2ItemsTxt *__fastcall ITEMAPPEARANCE_GetDisplayRecord(D2UnitStrc *pItem)
{
    int32_t nClassId;

    nClassId = ITEMAPPEARANCE_GetDisplayClassId(pItem);
    return nClassId >= 0 ? D2COMMON_DATATBLS_GetItemsTxtRecord(nClassId) : NULL;
}


//  D2Common's ITEMS_GetAltGfx (#10747) for the displayed row: alternategfx when set, else the code.
uint32_t __fastcall ITEMAPPEARANCE_GetDisplayAltGfx(D2UnitStrc *pItem)
{
    D2ItemsTxt *pRecord;

    pRecord = ITEMAPPEARANCE_GetDisplayRecord(pItem);
    if (!pRecord)
        return 0;

    return pRecord->dwAlternateGfx ? pRecord->dwAlternateGfx : pRecord->dwCode;
}


//  May pPlayer give pItem the look of pDonor? pDonor == pItem asks for the item's own look back.
D2ItemAppearanceResult __fastcall ITEMAPPEARANCE_CanApply(D2UnitStrc *pPlayer, D2UnitStrc *pItem, D2UnitStrc *pDonor)
{
    D2ItemsTxt *pItemRecord;
    D2ItemsTxt *pDonorRecord;
    D2ItemAppearanceResult nShape;
    int32_t nDonorClass;

    if (!pItem || pItem->dwUnitType != UNIT_ITEM || !pDonor || pDonor->dwUnitType != UNIT_ITEM)
        return ITEMAPPEARANCE_ERR_INVALID;

    pItemRecord = D2COMMON_DATATBLS_GetItemsTxtRecord((int)pItem->dwClassId);
    pDonorRecord = D2COMMON_DATATBLS_GetItemsTxtRecord((int)pDonor->dwClassId);
    if (!pItemRecord || !pDonorRecord)
        return ITEMAPPEARANCE_ERR_INVALID;

    if (!D2COMMON_ITEMS_CheckItemFlag(pItem, IFLAG_IDENTIFIED, __LINE__, __FILE__))
        return ITEMAPPEARANCE_ERR_UNIDENTIFIED;

    if (pItem == pDonor)
    {
        if (!ITEMAPPEARANCE_RecordHasGraphic(pItemRecord))
            return ITEMAPPEARANCE_ERR_NO_GRAPHIC;
        return ITEMAPPEARANCE_GetStoredClassId(pItem) >= 0 ? ITEMAPPEARANCE_OK : ITEMAPPEARANCE_ERR_SAME_LOOK;
    }

    nShape = ITEMAPPEARANCE_CheckRecords(pItemRecord, pDonorRecord);
    if (nShape != ITEMAPPEARANCE_OK)
        return nShape;

    nDonorClass = D2COMMON_ITEMS_GetClassOfClassSpecificItem(pDonor);
    if (nDonorClass != PCLASS_INVALID && (!pPlayer || pPlayer->dwUnitType != UNIT_PLAYER || nDonorClass != (int32_t)pPlayer->dwClassId))
        return ITEMAPPEARANCE_ERR_CLASS_RESTRICTED;

    if (ITEMAPPEARANCE_GetDisplayClassId(pItem) == (int32_t)pDonor->dwClassId)
        return ITEMAPPEARANCE_ERR_SAME_LOOK;

    return ITEMAPPEARANCE_OK;
}


int32_t __fastcall ITEMAPPEARANCE_GetCost(D2UnitStrc *pItem, BOOL bRestore)
{
    int32_t nLevel;
    int32_t nCost;

    nLevel = pItem ? ITEMS_GetItemLevel(pItem) : 1;
    if (nLevel < 1)
        nLevel = 1;

    nCost = nLevel * ITEMAPPEARANCE_GOLD_PER_ILVL;
    return bRestore ? nCost * ITEMAPPEARANCE_RESTORE_COST_PERCENT / 100 : nCost;
}


//  The vendors that offer the service: one blacksmith per act.
BOOL __fastcall ITEMAPPEARANCE_IsBlacksmith(int32_t nNpcClassId)
{
    switch (nNpcClassId)
    {
    case MONSTER_CHARSI:
    case MONSTER_FARA:
    case MONSTER_HRATLI:
    case MONSTER_HALBU:
    case MONSTER_LARZUK:
        return TRUE;
    default:
        return FALSE;
    }
}


//  Packet 0x38 as the anvil's other services send it (NPCMENU_SendQuestItemAction), with the donor in the action: 13 bytes, the action, the
//  NPC's GUID, the item's GUID.
void __fastcall ITEMAPPEARANCE_SendRequest(D2UnitGUID nNpcGUID, D2UnitGUID nItemGUID, D2UnitGUID nDonorGUID)
{
    const uint32_t nAction = ITEMAPPEARANCE_ACTION_FLAG | ((uint32_t)nDonorGUID & ITEMAPPEARANCE_ACTION_GUID_MASK);

    sgUIState.bWaitingForItemMsg = 0;
    MSG_SendCmdUnitParam(0x38, (int32_t)nAction, (int32_t)nNpcGUID, (int32_t)nItemGUID);
}


//  The tooltip line "Appearance: <base item name>" in tan, for an item drawn with another row's look; nothing otherwise. Appended to wszOut,
//  which holds nCapacity Unicodes; the line is dropped rather than cut when it does not fit. The callers put it last in a block they colour
//  themselves, so the colour code here does not leak into lines after it.
void __fastcall ITEMAPPEARANCE_AppendTooltipText(D2UnitStrc *pItem, Unicode *wszOut, int32_t nCapacity)
{
    D2ItemsTxt *pRecord;
    const Unicode *pwszName;
    char szDigit[4];
    Unicode wszLine[256];
    Unicode wszScratch[32];

    if (!pItem || ITEMAPPEARANCE_GetDisplayClassId(pItem) == (int32_t)pItem->dwClassId)
        return;

    pRecord = ITEMAPPEARANCE_GetDisplayRecord(pItem);
    if (!pRecord)
        return;

    pwszName = D2LANG_GetStringFromTblIndex((D2C_StringIndices)pRecord->wNameStr);
    if (!pwszName || Unicode::strlen(pwszName) > 200)
        return;

    Unicode::strcpy(wszLine, D2LANG_GetStringFromTblIndex(STR_IDX_3994_colorcode));
    wsprintfA(szDigit, "%c", (char)('0' + 7));   //  tan
    Unicode::win2Unicode(wszScratch, szDigit, 2);
    Unicode::strcat(wszLine, wszScratch);
    Unicode::win2Unicode(wszScratch, "Appearance: ", 31);
    Unicode::strcat(wszLine, wszScratch);
    Unicode::strcat(wszLine, pwszName);
    Unicode::strcat(wszLine, D2LANG_GetStringFromTblIndex(STR_IDX_3998_newline));

    if (Unicode::strlen(wszOut) + Unicode::strlen(wszLine) < nCapacity)
        Unicode::strcat(wszOut, wszLine);
}

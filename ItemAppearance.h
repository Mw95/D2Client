//  ItemAppearance.h -- item appearance ("transmog"): an item drawn on the character with the look of another base item, every other property
//  its own. Not part of the original D2Client.dll: this is a feature of the D2MOO-based game. The rules, the stat and the protocol are D2MOO's
//  (source/D2Common/include/D2ItemAppearance.h and doc/ItemAppearance.md); this unit is the client's copy of them, written against this
//  rewrite's headers. The server decides; the client only checks ahead so the anvil panel can refuse what the server would.
//  Include it after D2Client.h.
#pragma once

//  ItemStatCost.txt row of the stat (appended after passive_mag_pierce, 358). Its value is the donor's items.txt row + 1.
constexpr int32_t STAT_ITEM_APPEARANCE = 359;
//  Gold: item level * 100, half of that to restore the item's own look.
constexpr int32_t ITEMAPPEARANCE_GOLD_PER_ILVL = 100;
constexpr int32_t ITEMAPPEARANCE_RESTORE_COST_PERCENT = 50;
//  Packet 0x38's nAction for the request: this flag | the donor's GUID (the item's own GUID restores its look).
constexpr uint32_t ITEMAPPEARANCE_ACTION_FLAG = 0x80000000u;
constexpr uint32_t ITEMAPPEARANCE_ACTION_GUID_MASK = 0x7FFFFFFFu;

enum D2ItemAppearanceResult
{
    ITEMAPPEARANCE_OK = 0,
    ITEMAPPEARANCE_ERR_INVALID,
    ITEMAPPEARANCE_ERR_UNIDENTIFIED,
    ITEMAPPEARANCE_ERR_NO_GRAPHIC,
    ITEMAPPEARANCE_ERR_SLOT_MISMATCH,
    ITEMAPPEARANCE_ERR_WEAPON_CLASS,
    ITEMAPPEARANCE_ERR_CLASS_RESTRICTED,
    ITEMAPPEARANCE_ERR_SAME_LOOK,
};

//  D2Common exports the original D2Client does not import; D2Common.lib binds them by ordinal like the others.
extern "C" {
BOOL __stdcall ITEMS_CheckType(int nItemType1, int nItemType2);   /* #10730 */
int __stdcall ITEMS_GetItemLevel(D2UnitStrc *pItem);              /* #10717 */
}

int32_t __fastcall ITEMAPPEARANCE_GetStoredClassId(D2UnitStrc *pItem);
int32_t __fastcall ITEMAPPEARANCE_GetDisplayClassId(D2UnitStrc *pItem);
D2ItemsTxt *__fastcall ITEMAPPEARANCE_GetDisplayRecord(D2UnitStrc *pItem);
uint32_t __fastcall ITEMAPPEARANCE_GetDisplayAltGfx(D2UnitStrc *pItem);
BOOL __fastcall ITEMAPPEARANCE_RecordHasGraphic(const D2ItemsTxt *pRecord);
D2ItemAppearanceResult __fastcall ITEMAPPEARANCE_CheckRecords(const D2ItemsTxt *pItem, const D2ItemsTxt *pDonor);
D2ItemAppearanceResult __fastcall ITEMAPPEARANCE_CanApply(D2UnitStrc *pPlayer, D2UnitStrc *pItem, D2UnitStrc *pDonor);
int32_t __fastcall ITEMAPPEARANCE_GetCost(D2UnitStrc *pItem, BOOL bRestore);
BOOL __fastcall ITEMAPPEARANCE_IsBlacksmith(int32_t nNpcClassId);
void __fastcall ITEMAPPEARANCE_SendRequest(D2UnitGUID nNpcGUID, D2UnitGUID nItemGUID, D2UnitGUID nDonorGUID);
void __fastcall ITEMAPPEARANCE_AppendTooltipText(D2UnitStrc *pItem, Unicode *wszOut, int32_t nCapacity);

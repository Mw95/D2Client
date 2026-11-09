//  ItemAppearancePicker.h -- the look picker of the item appearance feature (ItemAppearance.h): a panel over the anvil that shows every look
//  the item on the anvil can take, as a grid of inventory icons, one entry per distinct graphic. Not part of the original D2Client.dll.
//  Include it after D2Client.h and ItemAppearance.h.
#pragma once

//  Rows that share one graphic are one entry; this many of their names are kept for the label.
constexpr int32_t ITEMAPPEARANCE_MAX_NAMES_PER_LOOK = 4;
//  Entries of one list. 1.10f's items.txt has about 650 rows; one slot of one weapon class has far fewer looks.
constexpr int32_t ITEMAPPEARANCE_MAX_LOOKS = 256;

struct D2ItemAppearanceLookStrc
{
    int32_t nClassId;                                       //  the row requested: the first row with this graphic, or the item's own
    int32_t nNameClassIds[ITEMAPPEARANCE_MAX_NAMES_PER_LOOK];   //  rows sharing the graphic, for the label
    int32_t nNameCount;
    int32_t nRowCount;                                      //  every row sharing the graphic, named or not
    BOOL bOriginal;                                         //  "Original look": gives the item its own look back
    D2CellFileStrc *pIcon;                                  //  the row's inventory graphic, loaded when the list is built
    int32_t nIconWidth;
    int32_t nIconHeight;
};

//  The list for the item last built for, and the item.
inline D2ItemAppearanceLookStrc gaItemAppearanceLooks[ITEMAPPEARANCE_MAX_LOOKS];
inline int32_t gnItemAppearanceLookCount;
inline D2UnitGUID gnItemAppearancePickerItemGUID;

int32_t __fastcall ITEMAPPEARANCE_BuildLookList(D2UnitStrc *pPlayer, D2UnitStrc *pItem);
void __cdecl ITEMAPPEARANCE_FreeLookList(void);
void __fastcall ITEMAPPEARANCE_GetLookLabel(const D2ItemAppearanceLookStrc *pLook, Unicode *wszOut, int32_t nCapacity);

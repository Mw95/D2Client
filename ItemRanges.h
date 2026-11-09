//  ItemRanges.h -- the stat ranges of an item's description, a quality-of-life addition (see Qol.h). Include it after D2Client.h.
//
//  The client only holds an item's final stats: a stat list keeps the sum of whatever rolled, not where it came from. The ranges are rebuilt
//  from what the item carries about its origin -- its affixes, its unique, set or runeword record, its socketed items and the cube recipe that
//  crafted it -- and the data tables those name. ItemRanges.cpp says how each source turns into stats.
#pragma once

#include "Qol.h"

//  The stat D2MOO's D2Game writes in the magic list of an item made by a cube recipe that adds properties to its output: (cubemain.txt row
//  + 1) << 2 | output index (0..2). It is not a stat of the original game: itemstatcost.txt needs a row for it at this id (after the last 1.10
//  stat, 358) with Save Bits wide enough for the value (14) and no description, or it is never written to the item's bitstream, and crafted
//  items only get ranges for their affixes. The same number is STAT_D2MOO_ITEM_CRAFTRECIPE in D2MOO.
constexpr int32_t ITEMRANGES_STAT_CRAFTRECIPE = 359;

constexpr int32_t ITEMRANGES_MAX_ENTRIES = 160;

//  One stat's possible values: the sum of the minimums and the sum of the maximums of every source that writes (nStatId, nLayer), in the
//  property's own units (before the stat's valshift).
struct D2ItemRangeEntryStrc
{
    int32_t nStatId;
    int32_t nLayer;
    int32_t nMin;
    int32_t nMax;
};

struct D2ItemRangeTableStrc
{
    int32_t nCount;
    D2ItemRangeEntryStrc aEntries[ITEMRANGES_MAX_ENTRIES];
};

//  TRUE while an item description is being built for pUnit with ranges switched on, or for the player when the description's item is a set
//  item (the full set's bonuses are rendered from the player's lists).
BOOL __fastcall ITEMRANGES_IsActiveFor(D2UnitStrc *pUnit);

//  The item whose description is being built, and the colour the block being built is drawn in: a range is grey, and the line goes on in this
//  colour after it. A scope: the description code holds one for as long as it builds the item's text.
struct ITEMRANGES_DescriptionScope
{
    ITEMRANGES_DescriptionScope(D2UnitStrc *pItem, uint32_t nBlockColor);
    ~ITEMRANGES_DescriptionScope();

    D2UnitStrc *pPreviousItem;
    uint32_t nPreviousColor;
};

void __fastcall ITEMRANGES_SetBlockColor(uint32_t nBlockColor);

//  Ranges off for the description being drawn: its text with ranges would not fit the 1023 characters D2Win draws. The description is built
//  again without them.
void __fastcall ITEMRANGES_SetSuppressed(BOOL bSuppressed);
BOOL __cdecl ITEMRANGES_IsSuppressed();
BOOL __cdecl ITEMRANGES_IsEnabled();

//  The sources of the stat list MODS_BuildStatsText renders: (nState, nFlag) of pUnit, with the runeword list and the socketed items when
//  nState is STATE_NONE. FALSE when there is nothing to give ranges for.
BOOL __fastcall ITEMRANGES_BuildTable(D2UnitStrc *pUnit, D2C_States nState, int32_t nFlag, D2ItemRangeTableStrc *pTable);

//  " [min-max]" for one stat line, in grey, the block colour after it; empty when the stat has no range, its value is not inside the range
//  (a source the table does not know of), or its description has no number to range.
void __fastcall ITEMRANGES_FormatStatSuffix(const D2ItemRangeTableStrc *pTable, D2UnitStrc *pUnit, D2StatListStrc *pStatList, int32_t nStatId, int32_t nLayer,
                                            int32_t nValue, Unicode *wszOut);

//  The same for one of MODS_FormatElementalDamageText's lines, by the stat that printed it.
void __fastcall ITEMRANGES_FormatDamageGroupSuffix(const D2ItemRangeTableStrc *pTable, D2UnitStrc *pUnit, D2StatListStrc *pStatList,
                                                   const D2ClientDamageGroupStrc *pDamageGroups, int32_t nStatId, Unicode *wszOut);

//  Puts wszSuffix into wszLine (or the last line of it) before its trailing newline, or at its end when it has none.
void __fastcall ITEMRANGES_InsertBeforeNewline(Unicode *wszLine, const Unicode *wszSuffix);

//  The item's base values: the line has been built and ends in a newline; the range goes in before it. nShown is the number the line prints,
//  nBlockColor the colour the line is drawn in.
void __fastcall ITEMRANGES_AppendDefenseRange(D2UnitStrc *pItem, int32_t nShown, uint32_t nBlockColor, Unicode *wszLine);
void __fastcall ITEMRANGES_AppendDamageRange(D2UnitStrc *pItem, D2C_ItemStats nMinStatId, D2C_ItemStats nMaxStatId, int32_t nShownMin, int32_t nShownMax,
                                             uint32_t nBlockColor, Unicode *wszLine);
void __fastcall ITEMRANGES_AppendDurabilityRange(D2UnitStrc *pItem, int32_t nShownMax, uint32_t nBlockColor, Unicode *wszLine);
void __fastcall ITEMRANGES_AppendSocketsRange(D2UnitStrc *pItem, int32_t nShown, uint32_t nBlockColor, Unicode *wszLine);

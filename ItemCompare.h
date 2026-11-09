//  ItemCompare.h -- the equipped item's description beside the hovered item's, a quality-of-life addition (see Qol.h). Include it after
//  D2Client.h.
#pragma once

#include "Qol.h"

//  Every draw of an item description's text goes through here rather than straight to D2WIN_10129_DrawFramedText. TRUE: the text was taken
//  for an equipped item's tooltip and must not be drawn; FALSE: the caller draws it, and it is remembered as the hovered item's.
BOOL __fastcall ITEMCOMPARE_OnDescriptionText(const Unicode *wszText, int32_t nX, int32_t nY, int32_t nColor, int32_t nAlign);

//  After INV_DrawItemDescription(pDescUnit, ...): when it drew the hovered item's description, the descriptions of what is equipped where the
//  item would go are built and laid out beside it, for ITEMCOMPARE_DrawPending.
void __fastcall ITEMCOMPARE_AfterDescription(D2UnitStrc *pDescUnit);

//  Just before D2WIN_10124 draws the frame's framed text: the equipped items' tooltips are drawn, each through D2Win's one framed-text slot,
//  and the hovered item's text is put back in the slot for D2WIN_10124.
void __cdecl ITEMCOMPARE_DrawPending();

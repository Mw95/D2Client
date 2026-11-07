//  CustomNpc.h -- mod, not part of the original D2Client.dll: the client side of D2MOO's custom NPCs (see D2MOO's doc/CustomNpcs.md), new
//  MonStats rows that trade, repair and heal like the town NPCs. The server does the work; the client needs the NPC's menu and, in the trade
//  screen, to price and label items the way the server will treat them.
//  The data comes from D2MOO D2Common export DATATBLS_GetCustomNpcRecord, looked up at run time (ModImports.h). Without it no NPC is custom
//  and the client behaves as the original. Include it after D2Client.h.
#pragma once

//  The menu of a custom NPC: Talk when it has gossip lines (NpcGossip1..4: Talk plays the next one, then the menu comes back), Trade (or
//  Trade/Repair with NpcRepair) when it has NpcTrade. NULL for every other class.
D2NpcMenuDescStrc *__fastcall CUSTOMNPC_GetMenuDescriptor(int32_t nClassId);

//  The class the trade screen's class switches should act as for nClassId: the repair NPC for a custom NPC with NpcRepair (its NpcTrade NPC
//  when that one is a smith, else Charsi, as the server repairs), its NpcTrade NPC for one that only trades, nClassId itself otherwise.
//  Repair prices are asked with this class too (the server repairs as it); buy and sell prices keep the NPC's own class, as the server does.
int32_t __fastcall CUSTOMNPC_GetTransactionClassId(int32_t nClassId);

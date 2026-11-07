//  SafeZone.h -- mod, not part of the original D2Client.dll: town-like safe zones inside regular levels (see D2MOO's doc/SafeZones.md).
//  A safe zone is a preset room D2Common's DUNGEON_IsRoomInTown already answers TRUE for, so the town rules need nothing here. This file
//  adds what makes the zone look like a town: its name as an entry title when the player walks in, and on the automap.
//  The data comes from four exports that only the D2MOO D2Common.dll has (by name, ordinals 11308..11311). The loader binds the name
//  D2Common.dll to the original, so they are looked up at run time among the loaded modules of that name. Without them every function
//  here answers "no safe zone" and the client behaves as the original.
//  Include it after D2Client.h.
#pragma once

//  The safe zone's LvlPrest id for the room, -1 when the room is not a safe zone (or the D2MOO exports are missing).
int32_t __fastcall SAFEZONE_GetPresetIdFromRoom(D2ActiveRoomStrc *pRoom);

//  LvlPrest.txt EntryName through the string tables, NULL when the zone has none.
const Unicode *__fastcall SAFEZONE_GetEntryName(int32_t nLvlPrestId);

//  The name the automap shows for the player's room: the safe zone's EntryName when the room is a zone that has one, NULL otherwise.
const Unicode *__fastcall SAFEZONE_GetAutomapName(D2ActiveRoomStrc *pRoom);

//  Called once a frame by UIPANELS_DrawInGamePanels, after the level's own entry title. Shows the zone's title for 120 frames when the
//  player steps from outside a safe zone into one within the same level: LvlPrest.txt EntryFile as an image laid out like the level titles,
//  else EntryName in the large font. bLevelTitleShowing holds it back while the level's own title is on screen.
void __fastcall SAFEZONE_UpdateEntryTitle(D2ActiveRoomStrc *pPlayerRoom, int32_t bLevelTitleShowing);

//  Frees the title image; called where the UI frees the level's entry title.
void __fastcall SAFEZONE_FreeEntryTitle();

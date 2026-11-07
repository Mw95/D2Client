//  ModImports.h -- mod, not part of the original D2Client.dll: the exports only the D2MOO D2Common.dll has (safe zones, custom NPCs).
//  The loader binds the import name D2Common.dll to the original DLL, which lacks them, so they are looked up at run time among all loaded
//  modules of that name. Include it after D2Client.h.
#pragma once

//  The first export of that name found in a loaded module named D2Common.dll, NULL when there is none (the original game).
FARPROC __fastcall MODIMPORTS_FindD2CommonExport(const char *szExportName);

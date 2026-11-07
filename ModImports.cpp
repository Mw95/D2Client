#define _D2VARS_H

#include "DLLmain.h"
#include "D2Client.h"
#include "ModImports.h"

#include <tlhelp32.h>

//  ModImports.cpp -- mod, not part of the original D2Client.dll. See ModImports.h.

FARPROC __fastcall MODIMPORTS_FindD2CommonExport(const char *szExportName)
{
    FARPROC pResult = NULL;
    HANDLE hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE, GetCurrentProcessId());
    if (hSnapshot == INVALID_HANDLE_VALUE)
        return NULL;

    MODULEENTRY32W tModule = {};
    tModule.dwSize = sizeof(tModule);
    for (BOOL bFound = Module32FirstW(hSnapshot, &tModule); bFound && !pResult; bFound = Module32NextW(hSnapshot, &tModule))
    {
        if (!_wcsicmp(tModule.szModule, L"D2Common.dll"))
            pResult = GetProcAddress(tModule.hModule, szExportName);
    }

    CloseHandle(hSnapshot);
    return pResult;
}

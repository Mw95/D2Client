#define _D2VARS_H

#include "DLLmain.h"
#include "D2Client.h"
#include "CustomNpc.h"
#include "ModImports.h"

#include <map>

//  CustomNpc.cpp -- mod, not part of the original D2Client.dll. See CustomNpc.h.

//  D2MOO's D2CustomNpcTxt (source/D2Common/include/DataTbls/CustomNpcTbls.h).
#pragma pack(push, 1)
struct D2CustomNpcTxt
{
    int32_t nTradeAs;
    uint32_t dwRepair;
    uint32_t dwHeal;
};
#pragma pack(pop)
static_assert(sizeof(D2CustomNpcTxt) == 12, "D2MOO's D2CustomNpcTxt");

using GetCustomNpcRecordFn = const D2CustomNpcTxt *(__stdcall *)(int nMonStatsId);

static const D2CustomNpcTxt *CUSTOMNPC_GetRecord(int32_t nClassId)
{
    static bool bResolved = false;
    static GetCustomNpcRecordFn pfnGetCustomNpcRecord = NULL;
    if (!bResolved)
    {
        bResolved = true;
        pfnGetCustomNpcRecord = (GetCustomNpcRecordFn)MODIMPORTS_FindD2CommonExport("DATATBLS_GetCustomNpcRecord");
    }

    return pfnGetCustomNpcRecord && nClassId >= 0 ? pfnGetCustomNpcRecord(nClassId) : NULL;
}

//  The smiths, as the server's repair accepts them.
static bool CUSTOMNPC_IsSmith(int32_t nClassId)
{
    switch (nClassId)
    {
    case MONSTER_CHARSI:
    case MONSTER_FARA:
    case MONSTER_HRATLI:
    case MONSTER_HALBU:
    case MONSTER_LARZUK:
        return true;
    default:
        return false;
    }
}

D2NpcMenuDescStrc *__fastcall CUSTOMNPC_GetMenuDescriptor(int32_t nClassId)
{
    const D2CustomNpcTxt *pRecord = CUSTOMNPC_GetRecord(nClassId);
    if (!pRecord)
        return NULL;

    //  One record per class, built on first use. The menu code writes nRows of some records; these are left alone by it since no unlock
    //  or class switch names them.
    static std::map<int32_t, D2NpcMenuDescStrc> sgDescriptors;
    auto it = sgDescriptors.find(nClassId);
    if (it == sgDescriptors.end())
    {
        D2NpcMenuDescStrc tDescriptor = {};
        tDescriptor.nMonsterId = nClassId;
        tDescriptor.bEnabled = 1;

        int32_t nButtons = 0;
        if (pRecord->nTradeAs > 0)
        {
            tDescriptor.wTblIdx[nButtons] = pRecord->dwRepair ? STR_IDX_3334_NPCMenuTradeRepair : STR_IDX_3396_NPCMenuTrade;
            tDescriptor.pfnHandler[nButtons] = &NPCMENU_OnTradeClicked;
            ++nButtons;
        }
        tDescriptor.nRows = nButtons + 1;

        it = sgDescriptors.emplace(nClassId, tDescriptor).first;
    }

    return &it->second;
}

int32_t __fastcall CUSTOMNPC_GetTransactionClassId(int32_t nClassId)
{
    const D2CustomNpcTxt *pRecord = CUSTOMNPC_GetRecord(nClassId);
    if (!pRecord)
        return nClassId;

    if (pRecord->dwRepair)
        return CUSTOMNPC_IsSmith(pRecord->nTradeAs) ? pRecord->nTradeAs : MONSTER_CHARSI;

    if (pRecord->nTradeAs > 0)
        return pRecord->nTradeAs;

    return nClassId;
}

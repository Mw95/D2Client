#define _D2VARS_H

#include "DLLmain.h"
#include "D2Client.h"
#include "CustomNpc.h"
#include "ModImports.h"
#include "npcmenu.h"

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
using GetCustomNpcGossipFn = int(__stdcall *)(int nMonStatsId, int nIndex);

static constexpr int32_t CUSTOMNPC_GOSSIP_LINES = 4;

//  The .tbl index of gossip line nIndex of the NPC (D2MOO's NpcGossip1..4), -1 when there is none.
static int32_t CUSTOMNPC_GetGossip(int32_t nClassId, int32_t nIndex)
{
    static bool bResolved = false;
    static GetCustomNpcGossipFn pfnGetGossip = NULL;
    if (!bResolved)
    {
        bResolved = true;
        pfnGetGossip = (GetCustomNpcGossipFn)MODIMPORTS_FindD2CommonExport("DATATBLS_GetCustomNpcGossip");
    }

    return pfnGetGossip && nClassId >= 0 ? pfnGetGossip(nClassId, nIndex) : -1;
}

static bool CUSTOMNPC_HasGossip(int32_t nClassId)
{
    for (int32_t i = 0; i < CUSTOMNPC_GOSSIP_LINES; ++i)
    {
        if (CUSTOMNPC_GetGossip(nClassId, i) > 0)
            return true;
    }
    return false;
}

//  The end of a gossip line: back to the NPC's menu, as the game's speech chains end (NPCMENU_ChooseNextSpeech), or out of the
//  conversation when the player clicked away.
static int32_t __fastcall CUSTOMNPC_OnGossipDismissed(void *pArg, int32_t bDismissed, int32_t bDismissedByMouse)
{
    D2UnitStrc *pNpc;

    sgUIState.bNpcSpeechActive = 0;
    TEXT_SetCallback(NULL, (int32_t)(intptr_t)pArg);

    pNpc = NPCMENU_NPC_GetActiveNpcUnit();
    if ((bDismissed && bDismissedByMouse) || !pNpc)
    {
        NPCMENU_NpcCloseInteraction();
        return 0;
    }

    sgUIState.CurrentState = 1;
    NPCMENU_OpenMainMenu(pNpc, 1);
    return 1;
}

//  The Talk row of a custom NPC: the NPC says its next gossip line (they take turns, per NPC class, for the session).
static int32_t __cdecl CUSTOMNPC_OnTalkClicked(void)
{
    static std::map<int32_t, int32_t> sgNextLine;
    D2UnitStrc *pNpc;
    int32_t nTblIndex;

    pNpc = NPCMENU_NPC_GetActiveNpcUnit();
    if (!pNpc)
    {
        NPCMENU_NpcCloseInteraction();
        return 0;
    }

    nTblIndex = -1;
    int32_t &nNext = sgNextLine[pNpc->dwClassId];
    for (int32_t i = 0; i < CUSTOMNPC_GOSSIP_LINES && nTblIndex <= 0; ++i)
    {
        nTblIndex = CUSTOMNPC_GetGossip(pNpc->dwClassId, nNext % CUSTOMNPC_GOSSIP_LINES);
        nNext = (nNext + 1) % CUSTOMNPC_GOSSIP_LINES;
    }
    if (nTblIndex <= 0)
        return 0;

    if (sgUIState.hDialogMainMenu)
    {
        DIALOG_Close(sgUIState.hDialogMainMenu);
        sgUIState.hDialogMainMenu = NULL;
    }
    sgUIState.CurrentState = 1;
    sgUIState.wNpcShownSpeechTblId = (uint16_t)nTblIndex;
    TEXT_ShowSpeechPopup(pNpc, (int16_t)nTblIndex, 0);
    sgUIState.bNpcSpeechActive = 1;
    TEXT_SetCallback((void *)CUSTOMNPC_OnGossipDismissed, 0);
    return 1;
}

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
        if (CUSTOMNPC_HasGossip(nClassId))
        {
            tDescriptor.wTblIdx[nButtons] = STR_IDX_3381_TalkMenu;
            tDescriptor.pfnHandler[nButtons] = &CUSTOMNPC_OnTalkClicked;
            ++nButtons;
        }
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

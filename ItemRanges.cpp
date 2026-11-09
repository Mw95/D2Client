//  ItemRanges.cpp -- the stat ranges of an item's description. Not a unit of the original D2Client.dll: a quality-of-life addition, switched
//  by gnQolStatRangesMode (Qol.h). With the mode QOLMODE_OFF nothing here changes a description.
//
//  Where the numbers come from. An item's stat lists hold what rolled, summed; the client rebuilds what could have rolled from the item's
//  sources and the data tables:
//
//      magic             magicprefix/magicsuffix.txt, the affix ids in wMagicPrefix[0] / wMagicSuffix[0]
//      rare, crafted     the same tables, up to three prefixes and three suffixes
//      automagic         magic, rare and crafted items: the automagic affix, wAutoAffix (automagic.txt rows share the affix table)
//      unique            uniqueitems.txt, the record in dwFileIndex
//      set               setitems.txt, its own properties, and its partial bonuses: in the item's magic list when the record's add func
//                        is 0, in the item's STATE_ITEMSET1..5 lists otherwise; the full set's bonuses, sets.txt, are in the player's lists
//      runeword          runes.txt, the record whose name string is the item's wMagicPrefix[0] (the item's runeword list, STATE_RUNEWORD)
//      socketed items    a gem's or a rune's gems.txt properties for the parent's apply type; a jewel's own sources
//      crafted (D2MOO)   cubemain.txt, the recipe and output named by the stat ITEMRANGES_STAT_CRAFTRECIPE
//
//  A property becomes stats as D2Common's property functions make them (properties.txt func1..7, the ITEMMODS_PropertyFuncNN of D2MOO's
//  ItemMods.cpp), here without the roll: each gives its stat (and layer) the property's min and max. Functions whose value is not the rolled
//  number (chance-to-cast and charges encode the level in the layer, random skills roll the layer, by-time stats pack a period) give nothing,
//  and their lines print no range.
//
//  The check that keeps a range honest: a stat's value must lie inside the range the table built for it. A source the table does not know of
//  -- a superior item's quality bonus, a recipe without the D2MOO stat, an affix changed in the data after the item rolled -- puts the value
//  outside, and the line prints no range rather than a wrong one.

#include "DLLmain.h"
#include "D2Client.h"
#include "Game.h"
#include "ItemRanges.h"


//  The description being built: its item, the colour of the block being built, and whether ranges are off for this build (it did not fit).
static D2UnitStrc *gpItemRangesItem = NULL;
static uint32_t gnItemRangesBlockColor = 0;
static BOOL gbItemRangesSuppressed = FALSE;

constexpr uint32_t ITEMRANGES_COLOR = 5;       //  dark grey


ITEMRANGES_DescriptionScope::ITEMRANGES_DescriptionScope(D2UnitStrc *pItem, uint32_t nBlockColor)
{
    pPreviousItem = gpItemRangesItem;
    nPreviousColor = gnItemRangesBlockColor;
    gpItemRangesItem = pItem;
    gnItemRangesBlockColor = nBlockColor;
}

ITEMRANGES_DescriptionScope::~ITEMRANGES_DescriptionScope()
{
    gpItemRangesItem = pPreviousItem;
    gnItemRangesBlockColor = nPreviousColor;
}

void __fastcall ITEMRANGES_SetBlockColor(uint32_t nBlockColor)
{
    gnItemRangesBlockColor = nBlockColor;
}

void __fastcall ITEMRANGES_SetSuppressed(BOOL bSuppressed)
{
    gbItemRangesSuppressed = bSuppressed;
}

BOOL __cdecl ITEMRANGES_IsSuppressed()
{
    return gbItemRangesSuppressed;
}

BOOL __cdecl ITEMRANGES_IsEnabled()
{
    return !gbItemRangesSuppressed && QOL_IsModeActive(gnQolStatRangesMode, gnQolStatRangesKey);
}

BOOL __fastcall ITEMRANGES_IsActiveFor(D2UnitStrc *pUnit)
{
    if (!pUnit || !gpItemRangesItem || !ITEMRANGES_IsEnabled())
        return FALSE;
    if (pUnit == gpItemRangesItem)
        return TRUE;
    //  the full set's bonuses: MODS_AppendStateModsText renders the player's lists for the set item being described
    return pUnit->dwUnitType == UNIT_PLAYER && D2COMMON_ITEMS_GetItemQuality(gpItemRangesItem) == ITEMQUAL_SET;
}


//  ---- the table ----

static void __fastcall ITEMRANGES_AddRange(D2ItemRangeTableStrc *pTable, int32_t nStatId, int32_t nLayer, int32_t nMin, int32_t nMax)
{
    if (nStatId < 0 || nStatId >= sgptDataTables->nItemStatCostTxtRecordCount)
        return;

    for (int32_t i = 0; i < pTable->nCount; ++i)
    {
        D2ItemRangeEntryStrc *pEntry = &pTable->aEntries[i];
        if (pEntry->nStatId == nStatId && pEntry->nLayer == nLayer)
        {
            pEntry->nMin += nMin;
            pEntry->nMax += nMax;
            return;
        }
    }

    if (pTable->nCount >= ITEMRANGES_MAX_ENTRIES)
        return;

    D2ItemRangeEntryStrc *pEntry = &pTable->aEntries[pTable->nCount++];
    pEntry->nStatId = nStatId;
    pEntry->nLayer = nLayer;
    pEntry->nMin = nMin;
    pEntry->nMax = nMax;
}

static const D2ItemRangeEntryStrc *__fastcall ITEMRANGES_FindRange(const D2ItemRangeTableStrc *pTable, int32_t nStatId, int32_t nLayer)
{
    if (!pTable)
        return NULL;
    for (int32_t i = 0; i < pTable->nCount; ++i)
    {
        if (pTable->aEntries[i].nStatId == nStatId && pTable->aEntries[i].nLayer == nLayer)
            return &pTable->aEntries[i];
    }
    return NULL;
}

//  One property, as D2Common's property functions assign it (D2MOO ItemMods.cpp, D2COMMON_11292_ItemAssignProperty and the functions of its
//  table), without the roll. The functions run in the record's order until one is 0 or past the table (25..35 are empty in 1.10).
static void __fastcall ITEMRANGES_AddProperty(D2ItemRangeTableStrc *pTable, const D2PropertyStrc *pProperty)
{
    if (!pProperty || pProperty->nProperty < 0 || pProperty->nProperty >= sgptDataTables->nPropertiesTxtRecordCount)
        return;

    const D2PropertiesTxt *pPropertiesTxt = &sgptDataTables->pPropertiesTxt[pProperty->nProperty];

    int32_t nMin = pProperty->nMin;
    int32_t nMax = pProperty->nMax;
    if (nMax < nMin)
        std::swap(nMin, nMax);

    for (int32_t i = 0; i < 7; ++i)
    {
        const int32_t nFunc = pPropertiesTxt->nFunc[i];
        if (nFunc <= 0 || nFunc > 24)
            break;

        const int32_t nStatId = pPropertiesTxt->wStat[i];
        switch (nFunc)
        {
        case 1:     //  the rolled value
        case 2:     //  the same, a percentage
        case 3:     //  the first function's value, or its own roll
        case 4:
        case 8:
        case 13:    //  max durability percent
            ITEMRANGES_AddRange(pTable, nStatId, 0, nMin, nMax);
            break;

        case 5:     //  minimum damage: the one-handed, two-handed and thrown stats
            ITEMRANGES_AddRange(pTable, STAT_MINDAMAGE, 0, nMin, nMax);
            ITEMRANGES_AddRange(pTable, STAT_SECONDARY_MINDAMAGE, 0, nMin, nMax);
            ITEMRANGES_AddRange(pTable, STAT_ITEM_THROW_MINDAMAGE, 0, nMin, nMax);
            break;

        case 6:     //  maximum damage, likewise
            ITEMRANGES_AddRange(pTable, STAT_MAXDAMAGE, 0, nMin, nMax);
            ITEMRANGES_AddRange(pTable, STAT_SECONDARY_MAXDAMAGE, 0, nMin, nMax);
            ITEMRANGES_AddRange(pTable, STAT_ITEM_THROW_MAXDAMAGE, 0, nMin, nMax);
            break;

        case 7:     //  enhanced damage: one roll written to both percentages
            ITEMRANGES_AddRange(pTable, STAT_ITEM_MINDAMAGE_PERCENT, 0, nMin, nMax);
            ITEMRANGES_AddRange(pTable, STAT_ITEM_MAXDAMAGE_PERCENT, 0, nMin, nMax);
            break;

        case 9:     //  a skill's stat, the skill as the layer
        case 24:
            ITEMRANGES_AddRange(pTable, nStatId, pProperty->nLayer, nMin, nMax);
            break;

        case 10:    //  a skill tab: the parameter is class * 3 + tab, the layer class << 3 | tab
            ITEMRANGES_AddRange(pTable, nStatId, pProperty->nLayer % 3 + 8 * pProperty->nLayer / 3, nMin, nMax);
            break;

        case 14:    //  sockets; a roll of 0 or less takes the parameter
            ITEMRANGES_AddRange(pTable, STAT_ITEM_NUMSOCKETS, 0, nMin > 0 ? nMin : pProperty->nLayer, nMax > 0 ? nMax : pProperty->nLayer);
            break;

        case 15:    //  the property's minimum, unrolled
            ITEMRANGES_AddRange(pTable, nStatId, 0, pProperty->nMin, pProperty->nMin);
            break;

        case 16:    //  its maximum, unrolled
            ITEMRANGES_AddRange(pTable, nStatId, 0, pProperty->nMax, pProperty->nMax);
            break;

        case 17:    //  per level: the parameter is the value
            ITEMRANGES_AddRange(pTable, nStatId, 0, pProperty->nLayer, pProperty->nLayer);
            break;

        case 21:    //  the properties.txt val column as the layer (class skills)
            ITEMRANGES_AddRange(pTable, nStatId, pPropertiesTxt->wVal[i], nMin, nMax);
            break;

        case 22:    //  the parameter as the layer (single skills)
            ITEMRANGES_AddRange(pTable, nStatId, (uint16_t)pProperty->nLayer, nMin, nMax);
            break;

        default:
            //  11 skill on event, 12 random skill, 18 by time, 19 charges, 20 indestructible, 23 ethereal: no value to range
            break;
        }
    }
}

static void __fastcall ITEMRANGES_AddProperties(D2ItemRangeTableStrc *pTable, const D2PropertyStrc *pProperties, int32_t nCount)
{
    for (int32_t i = 0; i < nCount; ++i)
    {
        if (pProperties[i].nProperty < 0)
            continue;
        ITEMRANGES_AddProperty(pTable, &pProperties[i]);
    }
}

static void __fastcall ITEMRANGES_AddAffix(D2ItemRangeTableStrc *pTable, int32_t nAffixId)
{
    if (nAffixId <= 0)
        return;

    const D2MagicAffixTxt *pAffix = D2COMMON_DATATBLS_GetMagicAffixTxtRecord(nAffixId);
    if (pAffix)
        ITEMRANGES_AddProperties(pTable, pAffix->pProperties, 3);
}

static const D2SetItemsTxt *__fastcall ITEMRANGES_GetSetItemsRecord(D2UnitStrc *pItem)
{
    const int32_t nFileIndex = D2COMMON_ITEMS_GetFileIndex(pItem);
    if (nFileIndex < 0 || nFileIndex >= sgptDataTables->nSetItemsTxtRecordCount)
        return NULL;
    return &sgptDataTables->pSetItemsTxt[nFileIndex];
}

//  The runeword's record: the one whose name string the item carries in its first prefix slot (ITEMS_ReadItemFromBitstream stores it there).
static const D2RunesTxt *__fastcall ITEMRANGES_GetRunesRecord(D2UnitStrc *pItem)
{
    const int32_t nStringId = D2COMMON_ITEMS_GetPrefixId(pItem, 0);
    if (nStringId <= 0)
        return NULL;
    for (int32_t i = 0; i < sgptDataTables->pRuneDataTables.nRunesTxtRecordCount; ++i)
    {
        if (sgptDataTables->pRuneDataTables.pRunesTxt[i].wStringId == nStringId)
            return &sgptDataTables->pRuneDataTables.pRunesTxt[i];
    }
    return NULL;
}

//  The recipe's properties, by the D2MOO stat: (row + 1) << 2 | output.
static void __fastcall ITEMRANGES_AddCraftRecipe(D2ItemRangeTableStrc *pTable, D2UnitStrc *pItem)
{
    if (ITEMRANGES_STAT_CRAFTRECIPE >= sgptDataTables->nItemStatCostTxtRecordCount)
        return;

    //  D2Game writes it in the item's magic list: the list an item's bitstream carries
    D2StatListStrc *pMagicList = D2COMMON_STATLIST_GetStatListFromUnitStateAndFlag(pItem, STATE_NONE, STATLIST_MAGIC);
    if (!pMagicList)
        return;

    const int32_t nValue = D2COMMON_STATLIST_GetStatValue(pMagicList, (D2C_ItemStats)ITEMRANGES_STAT_CRAFTRECIPE, 0);
    const int32_t nRecipe = (nValue >> 2) - 1;
    const int32_t nOutput = nValue & 3;
    if (nValue <= 0 || nRecipe < 0 || nRecipe >= sgptDataTables->nCubeMainTxtRecordCount || nOutput >= 3)
        return;

    const D2CubeOutputItem *pOutput = &sgptDataTables->pCubeMainTxt[nRecipe].pOutputItem[nOutput];
    for (int32_t i = 0; i < 5; ++i)
    {
        const D2CubeOutputParamStrc *pParam = &pOutput->pParam[i];
        if (pParam->nMod < 0)
            continue;

        D2PropertyStrc tProperty = {};
        tProperty.nProperty = pParam->nMod;
        tProperty.nLayer = pParam->nModParam;
        tProperty.nMin = pParam->nModMin;
        tProperty.nMax = pParam->nModMax;
        ITEMRANGES_AddProperty(pTable, &tProperty);
    }
}

//  The sources of one item's magic list and runeword list: not its socketed items.
static void __fastcall ITEMRANGES_AddItemSources(D2ItemRangeTableStrc *pTable, D2UnitStrc *pItem)
{
    if (!pItem || pItem->dwUnitType != UNIT_ITEM || !pItem->pItemData)
        return;
    if (!(pItem->pItemData->dwItemFlags & IFLAG_IDENTIFIED))
        return;

    const D2C_ItemQualities nQuality = D2COMMON_ITEMS_GetItemQuality(pItem);

    //  the automagic affix is rolled for magic, rare and crafted items only; another quality may carry an id it never applied
    if (nQuality == ITEMQUAL_MAGIC || nQuality == ITEMQUAL_RARE || nQuality == ITEMQUAL_CRAFT)
        ITEMRANGES_AddAffix(pTable, pItem->pItemData->wAutoAffix);

    switch (nQuality)
    {
    case ITEMQUAL_MAGIC:
        ITEMRANGES_AddAffix(pTable, D2COMMON_ITEMS_GetPrefixId(pItem, 0));
        ITEMRANGES_AddAffix(pTable, D2COMMON_ITEMS_GetSuffixId(pItem, 0));
        break;

    case ITEMQUAL_RARE:
    case ITEMQUAL_CRAFT:
        for (int32_t i = 0; i < 3; ++i)
        {
            ITEMRANGES_AddAffix(pTable, D2COMMON_ITEMS_GetPrefixId(pItem, i));
            ITEMRANGES_AddAffix(pTable, D2COMMON_ITEMS_GetSuffixId(pItem, i));
        }
        break;

    case ITEMQUAL_UNIQUE:
    {
        const int32_t nFileIndex = D2COMMON_ITEMS_GetFileIndex(pItem);
        if (nFileIndex >= 0 && nFileIndex < sgptDataTables->nUniqueItemsTxtRecordCount)
            ITEMRANGES_AddProperties(pTable, sgptDataTables->pUniqueItemsTxt[nFileIndex].pProperties, 12);
        break;
    }

    case ITEMQUAL_SET:
        if (const D2SetItemsTxt *pSetItems = ITEMRANGES_GetSetItemsRecord(pItem))
        {
            ITEMRANGES_AddProperties(pTable, pSetItems->pProperties, 9);
            //  with no add func the partial bonuses are not tied to pieces: ITEMMODS_AssignProperty puts them in the magic list
            if (!pSetItems->nAddFunc)
                ITEMRANGES_AddProperties(pTable, pSetItems->pPartialBoni, 10);
        }
        break;

    default:
        break;
    }

    if (pItem->pItemData->dwItemFlags & IFLAG_RUNEWORD)
    {
        if (const D2RunesTxt *pRunes = ITEMRANGES_GetRunesRecord(pItem))
        {
            for (int32_t i = 0; i < 7 && pRunes->pProperties[i].nProperty >= 0; ++i)
                ITEMRANGES_AddProperty(pTable, &pRunes->pProperties[i]);
        }
    }

    ITEMRANGES_AddCraftRecipe(pTable, pItem);
}

//  The items in pItem's sockets: a gem or a rune by its gems.txt properties for the parent's apply type (ITEM_ApplyGemMods), anything else
//  (a jewel) by its own sources.
static void __fastcall ITEMRANGES_AddSocketedSources(D2ItemRangeTableStrc *pTable, D2UnitStrc *pItem)
{
    if (!pItem->pInventory)
        return;

    for (D2UnitStrc *pNode = D2COMMON_INVENTORY_GetFirstItem(pItem->pInventory); pNode; pNode = D2COMMON_INVENTORY_GetNextItem(pNode))
    {
        D2UnitStrc *pSocketed = D2COMMON_INVENTORY_UnitIsItem(pNode);
        if (!pSocketed)
            break;

        if (D2COMMON_ITEMS_CheckItemTypeId(pSocketed, ITEMTYPE_GEM) || D2COMMON_ITEMS_CheckItemTypeId(pSocketed, ITEMTYPE_RUNE))
        {
            const D2ItemsTxt *pItemsTxt = D2COMMON_DATATBLS_GetItemsTxtRecord(pSocketed->dwClassId);
            const D2GemsTxt *pGems = pItemsTxt ? D2COMMON_DATATBLS_GetGemsTxtRecord(pItemsTxt->dwGemOffset) : NULL;
            const int32_t nPropSet = D2COMMON_ITEMS_GetGemApplyType(pItem);
            if (pGems && nPropSet >= 0 && nPropSet < 3)
                ITEMRANGES_AddProperties(pTable, pGems->pProperties[nPropSet], 3);
        }
        else
        {
            ITEMRANGES_AddItemSources(pTable, pSocketed);
        }
    }
}

//  The full set's bonuses on the player: the partial bonuses of sets.txt for the pieces worn, two per piece past the first, and the full
//  bonuses with every piece (ITEMMODS_UpdateFullSetBoni).
static void __fastcall ITEMRANGES_AddFullSetSources(D2ItemRangeTableStrc *pTable, D2UnitStrc *pPlayer, D2UnitStrc *pSetItem)
{
    const D2SetItemsTxt *pSetItems = ITEMRANGES_GetSetItemsRecord(pSetItem);
    if (!pSetItems || pSetItems->nSetId < 0 || pSetItems->nSetId >= sgptDataTables->nSetsTxtRecordCount)
        return;

    const D2SetsTxt *pSets = &sgptDataTables->pSetsTxt[pSetItems->nSetId];
    const uint32_t dwMask = D2COMMON_ITEMS_GetSetItemsMask(pPlayer, pSetItem, TRUE);
    int32_t nPieces = 0;
    for (uint32_t dwBits = dwMask & 0x3F; dwBits; dwBits &= dwBits - 1)
        ++nPieces;

    //  pBoni2 .. pBoni5 are contiguous: eight properties, two for each count of pieces from two to five
    const int32_t nPartial = std::min(2 * (nPieces - 1), 8);
    if (nPartial > 0)
        ITEMRANGES_AddProperties(pTable, pSets->pBoni2, nPartial);
    if (nPieces >= pSets->nSetItems)
        ITEMRANGES_AddProperties(pTable, pSets->pFBoni, 8);
}

BOOL __fastcall ITEMRANGES_BuildTable(D2UnitStrc *pUnit, D2C_States nState, int32_t nFlag, D2ItemRangeTableStrc *pTable)
{
    pTable->nCount = 0;
    if (!ITEMRANGES_IsActiveFor(pUnit))
        return FALSE;

    if (pUnit->dwUnitType == UNIT_PLAYER)
    {
        if (nState >= STATE_ITEMSET1 && nState <= STATE_ITEMSET6)
            ITEMRANGES_AddFullSetSources(pTable, pUnit, gpItemRangesItem);
        return pTable->nCount > 0;
    }

    if (pUnit->dwUnitType != UNIT_ITEM)
        return FALSE;

    if (nState == STATE_NONE && nFlag == STATLIST_MAGIC)
    {
        ITEMRANGES_AddItemSources(pTable, pUnit);
        ITEMRANGES_AddSocketedSources(pTable, pUnit);
    }
    else if (nState >= STATE_ITEMSET1 && nState <= STATE_ITEMSET5 && D2COMMON_ITEMS_GetItemQuality(pUnit) == ITEMQUAL_SET)
    {
        //  the set item's own partial bonus for this state: setitems.txt aprop<n>a and aprop<n>b
        const D2SetItemsTxt *pSetItems = ITEMRANGES_GetSetItemsRecord(pUnit);
        if (pSetItems && pSetItems->nAddFunc)
            ITEMRANGES_AddProperties(pTable, &pSetItems->pPartialBoni[2 * (nState - STATE_ITEMSET1)], 2);
    }

    return pTable->nCount > 0;
}


//  ---- the text ----

static void __fastcall ITEMRANGES_AppendColorCode(Unicode *wszOut, uint32_t nColor)
{
    Unicode wszDigit[2];

    wszDigit[0].ch = (uint16_t)('0' + nColor);
    wszDigit[1].ch = 0;
    Unicode::strcat(wszOut, D2LANG_GetStringFromTblIndex(STR_IDX_3994_colorcode));
    Unicode::strcat(wszOut, wszDigit);
}

static void __fastcall ITEMRANGES_AppendAscii(Unicode *wszOut, const char *szText)
{
    Unicode wszText[64];

    Unicode::toUnicode(wszText, szText, 63);
    Unicode::strcat(wszOut, wszText);
}

//  " <grey>[" + szBody + "]<block colour>"
static void __fastcall ITEMRANGES_WrapSuffix(Unicode *wszOut, const Unicode *wszBody, uint32_t nBlockColor)
{
    wszOut[0].ch = 0;
    Unicode::strcat(wszOut, D2LANG_GetStringFromTblIndex(STR_IDX_3995_space));
    ITEMRANGES_AppendColorCode(wszOut, ITEMRANGES_COLOR);
    ITEMRANGES_AppendAscii(wszOut, "[");
    Unicode::strcat(wszOut, wszBody);
    ITEMRANGES_AppendAscii(wszOut, "]");
    ITEMRANGES_AppendColorCode(wszOut, nBlockColor);
}

//  "a-b", or "a" when they are equal
static void __fastcall ITEMRANGES_FormatPair(Unicode *wszOut, int32_t nLow, int32_t nHigh)
{
    char szText[32];

    if (nLow > nHigh)
        std::swap(nLow, nHigh);
    if (nLow == nHigh)
        STORM_SStrPrintf(szText, sizeof(szText), "%i", nLow);
    else
        STORM_SStrPrintf(szText, sizeof(szText), "%i-%i", nLow, nHigh);
    ITEMRANGES_AppendAscii(wszOut, szText);
}

static void __fastcall ITEMRANGES_BuildSingleSuffix(Unicode *wszOut, int32_t nLow, int32_t nHigh, uint32_t nBlockColor)
{
    wszOut[0].ch = 0;
    if (nLow == nHigh)
        return;

    Unicode wszBody[64];

    wszBody[0].ch = 0;
    ITEMRANGES_FormatPair(wszBody, nLow, nHigh);
    ITEMRANGES_WrapSuffix(wszOut, wszBody, nBlockColor);
}

//  A stored value as MODS_FormatPropertyLine shows it before its description function: op 2..5 scales it by the local player's op base stat,
//  then the stat's valshift, then a blunt weapon's +50% vs undead.
static int32_t __fastcall ITEMRANGES_ScaleValue(D2UnitStrc *pUnit, int32_t nStatId, int32_t nValue)
{
    const int32_t nCount = sgptDataTables->nItemStatCostTxtRecordCount;
    const D2ItemStatCostTxt *pStatCost = &sgptDataTables->pItemStatCostTxt[nStatId];

    int32_t nScaled = nValue;
    if (pStatCost->nOp >= 2 && pStatCost->nOp <= 5)
    {
        const int32_t nOpBase = pStatCost->wOpBase;
        if (nOpBase < 0 || nOpBase >= nCount)
            return 0;
        const D2ItemStatCostTxt *pOpBase = &sgptDataTables->pItemStatCostTxt[nOpBase];
        nScaled = (nValue * (D2COMMON_STATLIST_UnitGetStatValue(CUNIT_GetPlayer(), (D2C_ItemStats)nOpBase, 0) >> pOpBase->nValShift)) >> pStatCost->nOpParam;
    }

    nScaled >>= pStatCost->nValShift;
    if (nStatId == STAT_ITEM_UNDEADDAMAGE_PERCENT && pUnit && pUnit->dwUnitType == UNIT_ITEM && D2COMMON_ITEMS_CheckItemTypeId(pUnit, ITEMTYPE_BLUNT))
        nScaled += 50;
    return nScaled;
}

//  The number a description function prints for the scaled value; FALSE for the functions that print none, or print something else.
static BOOL __fastcall ITEMRANGES_GetShownNumber(D2UnitStrc *pUnit, int32_t nDescFunc, int32_t nLayer, int32_t nScaled, int32_t *pnShown)
{
    switch (nDescFunc)
    {
    case 1: case 2: case 3: case 4: case 6: case 7: case 8: case 9: case 12:
    case 13: case 14: case 16: case 19: case 22: case 23: case 27:
        *pnShown = nScaled;
        return TRUE;

    case 20:
    case 21:
        *pnShown = -nScaled;
        return TRUE;

    case 5:
    case 10:
        *pnShown = 100 * nScaled / 128;
        return TRUE;

    case 28:
    {
        //  capped at 3 for a skill of the owner's class, as the line caps it
        *pnShown = nScaled;
        if (nLayer >= 0 && nLayer < sgptDataTables->nSkillsTxtRecordCount)
        {
            D2UnitStrc *pOwner = (pUnit && pUnit->dwUnitType == UNIT_PLAYER) ? pUnit : CUNIT_GetPlayer();
            const int32_t nOwnerClass = pOwner ? pOwner->dwClassId : -1;
            if (sgptDataTables->pSkillsTxt[nLayer].nCharClass == nOwnerClass && nScaled > 3)
                *pnShown = 3;
        }
        return TRUE;
    }

    default:
        //  11 self-repair, 15 chance to cast, 17/18 by time, 24 charges, 25/26 (no stock stat): no range
        return FALSE;
    }
}

//  The description function the line uses: the group's when every member of the stat's description group has the same value (the line then
//  prints once for the group), the stat's own otherwise.
static int32_t __fastcall ITEMRANGES_GetDescFunc(D2StatListStrc *pStatList, int32_t nStatId)
{
    const D2ItemStatCostTxt *pStatCost = &sgptDataTables->pItemStatCostTxt[nStatId];
    if (!pStatCost->wDescGrp || !pStatList)
        return pStatCost->nDescFunc;

    const int32_t nValue = D2COMMON_STATLIST_GetStatValue(pStatList, (D2C_ItemStats)nStatId, 0);
    for (int32_t iOther = 0; iOther < sgptDataTables->nItemStatCostTxtRecordCount; ++iOther)
    {
        if (sgptDataTables->pItemStatCostTxt[iOther].wDescGrp != pStatCost->wDescGrp)
            continue;
        if (D2COMMON_STATLIST_GetStatValue(pStatList, (D2C_ItemStats)iOther, 0) != nValue)
            return pStatCost->nDescFunc;
    }
    return pStatCost->nDescGrpFunc;
}

//  The shown range of (nStatId, nLayer): FALSE when the table has none, the range is a single value, or nValue (the stored value) lies outside.
static BOOL __fastcall ITEMRANGES_GetShownRange(const D2ItemRangeTableStrc *pTable, D2UnitStrc *pUnit, int32_t nDescFunc, int32_t nStatId, int32_t nLayer,
                                                int32_t nValue, int32_t *pnLow, int32_t *pnHigh)
{
    if (nStatId < 0 || nStatId >= sgptDataTables->nItemStatCostTxtRecordCount)
        return FALSE;

    const D2ItemRangeEntryStrc *pEntry = ITEMRANGES_FindRange(pTable, nStatId, nLayer);
    if (!pEntry || pEntry->nMin == pEntry->nMax)
        return FALSE;

    const int32_t nValShift = sgptDataTables->pItemStatCostTxt[nStatId].nValShift;
    const int32_t nStoredMin = pEntry->nMin * (1 << nValShift);
    const int32_t nStoredMax = pEntry->nMax * (1 << nValShift);
    if (nValue < nStoredMin || nValue > nStoredMax)
        return FALSE;

    int32_t nLow = 0;
    int32_t nHigh = 0;
    if (!ITEMRANGES_GetShownNumber(pUnit, nDescFunc, nLayer, ITEMRANGES_ScaleValue(pUnit, nStatId, nStoredMin), &nLow)
        || !ITEMRANGES_GetShownNumber(pUnit, nDescFunc, nLayer, ITEMRANGES_ScaleValue(pUnit, nStatId, nStoredMax), &nHigh))
    {
        return FALSE;
    }

    if (nLow > nHigh)
        std::swap(nLow, nHigh);
    *pnLow = nLow;
    *pnHigh = nHigh;
    return nLow != nHigh;
}

void __fastcall ITEMRANGES_FormatStatSuffix(const D2ItemRangeTableStrc *pTable, D2UnitStrc *pUnit, D2StatListStrc *pStatList, int32_t nStatId, int32_t nLayer,
                                            int32_t nValue, Unicode *wszOut)
{
    int32_t nLow;
    int32_t nHigh;

    wszOut[0].ch = 0;
    if (!pTable || !pTable->nCount || nStatId < 0 || nStatId >= sgptDataTables->nItemStatCostTxtRecordCount)
        return;

    const int32_t nDescFunc = ITEMRANGES_GetDescFunc(pStatList, nStatId);
    if (ITEMRANGES_GetShownRange(pTable, pUnit, nDescFunc, nStatId, nLayer, nValue, &nLow, &nHigh))
        ITEMRANGES_BuildSingleSuffix(wszOut, nLow, nHigh, gnItemRangesBlockColor);
}

//  A damage pair's range as the line shows its numbers: nMin/nMax are the stored values the group was built from.
static BOOL __fastcall ITEMRANGES_GetRawRange(const D2ItemRangeTableStrc *pTable, int32_t nStatId, int32_t nValue, int32_t *pnLow, int32_t *pnHigh)
{
    *pnLow = nValue;
    *pnHigh = nValue;
    if (nStatId < 0 || nStatId >= sgptDataTables->nItemStatCostTxtRecordCount)
        return FALSE;

    const D2ItemRangeEntryStrc *pEntry = ITEMRANGES_FindRange(pTable, nStatId, 0);
    if (!pEntry)
        return FALSE;

    const int32_t nValShift = sgptDataTables->pItemStatCostTxt[nStatId].nValShift;
    const int32_t nLow = pEntry->nMin * (1 << nValShift);
    const int32_t nHigh = pEntry->nMax * (1 << nValShift);
    if (nValue < nLow || nValue > nHigh)
        return FALSE;

    *pnLow = nLow;
    *pnHigh = nHigh;
    return TRUE;
}

//  "[a-b to c-d]" for a damage line: the minimum's range and the maximum's; one of them may be a single value. Nothing when neither varies.
static void __fastcall ITEMRANGES_BuildPairSuffix(Unicode *wszOut, int32_t nMinLow, int32_t nMinHigh, int32_t nMaxLow, int32_t nMaxHigh, uint32_t nBlockColor)
{
    wszOut[0].ch = 0;
    if (nMinLow == nMinHigh && nMaxLow == nMaxHigh)
        return;

    Unicode wszBody[96];

    wszBody[0].ch = 0;
    ITEMRANGES_FormatPair(wszBody, nMinLow, nMinHigh);
    Unicode::strcat(wszBody, D2LANG_GetStringFromTblIndex(STR_IDX_3995_space));
    Unicode::strcat(wszBody, D2LANG_GetStringFromTblIndex(STR_IDX_4003_to));
    Unicode::strcat(wszBody, D2LANG_GetStringFromTblIndex(STR_IDX_3995_space));
    ITEMRANGES_FormatPair(wszBody, nMaxLow, nMaxHigh);
    ITEMRANGES_WrapSuffix(wszOut, wszBody, nBlockColor);
}

void __fastcall ITEMRANGES_FormatDamageGroupSuffix(const D2ItemRangeTableStrc *pTable, D2UnitStrc *pUnit, D2StatListStrc *pStatList,
                                                   const D2ClientDamageGroupStrc *pDamageGroups, int32_t nStatId, Unicode *wszOut)
{
    int32_t nMinLow;
    int32_t nMinHigh;
    int32_t nMaxLow;
    int32_t nMaxHigh;

    wszOut[0].ch = 0;
    if (!pTable || !pTable->nCount || !pStatList)
        return;

    int32_t nMinStat;
    int32_t nMaxStat;
    switch (nStatId)
    {
    case STAT_ITEM_MINDAMAGE_PERCENT:
    {
        //  "+N% Enhanced Damage": the minimum percentage, as the line prints it
        int32_t nLow;
        int32_t nHigh;
        const int32_t nValue = D2COMMON_STATLIST_GetStatValue(pStatList, STAT_ITEM_MINDAMAGE_PERCENT, 0);
        if (ITEMRANGES_GetShownRange(pTable, pUnit, 1, STAT_ITEM_MINDAMAGE_PERCENT, 0, nValue, &nLow, &nHigh))
            ITEMRANGES_BuildSingleSuffix(wszOut, nLow, nHigh, gnItemRangesBlockColor);
        return;
    }

    case STAT_MINDAMAGE:
    case STAT_MAXDAMAGE:
    case STAT_SECONDARY_MINDAMAGE:
        //  "Adds N-M damage": the group was built from the one-handed pair, or the two-handed one where that is zero
        nMinStat = D2COMMON_STATLIST_GetStatValue(pStatList, STAT_MINDAMAGE, 0) ? STAT_MINDAMAGE : STAT_SECONDARY_MINDAMAGE;
        nMaxStat = D2COMMON_STATLIST_GetStatValue(pStatList, STAT_MAXDAMAGE, 0) ? STAT_MAXDAMAGE : STAT_SECONDARY_MAXDAMAGE;
        break;
    case STAT_FIREMINDAM:
        nMinStat = STAT_FIREMINDAM;
        nMaxStat = STAT_FIREMAXDAM;
        break;
    case STAT_LIGHTMINDAM:
        nMinStat = STAT_LIGHTMINDAM;
        nMaxStat = STAT_LIGHTMAXDAM;
        break;
    case STAT_MAGICMINDAM:
        nMinStat = STAT_MAGICMINDAM;
        nMaxStat = STAT_MAGICMAXDAM;
        break;
    case STAT_COLDMINDAM:
        nMinStat = STAT_COLDMINDAM;
        nMaxStat = STAT_COLDMAXDAM;
        break;
    case STAT_POISONMINDAM:
        nMinStat = STAT_POISONMINDAM;
        nMaxStat = STAT_POISONMAXDAM;
        break;
    default:
        return;
    }

    const BOOL bMin = ITEMRANGES_GetRawRange(pTable, nMinStat, D2COMMON_STATLIST_GetStatValue(pStatList, (D2C_ItemStats)nMinStat, 0), &nMinLow, &nMinHigh);
    const BOOL bMax = ITEMRANGES_GetRawRange(pTable, nMaxStat, D2COMMON_STATLIST_GetStatValue(pStatList, (D2C_ItemStats)nMaxStat, 0), &nMaxLow, &nMaxHigh);
    if (!bMin && !bMax)
        return;

    if (nStatId == STAT_POISONMINDAM)
    {
        //  per frame in 256ths, shown over the length MODS_FormatElementalDamageText divided by the count: the group holds that length now
        const uint32_t nFrames = (uint32_t)pDamageGroups[5].nLengthFrames;
        nMinLow = (int32_t)((uint32_t)nMinLow * nFrames + 128u) >> 8;
        nMinHigh = (int32_t)((uint32_t)nMinHigh * nFrames + 128u) >> 8;
        nMaxLow = (int32_t)((uint32_t)nMaxLow * nFrames + 128u) >> 8;
        nMaxHigh = (int32_t)((uint32_t)nMaxHigh * nFrames + 128u) >> 8;
    }
    //  the other lines print the stored values as they are

    //  a line with one number ("Adds N fire damage") prints the maximum
    const D2ClientDamageGroupStrc *pGroup = NULL;
    switch (nStatId)
    {
    case STAT_FIREMINDAM: pGroup = &pDamageGroups[4]; break;
    case STAT_LIGHTMINDAM: pGroup = &pDamageGroups[3]; break;
    case STAT_MAGICMINDAM: pGroup = &pDamageGroups[6]; break;
    case STAT_COLDMINDAM: pGroup = &pDamageGroups[2]; break;
    case STAT_POISONMINDAM: pGroup = &pDamageGroups[5]; break;
    default: break;
    }
    if (pGroup && pGroup->nMin >= pGroup->nMax)
    {
        ITEMRANGES_BuildSingleSuffix(wszOut, nMaxLow, nMaxHigh, gnItemRangesBlockColor);
        return;
    }

    ITEMRANGES_BuildPairSuffix(wszOut, nMinLow, nMinHigh, nMaxLow, nMaxHigh, gnItemRangesBlockColor);
}

void __fastcall ITEMRANGES_InsertBeforeNewline(Unicode *wszLine, const Unicode *wszSuffix)
{
    if (!wszSuffix[0].ch)
        return;

    const int32_t nLength = Unicode::strlen(wszLine);
    if (nLength > 0 && wszLine[nLength - 1].ch == '\n')
    {
        wszLine[nLength - 1].ch = 0;
        Unicode::strcat(wszLine, wszSuffix);
        Unicode::strcat(wszLine, D2LANG_GetStringFromTblIndex(STR_IDX_3998_newline));
        return;
    }
    Unicode::strcat(wszLine, wszSuffix);
}


//  ---- the base values ----
//
//  These lines print what the item makes of its base (items.txt) with its percentages and flat bonuses, and with what the character adds, for
//  some of them. The range is the item's own value at the sources' minimums and maximums, moved by whatever the line adds to the item's own
//  value (nShown - the item's value): the character's part is the same at both ends. The item's own value must lie within the modelled range
//  (one either side for the rounding), or nothing is shown.

static int32_t __fastcall ITEMRANGES_Apply(int32_t nBase, int32_t nPercent, int32_t nFlat)
{
    return nBase + nBase * nPercent / 100 + nFlat;
}

static void __fastcall ITEMRANGES_GetTableRange(const D2ItemRangeTableStrc *pTable, int32_t nStatId, int32_t *pnLow, int32_t *pnHigh)
{
    *pnLow = 0;
    *pnHigh = 0;
    if (const D2ItemRangeEntryStrc *pEntry = ITEMRANGES_FindRange(pTable, nStatId, 0))
    {
        *pnLow = pEntry->nMin;
        *pnHigh = pEntry->nMax;
    }
}

static BOOL __fastcall ITEMRANGES_BuildBaseTable(D2UnitStrc *pItem, D2ItemRangeTableStrc *pTable)
{
    pTable->nCount = 0;
    if (!pItem || pItem != gpItemRangesItem || !ITEMRANGES_IsEnabled())
        return FALSE;
    ITEMRANGES_BuildTable(pItem, STATE_NONE, STATLIST_MAGIC, pTable);
    return TRUE;
}

//  The modelled range [nLow, nHigh] against the item's own value; on success the range moved onto the shown value.
static BOOL __fastcall ITEMRANGES_Anchor(int32_t nItemValue, int32_t nShown, int32_t *pnLow, int32_t *pnHigh)
{
    if (*pnLow == *pnHigh)
        return FALSE;
    if (nItemValue < *pnLow - 1 || nItemValue > *pnHigh + 1)
        return FALSE;

    *pnLow = std::min(*pnLow, nItemValue) + nShown - nItemValue;
    *pnHigh = std::max(*pnHigh, nItemValue) + nShown - nItemValue;
    return *pnLow != *pnHigh;
}

static void __fastcall ITEMRANGES_InsertSingle(Unicode *wszLine, int32_t nLow, int32_t nHigh, uint32_t nBlockColor)
{
    Unicode wszSuffix[96];

    ITEMRANGES_BuildSingleSuffix(wszSuffix, nLow, nHigh, nBlockColor);
    ITEMRANGES_InsertBeforeNewline(wszLine, wszSuffix);
}

void __fastcall ITEMRANGES_AppendDefenseRange(D2UnitStrc *pItem, int32_t nShown, uint32_t nBlockColor, Unicode *wszLine)
{
    D2ItemRangeTableStrc tTable;
    int32_t nPercentLow;
    int32_t nPercentHigh;
    int32_t nFlatLow;
    int32_t nFlatHigh;

    if (!ITEMRANGES_BuildBaseTable(pItem, &tTable))
        return;

    const D2ItemsTxt *pItemsTxt = D2COMMON_DATATBLS_GetItemsTxtRecord(pItem->dwClassId);
    if (!pItemsTxt)
        return;

    //  the base rolls minac..maxac, half as much again when ethereal; a base outside that (a rule that set it) is taken as fixed
    const int32_t nBase = D2COMMON_STATLIST_GetUnitBaseStat(pItem, STAT_ARMORCLASS, 0);
    int32_t nBaseLow = pItemsTxt->dwMinAc;
    int32_t nBaseHigh = pItemsTxt->dwMaxAc;
    if (pItem->pItemData && (pItem->pItemData->dwItemFlags & IFLAG_ETHEREAL))
    {
        nBaseLow = 3 * nBaseLow / 2;
        nBaseHigh = 3 * nBaseHigh / 2;
    }
    if (nBase < nBaseLow || nBase > nBaseHigh)
    {
        nBaseLow = nBase;
        nBaseHigh = nBase;
    }

    ITEMRANGES_GetTableRange(&tTable, STAT_ITEM_ARMOR_PERCENT, &nPercentLow, &nPercentHigh);
    ITEMRANGES_GetTableRange(&tTable, STAT_ARMORCLASS, &nFlatLow, &nFlatHigh);

    int32_t nLow = ITEMRANGES_Apply(nBaseLow, nPercentLow, nFlatLow);
    int32_t nHigh = ITEMRANGES_Apply(nBaseHigh, nPercentHigh, nFlatHigh);
    if (ITEMRANGES_Anchor(D2COMMON_STATLIST_GetDefenseFromUnit(pItem), nShown, &nLow, &nHigh))
        ITEMRANGES_InsertSingle(wszLine, nLow, nHigh, nBlockColor);
}

void __fastcall ITEMRANGES_AppendDamageRange(D2UnitStrc *pItem, D2C_ItemStats nMinStatId, D2C_ItemStats nMaxStatId, int32_t nShownMin, int32_t nShownMax,
                                             uint32_t nBlockColor, Unicode *wszLine)
{
    D2ItemRangeTableStrc tTable;
    int32_t nPercentLow;
    int32_t nPercentHigh;
    int32_t nFlatLow;
    int32_t nFlatHigh;

    if (!ITEMRANGES_BuildBaseTable(pItem, &tTable))
        return;

    //  the base damage is fixed (items.txt, half as much again when ethereal); the percentages and the flat bonuses roll
    const int32_t nBaseMin = D2COMMON_STATLIST_GetUnitBaseStat(pItem, nMinStatId, 0);
    const int32_t nBaseMax = D2COMMON_STATLIST_GetUnitBaseStat(pItem, nMaxStatId, 0);

    ITEMRANGES_GetTableRange(&tTable, STAT_ITEM_MINDAMAGE_PERCENT, &nPercentLow, &nPercentHigh);
    ITEMRANGES_GetTableRange(&tTable, nMinStatId, &nFlatLow, &nFlatHigh);
    int32_t nMinLow = ITEMRANGES_Apply(nBaseMin, nPercentLow, nFlatLow);
    int32_t nMinHigh = ITEMRANGES_Apply(nBaseMin, nPercentHigh, nFlatHigh);

    ITEMRANGES_GetTableRange(&tTable, STAT_ITEM_MAXDAMAGE_PERCENT, &nPercentLow, &nPercentHigh);
    ITEMRANGES_GetTableRange(&tTable, nMaxStatId, &nFlatLow, &nFlatHigh);
    int32_t nMaxLow = ITEMRANGES_Apply(nBaseMax, nPercentLow, nFlatLow);
    int32_t nMaxHigh = ITEMRANGES_Apply(nBaseMax, nPercentHigh, nFlatHigh);

    const BOOL bMin = ITEMRANGES_Anchor(D2COMMON_STATLIST_UnitGetStatValue(pItem, nMinStatId, 0), nShownMin, &nMinLow, &nMinHigh);
    const BOOL bMax = ITEMRANGES_Anchor(D2COMMON_STATLIST_UnitGetStatValue(pItem, nMaxStatId, 0), nShownMax, &nMaxLow, &nMaxHigh);
    if (!bMin && !bMax)
        return;
    if (!bMin)
        nMinLow = nMinHigh = nShownMin;
    if (!bMax)
        nMaxLow = nMaxHigh = nShownMax;

    Unicode wszSuffix[128];

    ITEMRANGES_BuildPairSuffix(wszSuffix, nMinLow, nMinHigh, nMaxLow, nMaxHigh, nBlockColor);
    ITEMRANGES_InsertBeforeNewline(wszLine, wszSuffix);
}

void __fastcall ITEMRANGES_AppendDurabilityRange(D2UnitStrc *pItem, int32_t nShownMax, uint32_t nBlockColor, Unicode *wszLine)
{
    D2ItemRangeTableStrc tTable;
    int32_t nPercentLow;
    int32_t nPercentHigh;
    int32_t nFlatLow;
    int32_t nFlatHigh;

    if (!ITEMRANGES_BuildBaseTable(pItem, &tTable))
        return;

    const int32_t nBase = D2COMMON_STATLIST_GetUnitBaseStat(pItem, STAT_MAXDURABILITY, 0);
    ITEMRANGES_GetTableRange(&tTable, STAT_ITEM_MAXDURABILITY_PERCENT, &nPercentLow, &nPercentHigh);
    ITEMRANGES_GetTableRange(&tTable, STAT_MAXDURABILITY, &nFlatLow, &nFlatHigh);

    int32_t nLow = ITEMRANGES_Apply(nBase, nPercentLow, nFlatLow);
    int32_t nHigh = ITEMRANGES_Apply(nBase, nPercentHigh, nFlatHigh);
    if (ITEMRANGES_Anchor(nShownMax, nShownMax, &nLow, &nHigh))
        ITEMRANGES_InsertSingle(wszLine, nLow, nHigh, nBlockColor);
}

void __fastcall ITEMRANGES_AppendSocketsRange(D2UnitStrc *pItem, int32_t nShown, uint32_t nBlockColor, Unicode *wszLine)
{
    D2ItemRangeTableStrc tTable;
    int32_t nLow;
    int32_t nHigh;

    if (!ITEMRANGES_BuildBaseTable(pItem, &tTable))
        return;

    const D2ItemsTxt *pItemsTxt = D2COMMON_DATATBLS_GetItemsTxtRecord(pItem->dwClassId);
    if (!pItemsTxt)
        return;

    //  at most the item's cells and its items.txt maximum for its level (ITEMMODS_PropertyFunc14)
    int32_t nMaxSockets = pItemsTxt->nInvWidth * pItemsTxt->nInvHeight;
    nMaxSockets = std::min(nMaxSockets, (int32_t)D2COMMON_ITEMS_GetMaxSockets(pItem));
    if (nMaxSockets <= 0)
        return;

    if (const D2ItemRangeEntryStrc *pEntry = ITEMRANGES_FindRange(&tTable, STAT_ITEM_NUMSOCKETS, 0))
    {
        //  a sockets property (a unique's, a set item's, a runeword base's affix)
        nLow = std::clamp(pEntry->nMin, 1, nMaxSockets);
        nHigh = std::clamp(pEntry->nMax, 1, nMaxSockets);
    }
    else
    {
        //  a plain or superior base rolls one up to its maximum; anything else has no range the client can name
        const D2C_ItemQualities nQuality = D2COMMON_ITEMS_GetItemQuality(pItem);
        if (nQuality != ITEMQUAL_NORMAL && nQuality != ITEMQUAL_SUPERIOR)
            return;
        nLow = 1;
        nHigh = nMaxSockets;
    }

    if (nShown < nLow || nShown > nHigh)
        return;
    ITEMRANGES_InsertSingle(wszLine, nLow, nHigh, nBlockColor);
}

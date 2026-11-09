//  Qol.h -- quality-of-life features that are not part of the original D2Client.dll. Include it after D2Client.h.
//
//  Everything declared here is an addition to the rewrite: the original has none of it. Each feature is switched by a global that holds a
//  D2ClientQolMode; how the player changes these globals (a menu, a key, a config file) is left for later, so for now they are set only by
//  their initialisers below.
#pragma once

enum D2ClientQolMode : int32_t
{
    QOLMODE_OFF = 0,                //  the feature is never shown: the client behaves as the original does
    QOLMODE_ALWAYS = 1,             //  the feature is always shown
    QOLMODE_WHILE_KEY_HELD = 2,     //  the feature is shown while its key (a Windows virtual-key code) is held down
};

//  The stat ranges shown after an item's stat lines ("+25% Faster Cast Rate [25-35]"), ItemRanges.cpp.
inline int32_t gnQolStatRangesMode = QOLMODE_ALWAYS;
inline int32_t gnQolStatRangesKey = VK_SHIFT;

//  The equipped item's description shown beside the hovered item's, ItemCompare.cpp.
inline int32_t gnQolItemCompareMode = QOLMODE_ALWAYS;
inline int32_t gnQolItemCompareKey = VK_SHIFT;

//  TRUE when a feature with this mode and key is to be shown now.
inline BOOL QOL_IsModeActive(int32_t nMode, int32_t nVirtualKey)
{
    switch (nMode)
    {
    case QOLMODE_ALWAYS:
        return TRUE;
    case QOLMODE_WHILE_KEY_HELD:
        return (GetAsyncKeyState(nVirtualKey) & 0x8000) != 0;
    default:
        return FALSE;
    }
}

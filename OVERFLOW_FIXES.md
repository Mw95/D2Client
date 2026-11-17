# OVERFLOW FIXES

The rewrite reproduces the original's bugs (see `README.md` and `BUGS.md`) with **one class of exceptions**: life and
damage arithmetic that wraps in 32 bits. Those sites are fixed, not reproduced, and each is marked
`DIFFERENCE: overflow fix` in the source. The server-side counterpart is in D2MOO's `doc/OverflowFixes.md`.

## Approach

Life, mana, stamina and damages are signed 32-bit values, mostly 24.8 fixed point. The client only **displays** them,
but it multiplies them for display in 32 bits, and those products wrap long before the values do. Every fix keeps the
stored values and the module interface unchanged and takes the product or sum in 64 bits, saturating the result to the
int32 range, through `Saturate.h` (`SATURATE_ToInt32`, `SATURATE_Add`, `SATURATE_Mul`, `SATURATE_MulDiv`,
`SATURATE_Shl`). Below the point where the original wrapped, every result is exactly the original's, with the same
truncation toward zero.

## Sites

| file | function | what wrapped in the original |
|---|---|---|
| `panel.cpp` | `PANEL_DrawHealthGlobe` | `80 * life / max` on the 24.8 life: an empty or broken globe from 104,858 life |
| `panel.cpp` | `PANEL_DrawManaGlobe` | the same for mana, from 104,858 mana |
| `panel.cpp` | both globes | the regeneration / recovery band, `80 * regen / 100` |
| `Gfx.cpp` | `GFX_GetUnitHilightPaletteIndex` | the hit flash's life percent, `100 * life`, from 83,887 life (a hireling's real life). The `BUGS.md` entry is now marked fixed |
| `ui_panels.cpp` | `UIPANELS_GetUnitLifeFill128` | the health bar's `life << 7`, from 65,536 life |
| `RosterPets.cpp` | `ROSTERPETS_UpdateLifeAndArea` | `max life * percent` for a hireling, from 83,887 life |
| `Shared.cpp` | `DATATBLS_ApplyRatio` | its "both small" branch multiplies up to 2^36 in 32 bits (signed overflow, undefined in C++); the others wrap too. The four branches are kept, so the divide-first rounding is unchanged |
| `inv.cpp` | the hireling page | total damage `damage * (percent + 100) / 100`, the elemental and poison sums, the strength/dexterity bonus |
| `inv.cpp` | the item damage lines | the by-time max-damage bonuses, Holy Shield's damage on a shield, a throwing item's missile damage |
| `SkillDesc.cpp` | the skill panel and tooltips, throughout | weapon/skill damage with srcdam (`* nSrcDam / 128`), the damage percentages, masteries, poison `damage * length`, elemental sums, the summon's life |

`SkillDesc.cpp` has too many sites for one comment each. Its file header carries the `DIFFERENCE` note, and every fixed
expression there goes through a `SATURATE_` helper. A few attack-rating percentages in the same functions use the
same helpers.

## What the client cannot fix on its own

* **Life above 32,767.** The server sends the player's life, mana and stamina in 15-bit fields (packets 0x18, 0x95,
  0x96). D2MOO's server now follows such a packet with the exact value in a 0x1D–0x1F stat packet whenever a value does
  not fit, and this client applies it as it always did. With an original server the life still arrives modulo 32,768.
* **The 24.8 ceiling**: 8,388,607 life or damage, the largest value a 32-bit 24.8 stat can hold.

## Verification

Each changed file was type-checked with Clang against stub Windows headers and compared with the unchanged sources: no
new diagnostics. The rewrite was not built with MSVC here, so the "objects identical" comparison mentioned in the commit
history was not run. It would no longer match for the files listed above, by design.

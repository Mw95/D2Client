# Plan: adding an eighth player class

Status: **plan only — nothing here is implemented.** It covers both rewrites: D2MOO (D2Common, D2Game, D2.exe launcher
code) and D2Client 1.10. File and line references are to the branches this plan was written on.

## 0. Constraints that decide the approach

1. **Both projects are faithful rewrites.** D2Client builds to objects identical to the original and its generated
   headers (`D2Constants.h`, `D2Structs.h`, `D2Ptrs.h`, `D2Vars.h`, `D2Imports.h`) are not edited by hand. D2MOO
   deliberately does not fix bugs. A new class is a *mod*, so it must live on a separate branch/fork, or behind a
   build switch (e.g. `D2_EXTRA_CLASSES`), so the faithful build stays available. The plan assumes a switch, with
   `NUMBER_OF_PLAYERCLASSES` driven by it.
2. **Every module that touches the class id has to be our code.** The rewritten D2Client calls the *original*
   D2Common.dll, D2Game.dll, D2Launch.dll, D2Win.dll etc. through their exports. Those hardcode 7 classes. So:
   - D2Common: use D2MOO's D2Common.dll as a drop-in (possible today, "not guaranteed bug-free").
   - D2Game: D2MOO cannot yet replace D2Game.dll as a whole; it patches functions in through D2.Detours
     (`D2.Detours.patches/1.10f`). Every D2Game function changed below has to be patched in, or D2Game has to become
     a full replacement first.
   - **D2Launch.dll (character creation and selection screens) is in neither repository.** This is the biggest
     blocker: without it you can't create the new class from the UI. The ways around it are listed in §5.
3. **Class id 7 is already taken.** `PCLASS_EVILFORCE = 7` is also `PCLASS_INVALID = 7`
   (`D2Common/include/Units/Player.h:45-47`, `D2Client/D2Constants.h:3087-3088`). Code and data use 7 to mean
   "no class": `ITEMS_GetClassOfClassSpecificItem` returns `PCLASS_INVALID` (`Items.cpp:3713`),
   `SKILLS_GetClassIdFromSkillId` returns 7 for classless skills, D2Client `mods.cpp:662` (descfunc 27) and
   `spellsel.cpp:164` test against it, and `ItemsMagic.cpp:167` compares affix class against it.
   **Decision:** the new class becomes id **7**, and "invalid" moves to a new value (e.g. `PCLASS_INVALID = 0xFF`,
   or `NUMBER_OF_PLAYERCLASSES`). Every *use* of `PCLASS_INVALID` / literal 7 meaning "none" then has to be audited.
   An id of 8 or higher would also break the 3-bit fields in §3.

## 1. Game data (.txt / .tbl / assets): mostly data-driven, mostly no code

Most class content already comes from the tables. The new class needs:

| Data | What to add |
|---|---|
| `playerclass.txt` | a new code (e.g. `xyz`). The linker `pPlayerClassLinker` resolves `charclass` (skills), `class` (itemtypes, cubemain, arena) and `classspecific` (affixes). |
| `charstats.txt` | a new row: base stats, per-level gains, `StrAllSkills`, `StrSkillTab1-3`, `StrClassOnly`, starting items, `StartSkill`, `BaseSkill1-10`. |
| `experience.txt` | a new column. **Needs code:** the loader hardcodes seven columns (§2.1). |
| `plrtype.txt` | the two-letter animation token (AM, SO, … → e.g. `XY`). |
| `skills.txt` / `skilldesc.txt` | about 30 skills with `charclass = xyz`, `SkillPage/Row/Column`, `srvstfunc/srvdofunc/cltstfunc/cltdofunc` (existing function numbers can be reused; new behaviour needs new code, §2.4/§4.4). |
| `itemtypes.txt` / `items.txt` | class-specific item type(s) with `Class = xyz` (optional). |
| `magicprefix/suffix.txt`, `automagic.txt` | `+skills`/`+tab` affixes for the class. |
| `itemstatcost.txt` | `item_addclassskills` / `item_addskill_tab` descriptions; check `Save Param Bits` (§3). |
| `inventory.txt` | two new rows (classic + expansion layout). **Needs code** (§2.1). |
| `sounds.txt` | per-class player sounds (hit, death, "I can't", level up, …). |
| `hireling.txt`, `cubemain.txt`, `arena.txt` | optional. |
| String tables | class name, `StrSklTree*` tab labels, `partychar*`, "(Xyz only)", `ModStr` for +class skills. |
| Animation assets | `DATA\GLOBAL\CHARS\XY\…` COF/DCC for every mode × weapon class × component, plus `animdata.d2` entries. |
| UI assets | skill tree chart, skill icon sheet, class icon (`Hireables\…Icon`), creation/selection screen art (D2Launch). |

## 2. D2MOO / D2Common

### 2.1 Enumerations, tables and fixed-size arrays
- `Units/Player.h`: add `PCLASS_<NEW> = 7`, set `NUMBER_OF_PLAYERCLASSES = 8`, move `PCLASS_INVALID` (§0.3).
  Rename or retire `PCLASS_EVILFORCE`.
- `D2DataTbls.h:165` `D2ExperienceTxt::dwClass[7]` → `[NUMBER_OF_PLAYERCLASSES]`; `DataTbls.cpp:837` add the
  column to the field table and move `ExpRatio`'s offset (28 → 32).
- `DataTbls/SkillsTbls.cpp:822-826` `nClassSkillCount` calloc of `7 * sizeof(int)` and `nClass < 7` →
  `NUMBER_OF_PLAYERCLASSES`. Check how `nClassSkillList` is laid out (`nHighestClassSkillCount` stride).
- `D2Inventory.h:53-75` `INVENTORYRECORD_*`: add records for the new class (both layouts); this shifts
  `NUM_INVENTORY_PAGE_STATS` and the `*2` records, so they should be appended, not inserted, to keep the row
  numbers in `inventory.txt`.
- `Units/Units.cpp:961` `nInventoryRecordMappings[]`; `Units.cpp:2780` `gaClassesWeaponFrameBonus[14][7]`
  (add a column); `Units.cpp:2843` assert.
- `DataTbls/ArenaTbls.h:22` `dwScores[7]` (only if arena is wanted).
- `DATATBLS_GetLevelThreshold/GetMaxLevel/GetCurrentLevelFromExp` (`DataTbls.cpp:121-145`) already use the
  constant.

### 2.2 Class-specific rules hardcoded in code (decide what the new class does)
- `Items/Items.cpp:4749-4775` `ITEMS_GetBonusLifeBasedOnClass` / `…ManaBasedOnClass`: the per-class multipliers
  for +life/+mana per level. Add a case (otherwise it falls to `default`).
- Dual wielding / claws: `D2Inventory.cpp:3202-3207`, `Units.cpp:3342,3700`, `D2Composit.cpp:299,352`,
  `Items.cpp:3829`, `D2Skills.cpp:1042`. These are only Barbarian/Assassin special cases; only touch them if the
  class dual-wields.
- `Items.cpp:1205,1217,3706`: class-specific item checks (use the constant; work once §0.3 is fixed).

### 2.3 Serialization: bit widths
- Ear items store the victim's class in **3 bits** (`Items.cpp:5054`, `dwFileIndex`). Id 7 fits; 8 doesn't.
- `STAT_ITEM_ADDCLASSSKILLS` legacy reads use 3-bit params (`Items.cpp:5981-6005`); the current format uses
  `itemstatcost.txt` Save Param Bits. `item_addskill_tab` packs the class into its param. Check both encodings for
  id 7.
- Packets carry the class as a byte, so they are fine.

### 2.4 Skills
- The client and server skill function tables are indexed by the `*func` columns. New behaviour means new
  functions appended to `D2Game/src/SKILLS/Skills.cpp:64` (`gpSkillSrvStartFnTable`) and `:160`
  (`gpSkillSrvDoFnTable`), in a new `SkillXyz.cpp`, plus D2Client's `gpSkillCltDoFuncs`/start table (§4.4).
- Skill descriptions are `skilldesc.txt` + `skilldesccode.bin`, so they need no code.

### 2.5 D2Common tests
- `D2Common/tests/D2CompositTests.cpp` and `TestDefinitions/.../PlrModeTypeTxtFixture.h:32`: add the new token
  and class to the fixtures; add tests for the experience table, inventory record mapping and class-skill counts.

## 3. D2MOO / D2Game (server side; must be patched in with D2.Detours or replace D2Game.dll)
- `PLAYER/PlrSave2.cpp:794`: `nClass > NUMBER_OF_PLAYERCLASSES` (lets `nClass == NUMBER_OF_PLAYERCLASSES` through)
  should become `>=`. The skill section (`"if"`, `PlrSave2.cpp:588,1085-1099`, `PlrSave.cpp:1105`) writes
  `nClassSkillCount[class]` bytes, so a 30-skill class keeps the format. Saves of the new class won't load in a
  stock game, which is expected.
- `GAME/Clients.cpp:582,624`: join validation uses the constant (OK once it is 8).
- `GAME/Game.cpp:2929-2947`: class description strings; add a case.
- Per-class special cases to decide on: `PlrModes.cpp:1644,1652` (Amazon/Druid modes), `PlrMsg.cpp:3528`,
  `PartyScreen.cpp:66` (which classes are "female"), `ObjMode.cpp:1297` (Assassin opens locked chests without a
  key), `ItemMode.cpp:1033,1038,5432` (dual wield/claws), `SkillNec.cpp:1179,1543`.
- Quests: NPC intro speeches pick a class-specific line (`A1Intro.cpp:128-140`, `A2Intro.cpp:156-178`,
  `A3Intro.cpp:135-152`, `A5Intro.cpp:156-170`), and `A1Q0.cpp:77`, `A3Q0.cpp:93` and `A3Q1.cpp:397` give
  class-specific dialogue. They all fall through to `PCLASS_INVALID`/a generic line, so the new class just needs
  to not collide with that value (§0.3). Optional: new speech entries.
- `ITEMS/Items.cpp:2774`, `ItemsMagic.cpp:167`: class-restricted affix and item generation (§0.3).
- New skill server functions (§2.4) and any new missiles or states (data plus functions).

## 4. D2Client (rewrite of D2Client.dll)

D2Client's generated headers would need either a regenerated `D2Constants.h` (new enum values) or a hand-written
override header behind the build switch. The arrays below are all `[7]` globals in `D2Vars.h`, and several carry
`static_assert`s that pin them to the original layout. Those asserts have to be made conditional, not deleted.

### 4.1 Per-class tables to grow to 8 rows
- `sgptPlayerSkillTreeChart[7]` (skillstree.h; chart DC6 per class); the skill tree tab labels are a hardcoded
  per-class `switch` in `skillstree.cpp:659-765`, with positions per tab. Add a case and new strings. Remove or
  extend the `static_assert`s at `skillstree.cpp:82`.
- `gClassSkillIcons[7]` (`spellsel.cpp:22,141,164,571`): the skill icon sheet per class.
- `gpClassIcons[7]` (`Hireables.cpp:71-101`): loaded from a 12-row name table that is **not** in class order (it
  has a latent overflow bug). The party/hireable `switch` at `Hireables.cpp:351-369` maps class to icon.
- `gpPlayerClassSounds[7]` (`UnitSnd.cpp:511,1463,1573`), plus the class-specific sound branches at
  `UnitSnd.cpp:1293-1303`.
- `gaPlayerPreloadTable[7]` (`Gfx.cpp:3827`, its static_assert pins `7 * sizeof == 0x118*4`): the per-class
  animation preload lists.
- `gptAttackSpeedClassOffsets[7][2]`, `gWeaponSpeedDescs` (inv.h): the attack-speed text in the item tooltip.
- `ClassName[7]` (Game.h).

### 4.2 Per-class switches and branches
- `Game.cpp:2505-2524` `GAME_ApplyParamPlayerClass` and `D2ConfigStrc` (`bAmazon…bAssassin`): a new
  command-line switch (also `D2MOO/source/Game/src/Main.cpp:43`, the launcher's `CMD_BOOLEAN` table).
- `Party.cpp:496-514` `PARTY_DrawClassLabel`; `inv.cpp:8116-8136` `INV_GetPlayerClassName` (ears), and
  `inv.cpp:8954,8984,9446,9612,9618` (Paladin shields, Assassin boots, "invalid class").
- `Roster.cpp:2295` (female classes), `CUnit.cpp:2081,2382,2716-2727` (Barbarian sequence mode, per-class
  overlay/weapon special cases), `SkillDesc.cpp:216,6754` (Assassin charge-up text), `mods.cpp:662,1330-1333`
  (descfunc 27 "no class" = 7: **this breaks when the class becomes 7**; see §0.3).

### 4.3 Imports
D2Client reads `sgptDataTables` fields directly (`nClassSkillCount`, `pCharStatsTxt`, …). If D2Common's
`D2DataTblsStrc` layout changes (it shouldn't: the new data goes behind existing pointers), the D2Client mirror
in `D2Structs.h` has to match.

### 4.4 Client skill functions
Add `SkillsXyz.cpp` next to `SkillsAma.cpp` … `SkillsAss.cpp`, and append its functions to the client start/do
tables (`Skills.cpp:1064` area). Existing functions can be reused through skills.txt.

## 5. Outside both repositories (blockers)
- **D2Launch.dll**: character creation (the animated class line-up with fixed positions and per-class assets) and
  character selection. The options:
  1. Reimplement the needed parts of D2Launch (large),
  2. binary-patch the original D2Launch (the class count, the class-selection table, positions, assets), or
  3. as a stopgap for testing, create the character with the existing `-xyz`-style command-line switch (§4.2)
     plus a direct-to-game launch, or with a hex-edited/generated `.d2s`.
- **D2Win / D2Gfx**: no class-specific code expected.
- **Battle.net/realm** (D2MCPClient, servers): out of scope. Single player and open TCP/IP only.

## 6. Suggested order of work
1. Build switch, `NUMBER_OF_PLAYERCLASSES = 8`, new `PCLASS_INVALID` value; audit every "7 means none" use in
   both repos (grep `PCLASS_INVALID`, `PCLASS_EVILFORCE`, literal `7` near class code).
2. D2Common: data tables (§2.1), serialization checks (§2.3), unit tests. Run it as the replacement D2Common.dll
   with a **copy** of an existing class's data (e.g. clone the Amazon's rows under the new code and token) so the
   engine can be validated before any new art exists.
3. D2Game: save/load, the class rules (§3) patched in through D2.Detours.
4. D2Client: the tables and switches (§4.1–4.2) using the cloned assets; a test character from the command line or
   a generated `.d2s` (§5.3).
5. Real content: skills (txt + new server/client functions), animations, sounds, UI art, strings.
6. Character creation/selection (D2Launch, §5).
7. Regression: the faithful (switch-off) builds stay identical. D2Client: 94/94 objects identical; D2MOO tests
   pass.

## 7. Risks
- The `PCLASS_INVALID == 7` sentinel is easy to miss. Any spot that is missed makes the new class look
  "classless" (no class-only items, wrong skill icons, wrong `+skills` text).
- Hardcoded `7`s that are not named after the class enum (e.g. `SkillsTbls.cpp:822-826`, the `cmp eax, 7`
  sites noted in D2Client's static_asserts) won't show up in a grep for `PCLASS_`. Grep for literal `7` and
  `[7]` in class-related code.
- Mixed module set: any original DLL left in place that knows only seven classes (D2Game functions that weren't
  patched, D2Launch) can crash or corrupt saves when it meets class 7.
- Asset volume: a full class animation set is the largest single piece of work, beyond the code.

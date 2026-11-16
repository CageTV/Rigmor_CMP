# Rigmor Compatibility Patches

Patches and fixes for **Rigmor of Bruma** and **Rigmor of Cyrodiil** on large, modern load orders. This repository is the backup and source of the files published on Nexus:

https://www.nexusmods.com/skyrimspecialedition/mods/190901

The repository root is a FOMOD installer. Each option below is pre-selected only when the mods it needs are active in your load order; an option whose requirements are missing cannot be ticked.

## Contents

| Option | What it does | Needs | Status |
|---|---|---|---|
| TKAA - Rigmor of Bruma Children Patch | TKAA child skin and body for Bruma's children | Rigmor.esp, TKAA.esp | on Nexus |
| TKAA - Rigmor of Cyrodiil Children Patch | The same for Rigmor of Cyrodiil | RigmorCyrodiil.esm, HearthFires, TKAA.esp | on Nexus |
| Rigmor Riverwood Forest | Rigmor of Bruma with Unique Locations - Riverwood Forest | Rigmor.esp, UniqueLocationsRiverwoodForest.esp, USSEP, the Creation Club files it was built against | on Nexus |
| BleakFalls Rigmor Boss | Rigmor with Bleak Falls Barrow Boss | Bleak Falls Barrow Boss.esp, Rigmor.esp | on Nexus |
| Rigmor Stall Fix (Rose Watcher + Guardian) | Un-sticks Rose's scene (stage 65-69) and watches all Rigmor scenes for stalls. Installs the Papyrus Rose Watcher (acts from the start) and the native Rigmor Guardian (observe mode, log only); switch to the Guardian later with `act` = true and disable the Rose Watcher plugin | Rigmor.esp; SKSE64 and Address Library for the Guardian | Rose Watcher on Nexus; Guardian not, untested in game |
| Rigmor Texture Fixes | 16 rebuilt textures: full mipmaps for eyes, brows and the baby cowl, goldbear_d resized to 1024x1024 | either Rigmor mod | not on Nexus |
| Real Weathers - Rigmor Patch | Real Weathers values for Rigmor of Cyrodiil's two own weathers | RigmorCyrodiil.esm, Real Weathers Remastered | not on Nexus, untested in game |
| Creature Disables - Rigmor | Removes selected creatures from Rigmor's areas | Skyrim Immersive Creatures | not on Nexus, never pre-selected |

The Rose Watcher and the Guardian do the same job and are installed together as one option. Run only one of them for real: after setting the Guardian's `act` to true, disable `TR_RigmorRoseWatcher.esp`.

## Layout

```
fomod/                    installer (info.xml, ModuleConfig.xml)
options/<option name>/    the files each option installs, in Data layout
source/Rigmor-Guardian/   C++ source of the Guardian SKSE plugin (GPL-3.0-or-later)
```

## Real Weathers - Rigmor Patch

Rigmor of Cyrodiil's regions list vanilla weather records, which Real Weathers already edits, so they need nothing. Only two weathers defined by Rigmor itself (`RigmorStormRain`, `RigmorSkyrimFogSnow`) are patched: they take the lighting, fog and volumetric-lighting values of the vanilla weathers they were cloned from. Their precipitation, clouds, flags and image spaces are unchanged.

## Licence

Rigmor Guardian is GPL-3.0-or-later (see `source/Rigmor-Guardian/LICENSE`). The plugins and textures are compatibility patches that need the mods named above; none of those mods' own files are included.

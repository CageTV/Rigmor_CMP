# Rigmor Guardian

A native SKSE plugin that keeps the Rigmor trilogy (Rigmor of Bruma, Rigmor of Cyrodiil) from stopping on a hung scene.
It replaces the Papyrus "Rose Watcher": instead of a script that wakes every 3 seconds, it reads the scene and quest state
directly once a second and, when a rule's condition has held too long, does what the mod's own scripts do (`Scene.Stop`, `Quest.SetStage`).

- **Rules** (`Data/SKSE/Plugins/RigmorGuardian.json`): a quest, a stage range, optionally one scene, a time limit and a list of actions
  (`stopScene`, `setStage`, `evaluatePackage`, `notify`). The shipped rules are the two for Bruma stage 65-69 (Rose's "she has lost a lot of blood" scene).
- **Observer**: every scene defined by Rigmor.esp / RigmorCyrodiil.esm / RigmorCyrodiilPatch.esp (1,059 scenes) is timed. A scene that stays in one phase for
  `phaseStallSeconds` (45) is written to the log as `STALL?` with its quest and stage, so the next stall becomes a rule instead of a guess.
- **Observe mode**: `settings.act` ships as `false`. The log then says what *would* be done and nothing is done. Set it to `true` when the log shows the rules
  firing at the right moments, and disable `TR_RigmorRoseWatcher.esp` at the same time (both acting is harmless but pointless).
- Pauses itself in menus, loading screens and for `settleSeconds` after a load. No Papyrus, no hooks, no Address Library addresses of its own.

Log: `Documents/My Games/Skyrim Special Edition/SKSE/RigmorGuardian.log`. Look for `rule '...' ready` at startup (a rule that cannot find its quest or scene says so and
turns itself off), `RULE '...' fired` and `STALL?`.

Requires SKSE64 and Address Library. Skyrim SE/AE 1.5.97 to 1.7.x (built on alandtse's CommonLibSSE-NG; no VR build).

Status: v0.1.0, builds clean, not yet tested in game.

License: GPL-3.0-or-later (see LICENSE). Copyright (c) 2026 CageTV.

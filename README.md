# mod-temucraft-auto-learn

Learn new class spells without walking back to a trainer, if a player wants to.

- **Automatic learning** (off by default, each player chooses): new class spells are learned the moment you reach
  their level, and on login. Turn it on or off with the checkbox in the spellbook, `.temucraft autolearn on|off`, or at any
  class trainer.
- **Learn everything now**: the spellbook's **Train** button (its tooltip lists what's waiting), `.temucraft autolearn now`, or
  "Teach me everything I can learn" at a class trainer.
- **Free or paid** (`Cost`): free, or the trainer's price for every spell (with your reputation discount at a
  trainer). What you can't pay yet stays for later and the button says how much it costs.

What can be learned comes from the class trainers' own lists (`trainer` / `trainer_spell`, Type 0), checked exactly
like a trainer checks it: level, earlier ranks, required spells and skills, race and class, and other modules' trainer
rules (the core's trainer hooks). So spells added, moved or removed at the trainers, by hand or by other modules
(such as Totemic Recall at level 10 or Parachute from [mod-temucraft-class-tweaks](https://github.com/Temucraft/mod-temucraft-class-tweaks)), are learned here the same way.
Pet, mount and profession trainers aren't used, and profession first ranks are never picked.

The spellbook button and checkbox are part of the Temucraft interface built by [mod-temucraft-client-version](https://github.com/Temucraft/mod-temucraft-client-version) (addon
messages, prefix TAL); without it the commands and the trainer options work on their own.

## Install

1. Put this folder in `modules/` of your AzerothCore source, re-run CMake and build.
2. The SQL in `data/sql/` is applied by the worldserver on its next start: `mod_temucraft_auto_learn` (characters,
   each character's choice) and the module's texts in `module_string`.
3. Copy `conf/mod-temucraft-auto-learn.conf.dist` to `configs/modules/` as `mod-temucraft-auto-learn.conf` to change
   the settings. `.reload config` applies them and reads the trainers' lists again.

## Works with other modules

- Class trainers that have a script of their own keep their own menu (the two trainer options are not added there);
  the spellbook button and the commands still work for those players.
- Use it instead of [mod-learn-spells](https://github.com/azerothcore/mod-learn-spells), not with it: both would teach spells on level up.
- [mod-temucraft-account-wide](https://github.com/Temucraft/mod-temucraft-account-wide)'s locked trainers stay locked in person (its check runs first), but a locked trainer's
  spells can still be learned this way when other trainers teach the same list (class trainers share theirs).
- Spells only quests teach (warlock pets, druid forms, shaman totems...) stay quest rewards, as at the trainers.

## Independence

This module doesn't need any other module, nor any other Temucraft module.

## Texts and translations

The 14 player-facing texts are in `module_string` under `mod-temucraft-auto-learn`. Translations go in
`module_string_locale`. If a row is missing the English text in the code is used.

Part of the Temucraft modules for AzerothCore 3.3.5a.

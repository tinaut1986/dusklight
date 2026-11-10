# UI translations

This fork translates the Dusklight interface (menus, settings, prompts). It does
not touch the game's own text, which comes from the disc and is chosen with
`game.language`.

## How it works

* Every `res/l10n/<code>.json` file is a language. `en.json` is the source and the
  fallback for anything a translation leaves out.
* Keys are dot paths (`settings.gameplay.bigger_wallets`) and the code reads them
  with `i18n::tr_str("...")`, or `i18n::tr_source("...")` for strings passed to
  `fmt` with named placeholders (`{count}`, `{name}`).
* The selected language is stored in `config.json` as `ui.language`: a file code
  (`"es"`, `"pt-BR"`) or `"auto"`, which matches the system locale (`pt_BR` tries
  `pt-BR`, then `pt`).
* When a language loads, any entry whose placeholders differ from English is
  dropped (and logged), so a typo in a translation shows English instead of
  crashing.

## Adding a language

1. Copy `res/l10n/en.json` to `res/l10n/<code>.json`, using a BCP 47 style code
   (`fr`, `de`, `it`, `pt-BR`).
2. Set the native name at the top: `"_meta": { "name": "Français" }`.
3. Translate the values. Keep keys, `{placeholders}` and RML markup
   (`<br/>`, `<b>`, `<modal-tip>`) as they are. Entries you remove fall back to
   English.
4. Run the checker and rebuild. The language shows up in the Prelaunch tab of the
   settings (Menu Language) with no code changes.

Supported scripts today: Latin (including accented Latin-1 letters, which also
uppercase correctly through `cmake/PatchRmlUi.cmake`). Cyrillic, Greek, Latin
Extended-A (ł, ő, č...) and CJK need font and uppercase work first.

## Checking translations

```sh
python tools/check_l10n.py            # coverage + errors
python tools/check_l10n.py --missing  # also list untranslated keys
python tools/check_l10n.py --lang es  # one language only
```

It fails when a key used in the code is missing from `en.json`, when a
translation's placeholders do not match English, or when a file has no `_meta`.
It warns about obsolete keys and keys no code references.

## Keeping the fork up to date

Upstream (`TwilitRealm/dusklight`) adds new options with hardcoded English text.
After each sync:

```sh
git fetch upstream
git switch main && git merge --ff-only upstream/main && git push origin main
git switch i18n && git merge main
```

Then:

1. Resolve conflicts, usually new options in `src/dusk/ui/settings.cpp`: keep the
   `tr_str(...)` calls and add the new options with new keys.
2. Look for new hardcoded strings in `git diff ORIG_HEAD -- src/dusk/ui` and move
   them to `en.json` plus each translation.
3. Run `python tools/check_l10n.py` until it passes, then build.

## Not translated yet

* Item names in the save editor (`src/dusk/ui/editor.cpp`, about 300 entries).
* Controller labels printed on the hardware ("L3", "Options", "Share") stay as
  they are on purpose.

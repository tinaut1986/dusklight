# Fork releases

This fork (`tinaut1986/dusklight`) ships its own builds on top of upstream
`TwilitRealm/dusklight`, with UI translations and an in-place AppImage updater.

## Two version numbers

| | Example | Used for |
| --- | --- | --- |
| Fork version | `v1.2.0` | The in-game updater. Only this number decides what is newer. |
| Base version | `v2.0.3` | The upstream release the build is built on. Shown, never compared. |

Release tags carry both: `v1.2.0+base.2.0.3`. Everything after `+` is semver
build metadata, which the updater ignores. The start screen shows
"1.2.0 · base 2.0.3".

The base version is detected from the nearest upstream tag (`vX.Y.Z`, no `+`) by
`cmake/ForkVersion.cmake`. The release workflow refuses a tag whose `+base`
part does not match it.

Bump the fork version as in the other ports: the patch number for each new
release (features included, e.g. v1.1.0 -> v1.1.1), the minor or major number
for bigger milestones. Only "higher than the installed one" matters to players.

## Branches and channels

* `release`: beta. Upstream is merged here and fork work lands here first. A tag
  on it publishes a **prerelease** (beta).
* `main`: stable. It only moves by merging `release`. A tag that `main` contains
  publishes a **release** and becomes `/releases/latest`.

Beta builds look for beta updates; stable builds only see stable releases unless
the player enables Settings → Interface → Beta Updates. Either way the updater
offers the **highest version** among the candidates, not the most recently
published one: a beta build is offered a newer stable, and a beta published
after a newer stable never hides it (`src/dusk/update_select.cpp`).

## Publishing

Beta:

```sh
git switch release
git tag v1.1.0+base.2.0.3
git push origin release v1.1.0+base.2.0.3
```

Stable with a new number (push `main` before the tag, or the build cannot see the
merge and ships a beta). Use `--no-ff` so the tag lands on its own merge commit:
two tags on one commit make the version baked into the binary ambiguous.

```sh
git switch main && git merge --no-ff release -m "Release v1.1.0" && git push origin main
git tag v1.1.0+base.2.0.3
git push origin v1.1.0+base.2.0.3
```

Promoting an existing beta as-is (same number): merge it into `main` and push,
then re-run the "Build Release" workflow on the beta tag (Actions → Build
Release → Run workflow → pick the tag). The same release page becomes stable
and latest. Do not add a second tag to that commit.

The workflow (`.github/workflows/release.yml`) builds the Linux x86_64 AppImage,
checks translations, and attaches the AppImage plus its `.sha256` file.

## Syncing upstream

```sh
git fetch upstream --tags
git switch release
git merge upstream/main            # or a specific upstream tag, e.g. v2.0.4
python tools/check_l10n.py         # see translations.md for new strings
git push origin release
git push origin refs/tags/v2.0.4   # upstream tags are needed to detect the base
```

Upstream tags never trigger a build here: the workflow only runs for tags with
`+base.`.

## In-game updater

The start screen checks this fork's releases (`src/dusk/app_info.hpp`). When the
game runs from an AppImage it downloads the `*-linux-x86_64.AppImage` asset next
to the current file, verifies the SHA-256 digest GitHub publishes for it, and
replaces the AppImage; the new version starts on the next launch
(`src/dusk/self_update.cpp`). Otherwise it opens the release page as upstream
does.

## Differences from upstream builds

* Updates come from this fork, not TwilitRealm.
* No Sentry crash reporting (not configured in this fork's builds).
* Discord Rich Presence is off by default (Settings → Interface).
* Touch: dragging scrolls lists (`src/dusk/ui/drag_scroll.cpp`), and real
  touchscreen input hides the arrow and draws a ring (`touch_feedback.cpp`).
  On the Steam Deck, Game Mode turns touches into mouse clicks unless the
  game's controller layout adds **Touchscreen Native Support** (System tab, as
  an always-on command); only then can the game tell a finger from the mouse.
* The mod catalog and remote textures still use `twilitrealm.dev`.

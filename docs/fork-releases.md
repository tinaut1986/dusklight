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

Bump the fork version as you like (semver is a good guide): a new upstream base
with no fork changes is usually a minor bump, fixes to fork code a patch bump.

## Branches and channels

* `release`: beta. Upstream is merged here and fork work lands here first. A tag
  on it publishes a **prerelease** (beta).
* `main`: stable. It only moves by merging `release`. A tag that `main` contains
  publishes a **release** and becomes `/releases/latest`.

Beta builds look for beta updates; stable builds only see stable releases unless
the player enables Settings → Interface → Beta Updates.

## Publishing

Beta:

```sh
git switch release
git tag v1.1.0+base.2.0.3
git push origin release v1.1.0+base.2.0.3
```

Stable (push `main` before the tag, or the build cannot see the merge and ships a
beta):

```sh
git switch main && git merge --ff-only release && git push origin main
git tag v1.1.0+base.2.0.3      # skip if the beta tag is already on this commit
git push origin v1.1.0+base.2.0.3
```

To promote an existing beta tag after merging it into `main`, re-run the
"Build Release" workflow on that tag (Actions → Build Release → Run workflow →
pick the tag). The same release page becomes stable and latest.

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
* The mod catalog and remote textures still use `twilitrealm.dev`.

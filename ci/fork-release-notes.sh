#!/bin/bash
# Fork release body (tinaut1986/dusklight). Used by .github/workflows/release.yml
# and to regenerate the body of an already published release.
#
#   ci/fork-release-notes.sh <tag> <prerelease:true|false> <asset name>
#
# User-facing notes come from docs/release-notes/<version>.md (English) and
# docs/release-notes/<version>.<lang>.md (other menu languages). Each goes
# between <!-- dusklight-notes:<lang> --> markers: the in-game What's New and
# Changelog windows show the block for the menu language, English otherwise,
# and the commit list ("## Changes") when there are no notes at all.
set -euo pipefail

TAG="$1"
PRERELEASE="$2"
ASSET="$3"

VERSION="${TAG%%+*}"
BASE=$(git describe --tags --abbrev=0 --match 'v[0-9]*.[0-9]*.[0-9]*' --exclude '*+*' "$TAG")
NOTES_DIR="docs/release-notes"

# Previous fork release reachable through any parent (a stable tag sits on a
# merge into main; the beta it promotes is on the second parent). With none,
# start at the upstream base so upstream history stays out.
PREV=$(git describe --tags --abbrev=0 --match 'v*+base.*' --exclude "$TAG" "$TAG" 2>/dev/null || true)
RANGE="${PREV:-$BASE}..${TAG}"

language_name() {
  # Native name from the menu translation, or the code itself.
  python3 - "$1" <<'EOF' 2>/dev/null || echo "$1"
import json, sys
print(json.load(open(f"res/l10n/{sys.argv[1]}.json", encoding="utf-8"))["_meta"]["name"])
EOF
}

if [ "$PRERELEASE" = "true" ]; then
  echo "> **Beta build.** It comes from the \`release\` branch and has not been"
  echo "> merged into \`main\` yet, so it is not a stable version."
  echo
fi
echo "Based on [TwilitRealm/dusklight ${BASE}](https://github.com/TwilitRealm/dusklight/releases/tag/${BASE})."
echo

if [ -f "${NOTES_DIR}/${VERSION}.md" ]; then
  echo "## What's new"
  echo
  echo "<!-- dusklight-notes:en -->"
  cat "${NOTES_DIR}/${VERSION}.md"
  echo
  echo "<!-- /dusklight-notes:en -->"
  echo
  for f in "${NOTES_DIR}/${VERSION}".*.md; do
    [ -f "$f" ] || continue
    lang="${f#${NOTES_DIR}/${VERSION}.}"
    lang="${lang%.md}"
    echo "<details><summary>$(language_name "$lang")</summary>"
    echo
    echo "<!-- dusklight-notes:${lang} -->"
    cat "$f"
    echo
    echo "<!-- /dusklight-notes:${lang} -->"
    echo
    echo "</details>"
    echo
  done
else
  echo "::warning::${NOTES_DIR}/${VERSION}.md is missing: the game will show the commit list for ${VERSION}" >&2
fi

echo "## Install"
echo
echo "- Linux / Steam Deck: \`${ASSET}\`"
echo "- Already installed from an AppImage: the game offers to install this"
echo "  version from its start screen, verified against the SHA-256 digest"
echo "  GitHub publishes for the file (also attached as \`.sha256\`)."
echo
echo "## Changes"
echo
if [ -n "$PREV" ]; then
  echo "Since ${PREV}:"
  echo
fi
# First parent only: upstream merges show as one line instead of hundreds of
# upstream commits.
git log --first-parent --pretty='- %s (%h)' "${RANGE}"

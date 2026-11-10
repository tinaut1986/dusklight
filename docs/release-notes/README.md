# Release notes

One file per fork version, `vX.Y.Z.md` (English), written and merged into
`release` **before** the tag exists. Optional translations sit next to it as
`vX.Y.Z.<lang>.md`, with `<lang>` a `res/l10n` code (`vX.Y.Z.es.md`).

`ci/fork-release-notes.sh` copies them into the release body between
`<!-- dusklight-notes:<lang> -->` markers. The game's What's New window and
Settings → Interface → Changelog show the block for the menu language, English
when that language has none, and the commit list when the version has no notes.

- 3 to 6 short lines, each starting with `- `, plain text for the player.
- A **beta** lists only what is new since the previous tag.
- A **stable** lists everything since the previous *stable* tag, betas
  included: players on the stable channel never see the beta pages, although
  the updater joins the notes of every release newer than theirs.
- Mention a new upstream base when it brings something players notice.

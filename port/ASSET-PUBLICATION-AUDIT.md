# Asset publication audit

Audited 2026-10-21 against port branch commit
`0bb804c4`. Nothing was uploaded or created on GitHub during this audit.

The requested condition is a public repository without Super Mario sprites,
audio or other game assets. A normal fork of this repository does not satisfy
that condition. The separate data submodule is not the whole asset boundary.

## Confirmed findings

- The current tree contains `resources/icons/smw.png`, visually inspected and
  confirmed to depict Mario. Other icon formats and documentation graphics are
  also tracked and require exclusion/review for a source-only publication.
- Six distinct `data.zip` blobs are reachable from the current port branch's
  history. Each was opened as a ZIP locally; they contain hundreds of media
  entries, including game graphics and/or sound effects. Their inventories alone
  disprove that preserving this history avoids uploading game media.
- `docs/gfx.zip`, `testmaps.zip`, documentation images and historical application
  icons are additional review/exclusion candidates. This initial audit is not an
  exhaustive clearance of every blob, embedded resource or third-party license.
- `data` currently is a Git submodule pointing to
  `https://github.com/mmatyas/supermariowar-data`. Uploading its gitlink does not
  upload its contents, but does not remove assets already in the parent history.

## Historical game-data archives

| Blob SHA-1 | ZIP entries | Media entries by filename extension |
| --- | ---: | ---: |
| `7e5808195ac3b9014a87303a546a1e6d8e9c18ca` | 1402 | 770 |
| `17abcbd96f4e07035692beb37e7feced2ddb0aee` | 1402 | 770 |
| `a7cdba97ab29d25fed917ee6edbaf20fd72bc442` | 1510 | 877 |
| `a6f5b465de8a5bfbdf3468360c9f61e9fe3d77e7` | 1510 | 877 |
| `fc65ad204d8dd3e0f480194d64875e63b740a84b` | 1509 | 877 |
| `7f85a6b30b5ca6ea58da9be68986853d83aaf30d` | 1510 | 877 |

Media counts include PNG/BMP/GIF/WAV/OGG/MP3/MID names; they are inventory counts,
not findings about ownership of every individual file.

## Publication options

A normal GitHub fork retains upstream history and its asset blobs. Deleting
files in a new commit or adding `.gitignore` does not remove those blobs.

A separate source-only repository can instead start with an audited source
snapshot and fresh history, retaining attribution and applicable licenses.
Full upstream history can remain in the local development checkout. Future
port updates would be exported deliberately into the source-only repository;
that repository would not have ordinary shared-history upstream merges.
History filtering is another option, but changes commit IDs and needs its own
complete audit. A patch-only repository can also avoid copying upstream files,
but patches must themselves be checked for embedded media or binary content.

Do not publish XBEs, screenshots, WAV recordings, game packages, or source
archives that embed assets as part of an asset-free source publication. Inspect
tracked files, reachable history, archives, embedded icons/resources and any
release/CI output before creating a public repository. Leave the working game,
local assets and original development history intact.

## Reproduce the initial inspection

From the modern repository:

```sh
git ls-tree -r --name-only HEAD
git rev-list --objects HEAD
git log --all --oneline -- data
```

For each historical `data.zip` object, obtain bytes with
`git cat-file blob OBJECT_ID` and inspect them using Python's `zipfile.ZipFile`.
A full publication audit must inspect content as well as filename extensions.

This is a technical inventory and publication boundary, not a legal clearance.

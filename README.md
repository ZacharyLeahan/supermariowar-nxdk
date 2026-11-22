# Super Mario War 2.0 for the original Xbox

Remember **Super Mario War 1.7 April Fools Edition** or **1.8 Beta 1** on Xbox? This port brings the later 2.0 game back to the original console. Upstream fixes include correctly sorted scores and a fix for foreground tiles carrying over from the previous map. On Xbox you can see which controllers are plugged in, reconnect a pad mid-match, and return to your dashboard from the familiar **Exit** button. [Score fix](https://github.com/mmatyas/supermariowar/commit/c7056790ccb485b7309137251167b75fed1482e4) · [Map fix](https://github.com/mmatyas/supermariowar/commit/5693918)

![Original Xbox main menu with a Controller S icon under each of the four connected player slots](https://gist.githubusercontent.com/ZacharyLeahan/9f19b59192bac0b21cd025a845fb8d25/raw/a6f024b6d1dbdee4b10249a87499551f1649fdc2/main-menu.jpg)

## Get it on your Xbox

1. **[Download the Xbox builder ZIP](https://github.com/ZacharyLeahan/supermariowar-nxdk/archive/f6edfe3610e0bfb54e339e814f55657be2a3d2e5.zip)** and extract it on your computer. This snapshot includes the audio fixes and Controller S icons.
2. **Windows 10 or 11:** Double-click **Build Super Mario War.cmd**. The first run downloads a verified native toolchain (about 489 MiB) plus the game sources and data. The script keeps the tools in `%LOCALAPPDATA%\SuperMarioWarXbox` for later builds. You can run it directly from Windows without setting up WSL or Python yourself.

   **Mac:** Install [Homebrew](https://brew.sh/) if you need it, then double-click **Build Super Mario War.command**. The launcher uses Homebrew to install the build tools.
3. When the build finishes, look on your desktop for **SuperMarioWar-Xbox**. Copy the **whole folder** to a writable games directory on your homebrew Xbox hard drive, for example `F:\Games\SuperMarioWar-Xbox\`. Keep `default.xbe` and the `data` folder together.
4. Launch **Super Mario War** from your dashboard, or open **default.xbe** in its file manager.

The ZIP contains the builder and port source. It fetches a specific revision of the game and its data on your computer. Keep the assembled Xbox folder for local play. [Source and asset notices](NOTICE.md)

## How it plays

Stomp your opponents, grab power-ups, and try to be the last player standing in **Classic**. Up to four players can share one Xbox, with bots to fill out a match. For a change of pace, steal the other team’s flag in **Capture the Flag** or fight over one patch of ground in **King of the Hill**. This Xbox port supports local multiplayer.

![A two-player Classic match on original Xbox hardware](https://gist.githubusercontent.com/ZacharyLeahan/9f19b59192bac0b21cd025a845fb8d25/raw/d4f18535c8c5cb3f88db83fa339ff7047e11c3af/classic-match.jpg)

## Controllers and the Exit menu

**Check your pads.** The portraits tell you who will play (human, bot, or empty). The small icon underneath tells you what’s plugged in. A Controller S means the pad is connected. A gray dash means the port is empty.

**Recover a match.** If a human player’s pad comes loose, the game pauses and shows which port needs it. Plug it back into that port and press **A** or **Start**. If several pads came loose, each player confirms before play resumes.

**Finish a session.** Choose **Exit** on the main menu to return to the dashboard, restart the Xbox, or power it off. Each action asks for confirmation.

![Xbox Exit menu showing Return to Dashboard, Restart Xbox, Power Off Xbox, and Cancel](https://gist.githubusercontent.com/ZacharyLeahan/9f19b59192bac0b21cd025a845fb8d25/raw/50b00e2a1fce0625e9a2cb776084adc615fd18b2/exit-menu.jpg)

## From the April Fools Edition to 2.0

Samuele Poletto made *Mario War* for DOS in 2002. In 2004 Florian Hufsky began the open-source rewrite that became *Super Mario War*. Xbox players got builds including **1.7 April Fools Edition** and **1.8 Beta 1**. The Xbox archive lists Beta 1 in 2008. The original project later reached **1.8 Beta 2**. [Xbox release history](https://www.gamebrew.org/wiki/Super_Mario_War_Xbox) · [Original project archive](https://github.com/bjorn/super-mario-war)

mmatyas continued development, bringing bug fixes, audio improvements, and support for newer platforms to the game. This Xbox port builds from the **October 4, 2026** revision of that 2.0 code. It was the last commit before upstream switched to SDL3, making it a fit for the SDL2 library in the open-source Xbox development kit, **nxdk**. [Upstream history and changes since 1.8](https://github.com/mmatyas/supermariowar/tree/c7056790ccb485b7309137251167b75fed1482e4#history) · [SDL3 transition](https://github.com/mmatyas/supermariowar/commit/cf1f6a5fe9e13565b4db2d37ab57d06b4f7d4397) · [XboxDev/nxdk](https://github.com/XboxDev/nxdk)

## Tested on Xbox

The screenshots above come from an original Xbox. Hardware checks covered a complete Classic match with music and effects, menu audio after a power cycle and dashboard relaunches, and the Controller S indicators at Xbox resolution. The audio fix also produced the startup sound in xemu. The physical test console has 128 MB of RAM. Testing on a stock 64 MB console is still open. [Hardware test notes](port/TESTING.md) · [Build verification](VERIFICATION.md)

<details>
<summary>Developer build and port details</summary>

For a manual build with nxdk’s host prerequisites installed, use a fresh workspace outside this repository:

```sh
python3 tools/prepare.py --workspace /tmp/smw-nxdk-dev
python3 tools/build.py --workspace /tmp/smw-nxdk-dev --clean
```

The XBE is written to `/tmp/smw-nxdk-dev/repos/modern/ports/nxdk/bin/default.xbe`. Add `--with-data` to the prepare command for a local play build, or use the desktop launcher above to assemble the Xbox folder automatically. The builder omits optional Contest worlds with filenames that FATX cannot store.

The lockfile records exact game, data, SDK and SDL revisions and the hashes of the port files and patches. Keep upstream checkouts, game assets and captures outside this repository. Run `python3 tools/audit.py` after committing changes to check reachable Git history. [Dependency lock](upstream-lock.json) · [Port notes](port/README.md) · [Notices and credits](NOTICE.md)

</details>

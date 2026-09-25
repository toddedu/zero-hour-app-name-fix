# Fix: "Failed to fetch Steam App Name!" – C&C Generals Zero Hour (Windows, Linux, Steam Deck)

**Who this is for:** players of the Steam version of *Command & Conquer™ Generals Zero Hour* who get these pop-ups when the game starts, after which it closes:

> Failed to fetch Steam App Name!
> Failed to initialize Steam! The game will now exit.

| System | Status |
|---|---|
| Linux (Proton) | ✅ Tested, works |
| Steam Deck | Should work (it's the same as Linux), not tested yet |
| Windows | ⚠️ **Not tested yet on a real Windows PC.** It's built to work there. If you try it, please report back in the thread. |

---

## What's going on

When the game starts, it looks up its own name on the Steam Store:

```
https://store.steampowered.com/api/appdetails?appids=2732960
```

The game expects the answer to be filed under its own ID, `"2732960"`. Since around Sep 24, 2026 the Store has been filing it under a different ID, `"2748390"`. The information is still correct, but the game looks under its own ID, finds nothing, and quits.

**It isn't your PC.** Reinstalling the game, Steam, your drivers or Windows won't help. It's a Steam Store problem that Valve or EA have to fix.

You can check for yourself by opening that link in a browser:
- Starts with `{"2748390":` → the Store still has the bug, and this fix will help.
- Starts with `{"2732960":` → Steam has fixed it, and you don't need this.

---

## How the fix works

It's one small file, `dinput8.dll`, that goes in the game folder. The game normally uses `dinput8.dll` for mouse and keyboard input, and it loads a copy from its own folder first. This copy passes all input straight on to the real `dinput8.dll` from your system.

It also changes exactly one thing: when the Store's reply to the name lookup is filed under the wrong ID, it puts it back under the game's own ID (`2748390` → `2732960`).

- It only affects this game. Steam and your other games don't use it.
- It doesn't modify any game files, and nothing is installed on your system.
- Once Steam fixes the Store, it finds nothing to change and does nothing.
- To remove it, delete the file (see **Undo** below).

---

## Step 1: Download the file and check it

Download `dinput8.dll` from the **[latest release](https://github.com/toddedu/zero-hour-app-name-fix/releases/latest)** (under *Assets*).

Before using it, **check that the file is exactly the one described here**. You're about to run a file from a stranger on the internet, so this check matters.

**Windows**: open PowerShell in the folder where you saved it (Shift + right-click in the folder → *Open PowerShell window here*) and run:
```
Get-FileHash dinput8.dll
```

**Linux / Steam Deck**: in a terminal, in the folder where you saved it:
```
sha256sum dinput8.dll
```

The result must be exactly the following. Windows shows it in capital letters, which is fine:
```
8a42642bb411f90e5af8871e5adcc1a89abd8659bbf46a0c93d0f740afe71548
```
(File size: 15,872 bytes.) If it's different, **don't use it**.

If you'd rather not trust a downloaded file, you can build it yourself from the source. See **Build it yourself** at the bottom.

## Step 2: Open the game folder

1. In your Steam Library, right-click **Command & Conquer™ Generals Zero Hour**.
2. Choose **Manage → Browse local files**.

A folder opens that contains `Game.dat` and `Generals.exe`.

> **Is there already a `dinput8.dll` in that folder?** Then another mod is using that name, and this fix would replace it. Don't overwrite it. Rename the existing file first (for example to `dinput8.dll.backup`) so you can put it back later, and be aware that the other mod won't work while this fix is in place.

## Step 3: Copy the file into that folder

Copy `dinput8.dll` into that folder, next to `Game.dat`.

**Steam Deck:** do Steps 1–3 in Desktop Mode (Dolphin file manager).

## Step 4: Play

Launch the game as usual. It should now go straight past the point where the error used to appear.

No launch options are needed, on any system.

---

## Step 5: Report back (Windows players especially!)

Every time the game starts, the fix writes a small text file called **`zh-appname-fix.log`** in the game folder, next to `dinput8.dll`. It records what the fix did, so problems can be tracked down. It contains no personal information: only your Windows version, the game's program name and what the fix did. No user names or folder paths.

Nobody has tested this on Windows yet, so **if you're on Windows, please post in the thread whether it worked**, and include the log:

1. Start the game once (whether it works or not), then close it.
2. Open the game folder (Step 2) and open `zh-appname-fix.log` with Notepad.
3. Copy everything in it into your reply in the thread, and say whether the game started.

A working log looks roughly like this (an illustration, your times and versions will differ):
```
09:50:26.550  zh-appname-fix 1.0 started, date 2026-09-25
09:50:26.550  system: Windows 10.0 build 19045
09:50:26.550  running on real Windows (not Wine)
09:50:26.550  program: Game.dat
09:50:26.831  real dinput8 loaded from C:\WINDOWS\SysWOW64\dinput8.dll - OK
09:50:26.831  patched 4 of 4 web functions - OK
09:50:26.831  watching for game error messages
09:50:26.831  mouse/keyboard input setup: 0x00000000 - OK
09:50:26.831  game asked the store for its name: /api/appdetails?appids=2732960
09:50:27.160  store reply: 7250 bytes
09:50:27.160  store reply starts: {"2748390":{"success":true,"data":{"type":"game
09:50:27.160  FIXED store reply key 2748390 -> 2732960 - OK
09:50:31.402  game closed (store name lookup was seen)
```

What to look for:
- Lines ending in **`- OK`** are good. Anything marked **`PROBLEM`** points to the cause.
- **No log file at all** → the game didn't load the fix. Check the file name and location (see Troubleshooting).
- **`game showed a message box: ...`** → the log records the exact text of any error the game showed, even ones not related to this fix.
- **`store reply key is already correct ... Steam may have fixed the store`** → Steam has fixed the problem, and you can remove the fix (see Undo).

---

## Troubleshooting

- **Still getting the error?**
  - Check that `dinput8.dll` is in the **same folder as `Game.dat`**, not in a subfolder.
  - Check that the file name is exactly `dinput8.dll`. Windows sometimes adds `(1)` to downloads, or hides extensions and turns it into `dinput8.dll.dll`.
  - **Linux / Steam Deck only:** add this to the start of the game's Launch Options (right-click the game → Properties → General), keeping anything already there, and try again:
    ```
    WINEDLLOVERRIDES="dinput8=n,b" %command%
    ```
- **Windows Defender or your antivirus deletes or blocks the file:** small unsigned files that change how a game behaves sometimes get flagged. The fingerprint check in Step 1 and the public source code are how you can check it's safe. Whether to allow it is your call.
- **Want proof it's working?** Open `zh-appname-fix.log` in the game folder (see Step 5) and look for `FIXED store reply key 2748390 -> 2732960 - OK`.
- **No `zh-appname-fix.log` in the game folder?** The fix only writes it there if it's allowed to. Otherwise it goes into your temp folder instead: on Windows, type `%TEMP%` into the File Explorer address bar.
- **The game won't start at all after adding the file?** Delete the file (see **Undo**) and post in the thread with your log, saying whether you're on Windows or Linux.

---

## Undo (once Steam fixes it, or anytime)

1. Delete `dinput8.dll` and `zh-appname-fix.log` from the game folder.
2. If you renamed another mod's `dinput8.dll` in Step 2, rename it back.
3. Linux: if you added `WINEDLLOVERRIDES="dinput8=n,b"` to the Launch Options, remove it.

That puts everything back exactly as it was.

---

## Build it yourself (optional)

The source is short, readable C, and it's all in this repository: `dinput8.c` (passes input through to the real dinput8), `appname_fix.c` (the fix itself) and `log.c` (the log file).

On Ubuntu/Debian:

```
sudo apt install gcc-mingw-w64-i686
./build.sh
```

The finished file appears as `build/dinput8.dll`. Its fingerprint won't match the one in Step 1, because different compiler versions produce slightly different files. That's expected. On Windows you can build it with MSYS2's `mingw-w64-i686-gcc` using the same commands as in `build.sh`.

---

*Tested on Ubuntu with Proton Experimental. The base game **Generals** (app 2229870) looks like it has the same Store problem. This fix should work for it too, but it hasn't been tested there.*

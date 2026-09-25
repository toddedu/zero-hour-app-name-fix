# Fix for "Failed to fetch Steam App Name!" – C&C Generals Zero Hour

Does *Command & Conquer™ Generals Zero Hour* on Steam show this message and then close?

> Failed to fetch Steam App Name!
> Failed to initialize Steam! The game will now exit.

This small file fixes it. It takes about a minute.

---

## The fix: 3 steps

### 1. Download the file

👉 **[Click here to download dinput8.dll](https://github.com/toddedu/zero-hour-app-name-fix/releases/latest/download/dinput8.dll)**

It goes to your **Downloads** folder. If your browser asks whether to keep the file, choose **Keep**.

### 2. Put it in the game folder

1. Open **Steam** and go to your **Library**.
2. **Right-click** *Command & Conquer™ Generals Zero Hour*.
3. Click **Manage**, then **Browse local files**. The game's folder opens.
4. **Drag the downloaded file** from your Downloads folder into this game folder.

### 3. Play

Start the game from Steam as usual. That's it! 🎉

---

**Did it work?** Please say so in the [Steam discussion thread](https://steamcommunity.com/app/2732960/discussions/0/586186803158068745/). It has been tested on Linux. It's made for Windows too, but reports from Windows players are very welcome.

**Didn't work?** See [If it didn't work](#if-it-didnt-work) below.

**To remove it later:** delete `dinput8.dll` from the game folder. Once Steam fixes the problem on their side, you won't need this file anymore. Leaving it in doesn't hurt.

---
---

*Everything below is optional: extra help and details for those who want them.*

## If it didn't work

- **Is the file in the right place?** It must be in the same folder as the file called `Game.dat` (sometimes shown just as `Game`), not in a folder inside it.
- **Is the file named exactly `dinput8.dll`?** If you downloaded it twice, it may be called `dinput8 (1).dll`. Rename it to `dinput8.dll` (or just `dinput8` if Windows hides the ending).
- **Did your antivirus remove it?** Windows Defender sometimes flags small unknown files like this one. See [Is this file safe?](#is-this-file-safe) below.
- **Was there already a file called `dinput8.dll` in the game folder?** Then another mod uses that name. Rename the old one to `dinput8.dll.backup` first, then copy this one in.
- **Linux / Steam Deck:** if it still doesn't work, right-click the game → **Properties** → **General**, and in **Launch Options** type:
  ```
  WINEDLLOVERRIDES="dinput8=n,b" %command%
  ```
  (If there's already something there, put `WINEDLLOVERRIDES="dinput8=n,b"` in front of it.)

**Still stuck? Please report it**, so it can be fixed for everyone:

1. Start the game once and close it again.
2. In the game folder (see Step 2), open the file **`zh-appname-fix.log`** by double-clicking it.
3. Copy everything in it and paste it into a reply in the [Steam discussion thread](https://steamcommunity.com/app/2732960/discussions/0/586186803158068745/), and say whether you're on Windows or Linux.

The log contains no personal information: no user names or folder paths.

<details>
<summary>What a working log looks like (click to expand)</summary>

An illustration. Your times and versions will differ:
```
09:50:26.550  zh-appname-fix 1.0 started, date 2026-09-25
09:50:26.550  system: Windows 10.0 build 19045
09:50:26.550  running on real Windows (not Wine)
09:50:26.550  program: Game.dat
09:50:26.831  real dinput8 loaded from C:\WINDOWS\SysWOW64\dinput8.dll - OK
09:50:26.831  patched 4 of 4 web functions - OK
09:50:26.831  watching for game error messages
09:50:26.831  game asked the store for its name: /api/appdetails?appids=2732960
09:50:27.160  store reply: 7250 bytes
09:50:27.160  store reply starts: {"2748390":{"success":true,"data":{"type":"game
09:50:27.160  FIXED store reply key 2748390 -> 2732960 - OK
09:50:27.502  mouse/keyboard input setup: 0x00000000 - OK
09:50:31.402  game closed (store name lookup was seen)
```
- Lines ending in **`- OK`** are good. Anything marked **`PROBLEM`** points to the cause.
- **No log file at all** means the game didn't load the fix. Check the file name and location above. (If the game folder can't be written to, the log goes to your temp folder instead: type `%TEMP%` into the File Explorer address bar.)
- **`game showed a message box: ...`** records the exact text of any error the game showed.
- **`store reply key is already correct ... Steam may have fixed the store`** means Steam has fixed the problem, and you can remove this file.
</details>

## What's going on?

When the game starts, it asks the Steam Store for its own name:

```
https://store.steampowered.com/api/appdetails?appids=2732960
```

The game expects the answer to be filed under its own ID, `"2732960"`. Since around 24 September 2026 the Steam Store has been filing it under a different ID, `"2748390"`. The information is still correct, but the game looks under its own ID, finds nothing, and quits.

**It isn't your PC.** Reinstalling the game, Steam, drivers or Windows won't help. It's a Steam Store problem that Valve or EA have to fix. You can check whether it's still broken by opening the link above in a browser. If the reply starts with `{"2748390":`, the Store still has the bug. If it starts with `{"2732960":`, Steam has fixed it.

## How does the fix work?

The game normally uses a file called `dinput8.dll` for mouse and keyboard input, and it looks in its own folder first. This `dinput8.dll` passes all input straight on to the real one from your system. It also changes exactly one thing: when the Store's reply to the name lookup is filed under the wrong ID, it puts it back under the game's own ID (`2748390` → `2732960`).

- It only affects this game. Steam and other games don't use it.
- It doesn't change any game files, and nothing is installed on your system.
- Once Steam fixes the Store, it finds nothing to change and does nothing.
- It writes a small log file (`zh-appname-fix.log`) in the game folder each time the game starts, replacing the previous one.

| System | Status |
|---|---|
| Linux (Proton) | ✅ Tested, works |
| Steam Deck | Should work (it's the same as Linux), not tested yet |
| Windows | Made for it, not tested yet on a real Windows PC. Reports welcome! |

## Is this file safe?

You're right to ask, since this is a file from the internet. How to check:

- **The full source code is on this page.** It's short, readable C: `dinput8.c` (passes input through to the real dinput8), `appname_fix.c` (the fix itself) and `log.c` (the log file).
- **Check the file's fingerprint** to make sure the file you downloaded is exactly the one released here.
  - Windows: open PowerShell in your Downloads folder (Shift + right-click in the folder → *Open PowerShell window here*) and run `Get-FileHash dinput8.dll`
  - Linux: `sha256sum dinput8.dll`

  The result must match the one in `SHA256SUMS.txt` on the [release page](https://github.com/toddedu/zero-hour-app-name-fix/releases/latest). Windows shows it in capital letters, which is fine.
- **Antivirus warnings:** small, unsigned files that change how a game behaves sometimes get flagged even when they're harmless. Whether to allow it is your call.

## Steam Deck

Switch to **Desktop Mode**, then follow the 3 steps above using the Dolphin file manager. If needed, the Launch Options tip under [If it didn't work](#if-it-didnt-work) can also be set in Game Mode: game → ⚙ → Properties.

## Build it yourself

On Ubuntu/Debian:

```
sudo apt install gcc-mingw-w64-i686
./build.sh
```

The finished file appears as `build/dinput8.dll`. Its fingerprint won't match the released one, because different compiler versions produce slightly different files. That's expected. On Windows you can build it with MSYS2's `mingw-w64-i686-gcc` using the same commands as in `build.sh`.

---

*Tested on Ubuntu with Proton Experimental. The base game **Generals** (app 2229870) looks like it has the same Store problem. This fix should work for it too, but it hasn't been tested there.*

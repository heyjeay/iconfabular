# IconFabular

A [Windhawk](https://windhawk.net/) mod that lets you change taskbar icons.

Hold **Shift** and right-click a taskbar icon: IconFabular's menu opens, you pick
a `.png` or `.ico` (or a JPEG, BMP, GIF, TIFF, WebP, or an icon out of another
`.exe`/`.dll`), and the taskbar starts using it. A fit mode and a scale
percentage let you resize the artwork so it sits in the button properly instead
of being stretched edge to edge.

The whole mod is one file: [`iconfabular.wh.cpp`](iconfabular.wh.cpp).

## Install

1. Install [Windhawk](https://windhawk.net/).
2. Open Windhawk → **Explore** → **Create a new mod**.
3. Replace the editor contents with [`iconfabular.wh.cpp`](iconfabular.wh.cpp),
   then click **Compile mod** and enable it.

It targets `explorer.exe` only, and loads into the running taskbar without a
restart.

## Use

| | |
|---|---|
| **Shift + right-click** a taskbar icon | IconFabular's menu (Change icon / Reset icon / All taskbar icons) |
| **Ctrl + Alt + Shift + I** | the manager window, if the gesture does not reach your taskbar |

In the manager you pick the app (pinned shortcuts and running apps are listed,
or browse for any `.exe`/`.lnk`), pick the icon file, then set:

- **Fit** — *Contain* (fit inside, keep the aspect ratio), *Cover* (fill the
  square and crop), *Stretch* (fill exactly), *Center* (no scaling).
- **Scale %** — 10 to 400. Below 100 leaves padding, which is how most app icons
  are drawn; a logo exported edge-to-edge usually looks right around 85%.
- **Nudge X / Y** — shifts the artwork by a percentage of the button, for art
  that is not centred in its own canvas.

The preview strip shows the result at 16, 24, 32 and 48 px over a transparency
checkerboard. **Apply** saves it; the override survives an explorer restart and
a Windhawk update.

Overrides can also be declared by hand on the mod's settings page, which is
handy for scripting a set of icons.

## How it works

Icons are re-composed for every size the taskbar asks for, with high-quality
(Fant) resampling, rather than scaling one bitmap up — so they stay sharp at any
DPI. Decoding happens through WIC on a worker thread; the hooks on the taskbar's
own threads only read finished bitmaps out of a cache, so the taskbar never
waits on file I/O.

Four icon paths are intercepted inside explorer:

| Hook | Covers |
|---|---|
| `WM_GETICON` via `SendMessageTimeoutW` | buttons of running windows |
| `PrivateExtractIconsW` | icons extracted from an `.exe` or `.lnk` |
| `SHDefExtractIconW` | the shell's default extraction path |
| `SHGetFileInfoW` | remaining `SHGFI_ICON` lookups |

`TrackPopupMenuEx` is hooked too, to append the entries to classic (HMENU)
taskbar menus where those still exist.

By default replacement is limited to explorer's taskbar threads, so File
Explorer windows and the desktop keep the original icons. Turn off *Only replace
taskbar icons* in the settings if you want it everywhere in explorer.

## Why a modifier + right-click

On Windows 11 the right-click menu for a taskbar button is a XAML jump list, not
a menu a mod can append an item to. So IconFabular shows its own menu on a
modifier + right-click, which works on both Windows 10 and 11. Where the classic
HMENU menu is in use (Windows 10, or Windows 11 with a tool that restores the
old taskbar), it also appends **Change icon** to that menu directly. The gesture
is configurable — Shift, Ctrl, Alt, Ctrl+Shift, plain right-click, or off.

## Known limitations

- **Packaged (Store/UWP) apps** resolve their icon from the package manifest
  rather than from an icon file, so their buttons are usually unaffected.
- **Pinned items can lag behind.** Running apps update immediately; a pinned,
  not-running item may keep showing the icon the shell already cached. Try
  *Refresh taskbar* in the manager, then *Restart Explorer*.
- Icons are replaced inside `explorer.exe` only — other apps, and a window's own
  title-bar icon, are untouched.
- Identifying which button you clicked uses UI Automation and name matching, so
  it can guess wrong on an unusual setup. The manager always lets you correct
  the target before applying.

## Development

The mod is compiled by Windhawk itself (MinGW-w64 GCC). To type-check it outside
Windhawk, stub the handful of `Wh_*` API functions it uses and build it as a DLL:

```sh
sudo apt install g++-mingw-w64-x86-64            # or your distro's equivalent
x86_64-w64-mingw32-g++ -std=c++20 -Wall -Wextra \
  -include windhawk_api_stub.h -c iconfabular.wh.cpp -o mod.o
```

`windhawk_api_stub.h` only needs the documented signatures of `Wh_Log`,
`Wh_Get/Set/DeleteValue`, `Wh_Get*Setting`, `Wh_FreeStringSetting` and
`Wh_SetFunctionHook`. Link with
`-lcomctl32 -lcomdlg32 -lgdi32 -lole32 -loleaut32 -lshell32 -lshlwapi -luser32 -lwindowscodecs`
(the same list as the mod's `@compilerOptions`).

## License

GPL-3.0, like Windhawk's own mods. See [LICENSE](LICENSE).

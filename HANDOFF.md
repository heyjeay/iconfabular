# Handoff notes

Context for whoever (or whatever session) picks this up next. Written at the end
of the session that wrote the mod, which could not run it: that session was a
Linux cloud container, so **nothing here has ever executed on Windows**.

## What this is

`iconfabular.wh.cpp` — a single-file [Windhawk](https://windhawk.net/) mod for
`explorer.exe` that replaces taskbar icons with user-supplied artwork
(ICO/PNG/JPEG/BMP/GIF/TIFF/WebP, or icon index *n* out of an EXE/DLL), with a fit
mode and scale so the art can be resized into the button.

Read `README.md` for what it does and how it is meant to be used. This file is
only about the state of the code and what to do next.

## Status

| | |
|---|---|
| Compiles | Yes — MinGW-w64 GCC 13.2, `-std=c++20 -Wall -Wextra`, zero warnings |
| Links | Yes, against exactly the libs in `@compilerOptions` |
| Run on Windows | **Never.** Not once. |
| Settings block | Validated as YAML, shape matches a published Windhawk mod |
| Source encoding | Pure ASCII (deliberate — clipboard round-trips into Windhawk's editor) |

So: treat every runtime behaviour below as a hypothesis, not a fact.

## Where things live

Functions, not line numbers, since those move.

| Concern | Start here |
|---|---|
| Image decode (WIC), ICO frame choice | `DecodeImageFile`, `DecodeBinaryIcon` |
| Fit / scale / nudge maths | `ComposeBitmap` |
| HICON creation | `CreateIconFromBitmap`, `BitmapFromIcon` |
| Caching + the hook/worker split | `LookupComposed`, `LookupSharedIcon`, `ProcessComposeQueue`, `PrewarmOverride` |
| Which app an icon request is for | `FindOverrideForPath`, `PathForWindow` |
| The four icon hooks | `*_Hook` functions, installed in `Wh_ModInit` via `HookExport` |
| Shift+right-click capture | `TaskbarSubclassProc`, `SetSubclassFromAnyThread`, `ScanTaskbar` |
| Classic menu injection | `TrackPopupMenuEx_Hook` |
| Guessing the clicked button | `UiaNameAtPoint`, `ScoreCandidate`, `IdentifyTargetAtPoint` |
| Manager window | `BuildManagerControls`, `ManagerWndProc`, `ApplyFromFields` |
| Persistence | `SaveCustomOverrides`, `LoadCustomOverrides` |

### The one invariant worth knowing

Taskbar threads never decode, never touch COM, never do file I/O. They only read
finished bitmaps out of `g_composedCache`. Everything slow happens on the worker
thread, which owns the WIC factory and a COM apartment. A cache miss in a hook
queues work and lets the original icon through that one time; `PrewarmOverride`
renders the common sizes up front so misses are rare. **Keep it that way** — a
blocking call on a taskbar thread janks the whole shell.

Second invariant: an HICON handed to `WM_GETICON` or pushed via `WM_SETICON` is
never destroyed (see `g_retiredIcons`). The taskbar and the owning window may
still hold it. Leaking a few small icons beats a dangling handle.

## The three things most likely to be broken

Debug in this order; each is independent.

### 1. Shift+right-click does nothing

The gesture relies on subclassing the window that actually receives taskbar
mouse input. `IsClickableTaskbarClass` accepts `MSTaskListWClass` (Win10) and
`Windows.UI.Input.InputSite.WindowClass` (Win11). If your build routes input
somewhere else, nothing is caught.

Diagnose: set `verboseLog`, then check whether `SetWindowsHookEx failed` appears,
and whether anything is subclassed at all. Add a log line in `ScanTaskbar`
listing the class name of every child of `Shell_TrayWnd` — that tells you what
to add to `IsClickableTaskbarClass`. Also worth testing whether the taskbar
sends `WM_RBUTTONUP` or `WM_POINTERUP` there; both are handled, but only one
likely fires.

Fallback that must keep working regardless: **Ctrl+Alt+Shift+I** opens the
manager, which is the whole feature minus the convenience.

### 2. The menu opens but names the wrong app (or no app)

`IdentifyTargetAtPoint` asks UI Automation for the element under the cursor and
fuzzy-matches its name against pinned shortcuts and running processes. Logs
`No app matched the clicked button named '<x>'` on failure, and the match on
success when verbose.

If the name comes back empty, UIA is not reaching the XAML button — try walking
up to the parent element, or reading `AutomationId` instead (both already
attempted; extend `UiaNameAtPoint`). If the name is right but the match is
wrong, fix `ScoreCandidate`. Non-fatal either way: the manager's dropdown lets
the user pick the app by hand.

### 3. A pinned, not-running app keeps its old icon

Most likely the shell served the icon from its own cache rather than calling
anything we hooked. Check whether `PrivateExtractIconsW` / `SHDefExtractIconW`
are hit at all for that app (verbose log). If they are not, the icon came from
the icon cache DB or from `IShellItemImageFactory`, neither of which is hooked.

Options if so: hook `SHGetImageList`/`IImageList::GetIcon`, or accept it and
rely on *Restart Explorer*. Do not add a COM vtable hook without thinking hard
about unload safety.

Also check `taskbarOnly`: if icon replacement is skipped entirely, it may be
that the icon is loaded on a thread `IsTaskbarThread` does not know about. Turn
the setting off to test that theory in one click.

## Windhawk API facts (verified against the docs, after getting them wrong)

- `Wh_GetStringValue(name, buffer, chars)` returns **`size_t`**, not `BOOL`.
  The code checks `buffer[0]` instead of the return value for that reason.
- There is **no** `Wh_DeleteStringValue`. It is `Wh_DeleteValue(name)`.
- Array settings are read as `Wh_GetStringSetting(L"overrides[%d].target", i)`.
- Entry points (`Wh_ModInit` etc.) are plain functions, no `extern "C"`.
- `Wh_Log` is a **macro**, not a function. It pastes a prefix onto the format
  string (`InternalWh_Log_Wrapper(L"[%d:%S]: " message, __LINE__, ...)`), so the
  format argument must be a *string literal* at the call site. A helper that
  takes a `PCWSTR format` and forwards it to `Wh_Log` does not compile - that is
  why `VerboseLog` is a variadic macro rather than a function.

## Compile-checking outside Windhawk

Windhawk compiles the mod itself, but you can reproduce its compile **exactly**
using Windhawk's own bundled clang and headers, which is far more faithful than
a MinGW cross-compiler with a stubbed API header (the stub declares `Wh_Log` as
a function and so misses the macro-only errors above):

```powershell
& "C:\Program Files\Windhawk\Compiler\bin\clang++.exe" `
  -x c++ -std=c++23 -target x86_64-w64-mingw32 `
  -DUNICODE -D_UNICODE -DWINVER=0x0A00 -D_WIN32_WINNT=0x0A00 -D_WIN32_IE=0x0A00 `
  -DNTDDI_VERSION=0x0A000008 -D__USE_MINGW_ANSI_STDIO=0 -DWH_MOD `
  -include windhawk_api.h `
  -Wall -Wextra -Wno-unused-parameter -Wno-missing-field-initializers `
  -Wno-cast-function-type-mismatch `
  -fsyntax-only iconfabular.wh.cpp
```

The flags come from `C:\ProgramData\Windhawk\EditorWorkspace\compile_flags.txt`.
Verified to produce byte-identical diagnostics to the Windhawk UI. Exit code 0
means it will compile in Windhawk.

This still only catches syntax, types and missing libraries. Every real bug left
in this mod is a runtime one.

## Test checklist

1. Compile in Windhawk, enable, confirm it loaded into `explorer.exe`.
2. Set logging to verbose in the mod's Advanced settings.
3. **Ctrl+Alt+Shift+I** → manager window opens. (Engine is alive.)
4. Override a *running* app (Notepad) with a PNG → button changes in a second
   or two. (Decode, compose, cache, `WM_GETICON`, persistence.)
5. Fit modes and Scale % visibly change the preview strip and the real button.
6. Shift+right-click a taskbar icon → menu, with the right app named.
7. Override a *pinned, closed* app → may need *Refresh taskbar* / *Restart
   Explorer*; note which was required.
8. Disable the mod → every icon returns to normal, no crash, no icon corruption.
9. Restart explorer → overrides still in effect (persistence survived).

Record which steps pass. Steps 1-5 are the core; 6-7 are the fragile parts.

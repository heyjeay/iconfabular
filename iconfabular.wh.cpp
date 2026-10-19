// ==WindhawkMod==
// @id              iconfabular
// @name            IconFabular
// @description     Right-click a taskbar icon to swap it for your own .ico or .png, and resize it to fit
// @version         1.0.0
// @author          heyjeay
// @github          https://github.com/heyjeay
// @include         explorer.exe
// @compilerOptions -lcomctl32 -lcomdlg32 -lgdi32 -lole32 -loleaut32 -lshell32 -lshlwapi -luser32 -lwindowscodecs
// ==/WindhawkMod==

// ==WindhawkModReadme==
/*
# IconFabular

Give any taskbar button the icon you want.

Hold **Shift** and right-click a taskbar icon: IconFabular's menu opens, you
pick a `.png`, `.ico` (or `.jpg`, `.bmp`, `.gif`, `.webp`, or an icon out of
another `.exe`/`.dll`), and the taskbar starts using it. A fit mode and a scale
percentage let you resize the artwork so it sits in the button the way the
built-in icons do, instead of being stretched edge to edge.

## What it does

- **Any image format** - PNG with transparency, ICO (the best-matching frame is
  picked per size, then scaled), JPEG, BMP, GIF, TIFF, WebP, or icon index *n*
  from an EXE/DLL/ICL (`C:\path\app.dll,3`).
- **Resize to fit** - *Contain* (default, keeps the aspect ratio), *Cover*
  (fills the square and crops), *Stretch*, or *Center* (no scaling), plus a
  scale percentage (handy for padding a logo down to ~85%) and X/Y nudges.
- **Per-size rendering** - the icon is re-composed for every size the taskbar
  asks for (16px through 256px) with high-quality Fant resampling, so it stays
  crisp at any DPI instead of being a blurry upscale of a 32px bitmap.
- **A manager window** - lists every override, lets you add one for any pinned
  shortcut or running app without clicking its button, and previews the result
  at 16/24/32/48px over a transparency checkerboard.
- **Persistence** - overrides made from the menu are saved by the mod itself, so
  they survive an explorer restart. You can also declare them by hand in the
  mod's settings below.

## How to use it

1. Shift + right-click a taskbar icon (configurable: Ctrl, Alt, Ctrl+Shift, or
   plain right-click).
2. Choose **Change icon...**, pick an image, adjust *Fit* and *Scale* until the
   preview looks right, press **Apply**.
3. **Reset icon** in the same menu puts the original back.

The gesture is a modifier + right-click because on Windows 11 the normal
right-click menu for a taskbar button is a XAML jump list that a mod cannot
append to. Where the classic HMENU-based menu is used (Windows 10, or Windows 11
with a tool that restores the old taskbar), IconFabular also appends its own
entries to that menu directly.

## Known limitations

- **Packaged (Store/UWP) apps** resolve their icon from the package manifest,
  not from an icon file, so their taskbar buttons are usually not affected.
- **Pinned items may lag behind.** Running apps update immediately; a pinned,
  not-running item can keep showing the icon the shell already cached. Use
  *Refresh taskbar* in the manager, and if the icon still sticks, restart
  explorer.
- Icons are replaced inside explorer.exe only. Other apps, and the window's own
  title-bar icon, are untouched.

## Settings

Everything below can be driven from the UI; the settings page is there for
making overrides by hand or scripting them.
*/
// ==/WindhawkModReadme==

// ==WindhawkModSettings==
/*
- gesture: shift
  $name: Gesture
  $description: Which modifier to hold while right-clicking a taskbar icon to open the IconFabular menu.
  $options:
  - shift: Shift + right-click
  - ctrl: Ctrl + right-click
  - alt: Alt + right-click
  - ctrlshift: Ctrl + Shift + right-click
  - none: Plain right-click (replaces the jump list)
  - off: No gesture (use the hotkey or the settings below)
- hotkey: true
  $name: Ctrl+Alt+Shift+I opens the manager
  $description: A fallback in case the right-click gesture does not reach the taskbar on your build.
- injectContextMenu: true
  $name: Add entries to classic context menus
  $description: Appends "Change icon" to HMENU-based taskbar menus (Windows 10, or Windows 11 with a restored classic taskbar). Has no effect on Windows 11 XAML jump lists.
- taskbarOnly: true
  $name: Only replace taskbar icons
  $description: Restricts the icon replacement to explorer's taskbar threads, so File Explorer windows and the desktop keep the original icons.
- matchFileNameOnly: false
  $name: Match apps by file name
  $description: Match chrome.exe no matter which folder it was started from. Off means the full path must match.
- hookWindowIcons: true
  $name: Replace icons of running windows
  $description: Answers the taskbar's WM_GETICON query for matched apps. Turning this off leaves running apps with their own icon.
- hookShellIcons: true
  $name: Replace icons extracted from files
  $description: Covers pinned shortcuts and anything whose icon the shell pulls out of an .exe or .lnk.
- refreshMode: windowIcon
  $name: How to refresh after a change
  $options:
  - windowIcon: Push the new icon to matched windows (quiet, recommended)
  - themeChange: Ask the taskbar to reload (more thorough, may flicker)
  - both: Both
  - none: Nothing - wait for the taskbar to ask again
- verboseLog: false
  $name: Verbose logging
  $description: Log every icon request that matches an override. For troubleshooting only.
- overrides:
  - - target: ""
      $name: Application
      $description: Full path to an .exe or a pinned .lnk, or just a file name such as notepad.exe.
    - icon: ""
      $name: Icon file
      $description: .png, .ico, .jpg, .bmp, .gif, .tif, .webp, or file.dll,3 to take icon index 3 out of a binary.
    - fit: contain
      $name: Fit
      $options:
      - contain: Contain - fit inside, keep aspect ratio
      - cover: Cover - fill and crop
      - stretch: Stretch - fill, ignore aspect ratio
      - center: Center - no scaling
    - scale: 100
      $name: Scale (%)
      $description: 10 to 400. Below 100 leaves padding around the artwork.
    - offsetX: 0
      $name: Nudge X (%)
    - offsetY: 0
      $name: Nudge Y (%)
  $name: Icon overrides
  $description: Overrides made from the menu are stored separately by the mod and do not show up here.
*/
// ==/WindhawkModSettings==

#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0A00
#endif

#include <windows.h>

#include <commctrl.h>
#include <commdlg.h>
#include <windowsx.h>
#include <objbase.h>
#include <shlobj.h>
#include <shlwapi.h>
#include <wincodec.h>

#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <cwchar>
#include <cwctype>
#include <deque>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#if __has_include(<uiautomation.h>)
#include <uiautomation.h>
#define ICONFABULAR_HAS_UIA 1
#endif

// Older mingw headers do not carry the pointer-input bits the Windows 11
// taskbar uses, so spell them out rather than depend on the SDK version.
#ifndef WM_POINTERDOWN
#define WM_POINTERDOWN 0x0246
#endif
#ifndef WM_POINTERUP
#define WM_POINTERUP 0x0247
#endif
#ifndef POINTER_MESSAGE_FLAG_SECONDBUTTON
#define POINTER_MESSAGE_FLAG_SECONDBUTTON 0x00000010
#endif
#define IFAB_POINTER_FLAGS(wp) ((DWORD)HIWORD(wp))
#define IFAB_POINTER_IS_RIGHT(wp) \
    ((IFAB_POINTER_FLAGS(wp) & POINTER_MESSAGE_FLAG_SECONDBUTTON) != 0)

// ---------------------------------------------------------------------------
// GUIDs. Defined locally so the mod does not depend on which GUID symbols the
// toolchain's import libraries happen to export.
// ---------------------------------------------------------------------------

static const GUID kCLSID_WICImagingFactory = {
    0xCACAF262, 0x9370, 0x4615, {0xA1, 0x3B, 0x9F, 0x55, 0x39, 0xDA, 0x4C, 0x0A}};
static const GUID kIID_IWICImagingFactory = {
    0xEC5EC8A9, 0xC395, 0x4314, {0x9C, 0x77, 0x54, 0xD7, 0xA9, 0x35, 0xFF, 0x70}};
static const GUID kWICPixelFormat32bppPBGRA = {
    0x6FDDC324, 0x4E03, 0x4BFE, {0xB1, 0x85, 0x3D, 0x77, 0x76, 0x8D, 0xC9, 0x10}};
static const GUID kCLSID_ShellLink = {
    0x00021401, 0x0000, 0x0000, {0xC0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x46}};
static const GUID kIID_IShellLinkW = {
    0x000214F9, 0x0000, 0x0000, {0xC0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x46}};
static const GUID kIID_IPersistFile = {
    0x0000010B, 0x0000, 0x0000, {0xC0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x46}};
static const GUID kFOLDERID_RoamingAppData = {
    0x3EB685DB, 0x65F9, 0x4CF6, {0xA0, 0x3A, 0xE3, 0xEF, 0x65, 0x72, 0x9F, 0x3D}};
#ifdef ICONFABULAR_HAS_UIA
static const GUID kCLSID_CUIAutomation = {
    0xFF48DBA4, 0x60EF, 0x4201, {0xAA, 0x87, 0x54, 0x10, 0x3E, 0xEF, 0x59, 0x4E}};
static const GUID kIID_IUIAutomation = {
    0x30CBE57D, 0xD9D0, 0x452A, {0xAB, 0x13, 0x7A, 0xC5, 0xAC, 0x48, 0x25, 0xEE}};
#endif

// ---------------------------------------------------------------------------
// Small helpers
// ---------------------------------------------------------------------------

template <typename T>
class ComPtr {
  public:
    ComPtr() = default;
    ComPtr(const ComPtr&) = delete;
    ComPtr& operator=(const ComPtr&) = delete;
    ComPtr(ComPtr&& other) : m_p(other.m_p) { other.m_p = nullptr; }
    ~ComPtr() { reset(); }

    void reset() {
        if (m_p) {
            m_p->Release();
            m_p = nullptr;
        }
    }

    T** operator&() { return &m_p; }
    T* operator->() const { return m_p; }
    T* get() const { return m_p; }
    explicit operator bool() const { return m_p != nullptr; }

  private:
    T* m_p = nullptr;
};

class BStr {
  public:
    BStr() = default;
    BStr(const BStr&) = delete;
    BStr& operator=(const BStr&) = delete;
    ~BStr() {
        if (m_s) {
            SysFreeString(m_s);
        }
    }
    BSTR* operator&() { return &m_s; }
    std::wstring str() const { return m_s ? std::wstring(m_s) : std::wstring(); }

  private:
    BSTR m_s = nullptr;
};

static std::wstring ToLower(std::wstring s) {
    std::transform(s.begin(), s.end(), s.begin(),
                   [](wchar_t c) { return (wchar_t)towlower(c); });
    return s;
}

static std::wstring Trim(const std::wstring& s) {
    size_t first = s.find_first_not_of(L" \t\r\n\"");
    if (first == std::wstring::npos) {
        return L"";
    }
    size_t last = s.find_last_not_of(L" \t\r\n\"");
    return s.substr(first, last - first + 1);
}

static std::wstring ExpandEnv(const std::wstring& s) {
    if (s.find(L'%') == std::wstring::npos) {
        return s;
    }
    wchar_t buffer[MAX_PATH * 2];
    DWORD len = ExpandEnvironmentStringsW(s.c_str(), buffer, ARRAYSIZE(buffer));
    if (len == 0 || len > ARRAYSIZE(buffer)) {
        return s;
    }
    return std::wstring(buffer);
}

static std::wstring BaseName(const std::wstring& path) {
    size_t pos = path.find_last_of(L"\\/");
    return pos == std::wstring::npos ? path : path.substr(pos + 1);
}

static std::wstring Extension(const std::wstring& path) {
    std::wstring name = BaseName(path);
    size_t pos = name.find_last_of(L'.');
    return pos == std::wstring::npos ? L"" : ToLower(name.substr(pos));
}

static bool IsBinaryIconSource(const std::wstring& path) {
    std::wstring ext = Extension(path);
    return ext == L".exe" || ext == L".dll" || ext == L".icl" || ext == L".ocx" ||
           ext == L".cpl" || ext == L".scr" || ext == L".mui";
}

static std::wstring FormatInt(int value) {
    wchar_t buffer[24];
    swprintf(buffer, ARRAYSIZE(buffer), L"%d", value);
    return std::wstring(buffer);
}

static int ParseInt(const std::wstring& s, int fallback) {
    if (s.empty()) {
        return fallback;
    }
    wchar_t* end = nullptr;
    long value = wcstol(s.c_str(), &end, 10);
    if (end == s.c_str()) {
        return fallback;
    }
    return (int)value;
}

static ULONGLONG FileStamp(const std::wstring& path) {
    WIN32_FILE_ATTRIBUTE_DATA data{};
    if (!GetFileAttributesExW(path.c_str(), GetFileExInfoStandard, &data)) {
        return 0;
    }
    ULARGE_INTEGER time{};
    time.LowPart = data.ftLastWriteTime.dwLowDateTime;
    time.HighPart = data.ftLastWriteTime.dwHighDateTime;
    return time.QuadPart ^ ((ULONGLONG)data.nFileSizeLow << 1);
}

// ---------------------------------------------------------------------------
// Model
// ---------------------------------------------------------------------------

enum class FitMode { Contain, Cover, Stretch, Center };

enum class Gesture { Shift, Ctrl, Alt, CtrlShift, None, Off };

enum class RefreshMode { None, WindowIcon, ThemeChange, Both };

struct IconOverride {
    std::wstring key;       // lowercase match key: full path, or a bare file name
    std::wstring display;   // what to show in the UI (original, un-lowercased)
    std::wstring iconPath;  // source image, environment variables expanded
    int iconIndex = 0;      // only used when iconPath is a binary
    FitMode fit = FitMode::Contain;
    int scalePercent = 100;
    int offsetXPercent = 0;
    int offsetYPercent = 0;
    bool fromSettings = false;  // declared in the mod settings, not editable here
};

static FitMode ParseFit(const std::wstring& s) {
    std::wstring v = ToLower(Trim(s));
    if (v == L"cover") {
        return FitMode::Cover;
    }
    if (v == L"stretch") {
        return FitMode::Stretch;
    }
    if (v == L"center") {
        return FitMode::Center;
    }
    return FitMode::Contain;
}

static PCWSTR FitName(FitMode fit) {
    switch (fit) {
        case FitMode::Cover:
            return L"cover";
        case FitMode::Stretch:
            return L"stretch";
        case FitMode::Center:
            return L"center";
        default:
            return L"contain";
    }
}

// A decoded, premultiplied BGRA image, top-down.
struct Bitmap32 {
    int width = 0;
    int height = 0;
    std::vector<uint32_t> pixels;

    bool valid() const {
        return width > 0 && height > 0 &&
               pixels.size() == (size_t)width * (size_t)height;
    }
};

using Bitmap32Ptr = std::shared_ptr<const Bitmap32>;

// ---------------------------------------------------------------------------
// Global state
// ---------------------------------------------------------------------------

struct Settings {
    Gesture gesture = Gesture::Shift;
    bool hotkey = true;
    bool injectContextMenu = true;
    bool taskbarOnly = true;
    bool matchFileNameOnly = false;
    bool hookWindowIcons = true;
    bool hookShellIcons = true;
    RefreshMode refresh = RefreshMode::WindowIcon;
    bool verboseLog = false;
};

static Settings g_settings;

static HINSTANCE g_hinst;
static volatile bool g_unloading;

// Overrides, keyed by IconOverride::key. Guarded by g_overrideMutex.
static std::mutex g_overrideMutex;
static std::unordered_map<std::wstring, IconOverride> g_overrides;
static std::unordered_map<std::wstring, std::wstring> g_overridesByName;

// Composed icon bitmaps and the handles handed to WM_GETICON callers, keyed by
// "<override key>|<generation>|<size>". Guarded by g_cacheMutex.
static std::mutex g_cacheMutex;
static std::unordered_map<std::wstring, Bitmap32Ptr> g_composedCache;
static std::unordered_map<std::wstring, HICON> g_iconCache;
static unsigned g_generation;  // bumped whenever an override changes

// Shortcut target and process path lookups, filled on the worker thread and
// only read from the hooks. Guarded by g_pathMutex.
static std::mutex g_pathMutex;
static std::unordered_map<std::wstring, std::wstring> g_linkTargets;
static std::unordered_map<DWORD, std::wstring> g_processPaths;

// Taskbar threads, so icon replacement can be kept out of File Explorer.
static constexpr size_t kMaxTaskbarThreads = 16;
static std::atomic<DWORD> g_taskbarThreads[kMaxTaskbarThreads];

// Worker thread.
static HANDLE g_workerThread;
static DWORD g_workerThreadId;
static HWND g_workerWindow;
static HWND g_managerWindow;

static UINT g_msgSubclass;

#define IFAB_WM_SHOW_MENU (WM_APP + 0x101)
#define IFAB_WM_SHOW_MANAGER (WM_APP + 0x102)
#define IFAB_WM_COMPOSE (WM_APP + 0x103)
#define IFAB_WM_RESCAN (WM_APP + 0x104)
#define IFAB_WM_REFRESH (WM_APP + 0x105)

#define IFAB_IDM_BASE 0x7E10
#define IFAB_IDM_CHANGE (IFAB_IDM_BASE + 0)
#define IFAB_IDM_RESET (IFAB_IDM_BASE + 1)
#define IFAB_IDM_MANAGER (IFAB_IDM_BASE + 2)

#define IFAB_HOTKEY_ID 0x4942

static void Log(PCWSTR message) {
    Wh_Log(L"%s", message);
}

// Wh_Log is a macro that pastes a prefix onto its format string, so the format
// has to be a string literal at the call site. That rules out a function taking
// a PCWSTR format, which is why this is a macro.
#define VerboseLog(format, ...)              \
    do {                                     \
        if (g_settings.verboseLog) {         \
            Wh_Log(format, __VA_ARGS__);     \
        }                                    \
    } while (0)

// ---------------------------------------------------------------------------
// Taskbar thread bookkeeping
// ---------------------------------------------------------------------------

static bool IsTaskbarThread() {
    DWORD self = GetCurrentThreadId();
    for (size_t i = 0; i < kMaxTaskbarThreads; i++) {
        DWORD tid = g_taskbarThreads[i].load(std::memory_order_relaxed);
        if (tid == 0) {
            break;
        }
        if (tid == self) {
            return true;
        }
    }
    return false;
}

static bool IconReplacementAllowedHere() {
    if (g_unloading) {
        return false;
    }
    if (!g_settings.taskbarOnly) {
        return true;
    }
    return IsTaskbarThread();
}

// ---------------------------------------------------------------------------
// Image decoding and composition
//
// All decoding and scaling happens on the worker thread, which owns the WIC
// factory and a COM apartment. The hooks only ever read the finished bitmaps
// out of g_composedCache, so no taskbar thread does file I/O or touches COM.
// ---------------------------------------------------------------------------

static IWICImagingFactory* g_wicFactory;  // worker thread only

static bool EnsureWicFactory() {
    if (g_wicFactory) {
        return true;
    }
    HRESULT hr = CoCreateInstance(kCLSID_WICImagingFactory, nullptr,
                                  CLSCTX_INPROC_SERVER, kIID_IWICImagingFactory,
                                  (void**)&g_wicFactory);
    if (FAILED(hr)) {
        Wh_Log(L"CoCreateInstance(WICImagingFactory) failed: 0x%08X", (unsigned)hr);
        return false;
    }
    return true;
}

static void PremultiplyInPlace(std::vector<uint32_t>& pixels) {
    for (uint32_t& value : pixels) {
        uint32_t a = (value >> 24) & 0xFF;
        if (a == 255) {
            continue;
        }
        uint32_t r = (value >> 16) & 0xFF;
        uint32_t g = (value >> 8) & 0xFF;
        uint32_t b = value & 0xFF;
        r = (r * a + 127) / 255;
        g = (g * a + 127) / 255;
        b = (b * a + 127) / 255;
        value = (a << 24) | (r << 16) | (g << 8) | b;
    }
}

// Reads an HICON into a premultiplied BGRA bitmap, used for icons taken out of
// an .exe/.dll and for the icons we hand back to WM_GETICON callers.
static bool BitmapFromIcon(HICON icon, Bitmap32* out) {
    ICONINFO info{};
    if (!GetIconInfo(icon, &info)) {
        return false;
    }

    BITMAP bm{};
    bool haveColor = info.hbmColor && GetObjectW(info.hbmColor, sizeof(bm), &bm);
    if (!haveColor) {
        // A 1bpp icon: the mask holds the AND and XOR halves stacked.
        if (info.hbmMask && GetObjectW(info.hbmMask, sizeof(bm), &bm)) {
            bm.bmHeight /= 2;
        } else {
            if (info.hbmColor) DeleteObject(info.hbmColor);
            if (info.hbmMask) DeleteObject(info.hbmMask);
            return false;
        }
    }

    int width = bm.bmWidth;
    int height = bm.bmHeight;
    bool ok = false;

    if (width > 0 && height > 0 && width <= 1024 && height <= 1024) {
        // Draw the icon over a transparent 32bpp DIB. DrawIconEx handles the
        // 32bpp, 24bpp-with-mask and 1bpp cases for us.
        BITMAPINFO bi{};
        bi.bmiHeader.biSize = sizeof(bi.bmiHeader);
        bi.bmiHeader.biWidth = width;
        bi.bmiHeader.biHeight = -height;
        bi.bmiHeader.biPlanes = 1;
        bi.bmiHeader.biBitCount = 32;
        bi.bmiHeader.biCompression = BI_RGB;

        HDC screen = GetDC(nullptr);
        HDC memDC = CreateCompatibleDC(screen);
        void* bits = nullptr;
        HBITMAP dib = CreateDIBSection(memDC, &bi, DIB_RGB_COLORS, &bits, nullptr, 0);
        if (dib && bits) {
            memset(bits, 0, (size_t)width * height * 4);
            HGDIOBJ old = SelectObject(memDC, dib);
            if (DrawIconEx(memDC, 0, 0, icon, width, height, 0, nullptr, DI_NORMAL)) {
                GdiFlush();
                out->width = width;
                out->height = height;
                out->pixels.assign((uint32_t*)bits,
                                   (uint32_t*)bits + (size_t)width * height);

                // DrawIconEx leaves the alpha channel of a legacy (non-32bpp)
                // icon at zero. Rebuild it from the mask in that case.
                bool anyAlpha = false;
                for (uint32_t value : out->pixels) {
                    if ((value >> 24) != 0) {
                        anyAlpha = true;
                        break;
                    }
                }
                if (!anyAlpha) {
                    for (uint32_t& value : out->pixels) {
                        value |= 0xFF000000;
                    }
                    if (info.hbmMask) {
                        // Mask bit set means transparent.
                        int stride = ((width + 31) / 32) * 4;
                        std::vector<BYTE> mask((size_t)stride * height, 0);
                        BITMAPINFO mi{};
                        mi.bmiHeader.biSize = sizeof(mi.bmiHeader);
                        mi.bmiHeader.biWidth = width;
                        mi.bmiHeader.biHeight = -height;
                        mi.bmiHeader.biPlanes = 1;
                        mi.bmiHeader.biBitCount = 1;
                        mi.bmiHeader.biCompression = BI_RGB;
                        if (GetDIBits(memDC, info.hbmMask, 0, height, mask.data(), &mi,
                                      DIB_RGB_COLORS)) {
                            for (int y = 0; y < height; y++) {
                                const BYTE* row = mask.data() + (size_t)y * stride;
                                for (int x = 0; x < width; x++) {
                                    if (row[x / 8] & (0x80 >> (x % 8))) {
                                        out->pixels[(size_t)y * width + x] = 0;
                                    }
                                }
                            }
                        }
                    }
                }
                ok = true;
            }
            SelectObject(memDC, old);
        }
        if (dib) {
            DeleteObject(dib);
        }
        DeleteDC(memDC);
        ReleaseDC(nullptr, screen);
    }

    if (info.hbmColor) {
        DeleteObject(info.hbmColor);
    }
    if (info.hbmMask) {
        DeleteObject(info.hbmMask);
    }
    return ok;
}

using PrivateExtractIconsW_t = UINT(WINAPI*)(LPCWSTR, int, int, int, HICON*, UINT*,
                                             UINT, UINT);
static PrivateExtractIconsW_t PrivateExtractIconsW_Original;

// Pulls icon `index` out of a binary at the largest size available.
static bool DecodeBinaryIcon(const std::wstring& path, int index, Bitmap32* out) {
    if (!PrivateExtractIconsW_Original) {
        return false;
    }
    static const int kSizes[] = {256, 128, 64, 48, 32};
    for (int size : kSizes) {
        HICON icon = nullptr;
        UINT id = 0;
        UINT count = PrivateExtractIconsW_Original(path.c_str(), index, size, size,
                                                   &icon, &id, 1, LR_DEFAULTCOLOR);
        if (count == 1 && icon) {
            bool ok = BitmapFromIcon(icon, out);
            DestroyIcon(icon);
            if (ok) {
                return true;
            }
        } else if (icon) {
            DestroyIcon(icon);
        }
    }
    return false;
}

// Decodes any WIC-supported image. For a multi-frame .ico the frame closest to
// (and preferably not smaller than) `desired` is chosen.
static bool DecodeImageFile(const std::wstring& path, int desired, Bitmap32* out) {
    if (!EnsureWicFactory()) {
        return false;
    }

    ComPtr<IWICBitmapDecoder> decoder;
    HRESULT hr = g_wicFactory->CreateDecoderFromFilename(
        path.c_str(), nullptr, GENERIC_READ, WICDecodeMetadataCacheOnLoad, &decoder);
    if (FAILED(hr)) {
        Wh_Log(L"CreateDecoderFromFilename(%s) failed: 0x%08X", path.c_str(),
               (unsigned)hr);
        return false;
    }

    UINT frameCount = 0;
    if (FAILED(decoder->GetFrameCount(&frameCount)) || frameCount == 0) {
        return false;
    }

    UINT bestFrame = 0;
    UINT bestScore = 0xFFFFFFFF;
    for (UINT i = 0; i < frameCount; i++) {
        ComPtr<IWICBitmapFrameDecode> frame;
        if (FAILED(decoder->GetFrame(i, &frame))) {
            continue;
        }
        UINT w = 0, h = 0;
        if (FAILED(frame->GetSize(&w, &h)) || w == 0 || h == 0) {
            continue;
        }
        UINT side = std::max(w, h);
        // Prefer the smallest frame that is still at least `desired` across;
        // frames smaller than that are ranked behind every large one.
        UINT score = side >= (UINT)desired ? side - (UINT)desired
                                           : 0x10000 + ((UINT)desired - side);
        if (score < bestScore) {
            bestScore = score;
            bestFrame = i;
        }
    }

    ComPtr<IWICBitmapFrameDecode> frame;
    if (FAILED(decoder->GetFrame(bestFrame, &frame))) {
        return false;
    }

    ComPtr<IWICFormatConverter> converter;
    if (FAILED(g_wicFactory->CreateFormatConverter(&converter))) {
        return false;
    }
    hr = converter->Initialize(frame.get(), kWICPixelFormat32bppPBGRA,
                               WICBitmapDitherTypeNone, nullptr, 0.0,
                               WICBitmapPaletteTypeCustom);
    if (FAILED(hr)) {
        Wh_Log(L"Format conversion failed: 0x%08X", (unsigned)hr);
        return false;
    }

    UINT width = 0, height = 0;
    if (FAILED(converter->GetSize(&width, &height)) || width == 0 || height == 0) {
        return false;
    }
    if (width > 4096 || height > 4096) {
        Log(L"Image is too large (max 4096px)");
        return false;
    }

    out->width = (int)width;
    out->height = (int)height;
    out->pixels.assign((size_t)width * height, 0);
    hr = converter->CopyPixels(nullptr, width * 4, (UINT)(out->pixels.size() * 4),
                               (BYTE*)out->pixels.data());
    return SUCCEEDED(hr);
}

static bool ScaleBitmap(const Bitmap32& src, int width, int height, Bitmap32* out) {
    if (width == src.width && height == src.height) {
        *out = src;
        return true;
    }
    if (!EnsureWicFactory()) {
        return false;
    }

    ComPtr<IWICBitmap> source;
    HRESULT hr = g_wicFactory->CreateBitmapFromMemory(
        (UINT)src.width, (UINT)src.height, kWICPixelFormat32bppPBGRA,
        (UINT)src.width * 4, (UINT)(src.pixels.size() * 4),
        (BYTE*)src.pixels.data(), &source);
    if (FAILED(hr)) {
        return false;
    }

    ComPtr<IWICBitmapScaler> scaler;
    if (FAILED(g_wicFactory->CreateBitmapScaler(&scaler))) {
        return false;
    }
    hr = scaler->Initialize(source.get(), (UINT)width, (UINT)height,
                            WICBitmapInterpolationModeFant);
    if (FAILED(hr)) {
        return false;
    }

    out->width = width;
    out->height = height;
    out->pixels.assign((size_t)width * height, 0);
    hr = scaler->CopyPixels(nullptr, (UINT)width * 4,
                            (UINT)(out->pixels.size() * 4), (BYTE*)out->pixels.data());
    return SUCCEEDED(hr);
}

// Lays `src` out inside a cx by cy transparent square per the override's fit
// mode, scale and nudges.
static Bitmap32Ptr ComposeBitmap(const Bitmap32& src, int cx, int cy,
                                 const IconOverride& ov) {
    if (!src.valid() || cx <= 0 || cy <= 0) {
        return nullptr;
    }

    double targetW = cx;
    double targetH = cy;
    switch (ov.fit) {
        case FitMode::Stretch:
            break;
        case FitMode::Cover: {
            double factor = std::max((double)cx / src.width, (double)cy / src.height);
            targetW = src.width * factor;
            targetH = src.height * factor;
            break;
        }
        case FitMode::Center: {
            targetW = src.width;
            targetH = src.height;
            if (targetW > cx || targetH > cy) {
                double factor =
                    std::min((double)cx / src.width, (double)cy / src.height);
                targetW = src.width * factor;
                targetH = src.height * factor;
            }
            break;
        }
        case FitMode::Contain:
        default: {
            double factor = std::min((double)cx / src.width, (double)cy / src.height);
            targetW = src.width * factor;
            targetH = src.height * factor;
            break;
        }
    }

    double scale = std::clamp(ov.scalePercent, 10, 400) / 100.0;
    int scaledW = std::clamp((int)std::lround(targetW * scale), 1, 4096);
    int scaledH = std::clamp((int)std::lround(targetH * scale), 1, 4096);

    Bitmap32 scaled;
    if (!ScaleBitmap(src, scaledW, scaledH, &scaled)) {
        return nullptr;
    }

    auto result = std::make_shared<Bitmap32>();
    result->width = cx;
    result->height = cy;
    result->pixels.assign((size_t)cx * cy, 0);

    int offsetX = (cx - scaledW) / 2 +
                  (int)std::lround(cx * std::clamp(ov.offsetXPercent, -100, 100) / 100.0);
    int offsetY = (cy - scaledH) / 2 +
                  (int)std::lround(cy * std::clamp(ov.offsetYPercent, -100, 100) / 100.0);

    for (int y = 0; y < scaledH; y++) {
        int destY = y + offsetY;
        if (destY < 0 || destY >= cy) {
            continue;
        }
        for (int x = 0; x < scaledW; x++) {
            int destX = x + offsetX;
            if (destX < 0 || destX >= cx) {
                continue;
            }
            result->pixels[(size_t)destY * cx + destX] =
                scaled.pixels[(size_t)y * scaledW + x];
        }
    }
    return result;
}

static HICON CreateIconFromBitmap(const Bitmap32& image) {
    if (!image.valid()) {
        return nullptr;
    }

    BITMAPV5HEADER header{};
    header.bV5Size = sizeof(header);
    header.bV5Width = image.width;
    header.bV5Height = -image.height;  // top-down
    header.bV5Planes = 1;
    header.bV5BitCount = 32;
    header.bV5Compression = BI_BITFIELDS;
    header.bV5RedMask = 0x00FF0000;
    header.bV5GreenMask = 0x0000FF00;
    header.bV5BlueMask = 0x000000FF;
    header.bV5AlphaMask = 0xFF000000;

    HDC screen = GetDC(nullptr);
    void* bits = nullptr;
    HBITMAP color = CreateDIBSection(screen, (BITMAPINFO*)&header, DIB_RGB_COLORS,
                                     &bits, nullptr, 0);
    ReleaseDC(nullptr, screen);
    if (!color || !bits) {
        if (color) {
            DeleteObject(color);
        }
        return nullptr;
    }
    memcpy(bits, image.pixels.data(), image.pixels.size() * 4);

    // A 32bpp icon carries its own alpha, so an all-zero (fully opaque) mask is
    // what belongs here - but CreateBitmap does not clear its bits for us.
    size_t maskStride = (size_t)((image.width + 15) / 16) * 2;
    std::vector<BYTE> maskBits(maskStride * image.height, 0);
    HBITMAP mask = CreateBitmap(image.width, image.height, 1, 1, maskBits.data());
    if (!mask) {
        DeleteObject(color);
        return nullptr;
    }

    ICONINFO info{};
    info.fIcon = TRUE;
    info.hbmColor = color;
    info.hbmMask = mask;
    HICON icon = CreateIconIndirect(&info);

    DeleteObject(color);
    DeleteObject(mask);
    return icon;
}

// ---------------------------------------------------------------------------
// Composed icon cache
// ---------------------------------------------------------------------------

static std::wstring MakeCacheKey(const std::wstring& overrideKey, unsigned generation,
                                 int cx, int cy) {
    wchar_t suffix[48];
    swprintf(suffix, ARRAYSIZE(suffix), L"|%u|%dx%d", generation, cx, cy);
    return overrideKey + suffix;
}

// Worker-thread only: decoded source images, so a png is not re-read for every
// size the taskbar asks about.
static std::unordered_map<std::wstring, Bitmap32Ptr> g_sourceCache;

static std::deque<std::wstring> g_composeQueue;  // guarded by g_cacheMutex
static std::unordered_set<std::wstring> g_composeQueued;

// Handles that went out through WM_GETICON, or were pushed into another
// process with WM_SETICON, are not ours to destroy: the taskbar and the owning
// window may still be holding them. They are retired instead, which leaks a few
// small icons per change - far better than handing anyone a dangling handle.
static std::vector<HICON> g_retiredIcons;

static void ClearIconCaches() {
    std::lock_guard<std::mutex> lock(g_cacheMutex);
    g_generation++;
    g_composedCache.clear();
    for (auto& entry : g_iconCache) {
        if (entry.second) {
            g_retiredIcons.push_back(entry.second);
        }
    }
    g_iconCache.clear();
    g_composeQueue.clear();
    g_composeQueued.clear();
}

// Called from hook context: never blocks on I/O. A miss queues the work for the
// worker thread and lets the original icon through this one time.
static Bitmap32Ptr LookupComposed(const std::wstring& overrideKey, int cx, int cy,
                                  bool queueOnMiss) {
    std::wstring key;
    {
        std::lock_guard<std::mutex> lock(g_cacheMutex);
        key = MakeCacheKey(overrideKey, g_generation, cx, cy);
        auto it = g_composedCache.find(key);
        if (it != g_composedCache.end()) {
            return it->second;  // may be a cached failure (null)
        }
        if (!queueOnMiss || g_composeQueued.count(key) ||
            g_composeQueue.size() >= 256) {
            return nullptr;
        }
        g_composeQueued.insert(key);
        g_composeQueue.push_back(key);
    }
    if (g_workerWindow) {
        PostMessageW(g_workerWindow, IFAB_WM_COMPOSE, 0, 0);
    }
    return nullptr;
}

// Called from hook context for WM_GETICON, whose caller does not own the handle
// it gets back, so the handle has to outlive the call.
static HICON LookupSharedIcon(const std::wstring& overrideKey, int cx, int cy) {
    Bitmap32Ptr bitmap = LookupComposed(overrideKey, cx, cy, true);
    if (!bitmap) {
        return nullptr;
    }

    std::lock_guard<std::mutex> lock(g_cacheMutex);
    std::wstring key = MakeCacheKey(overrideKey, g_generation, cx, cy);
    auto it = g_iconCache.find(key);
    if (it != g_iconCache.end()) {
        return it->second;
    }
    HICON icon = CreateIconFromBitmap(*bitmap);
    g_iconCache[key] = icon;
    return icon;
}

// ---------------------------------------------------------------------------
// Override store
// ---------------------------------------------------------------------------

static std::vector<IconOverride> g_customOverrides;  // guarded by g_overrideMutex

static std::wstring NormalizeTargetKey(const std::wstring& raw) {
    std::wstring target = ExpandEnv(Trim(raw));
    if (target.empty()) {
        return L"";
    }
    return ToLower(target);
}

static void RebuildIndexLocked() {
    g_overridesByName.clear();
    for (const auto& entry : g_overrides) {
        std::wstring name = BaseName(entry.first);
        if (!name.empty() && !g_overridesByName.count(name)) {
            g_overridesByName[name] = entry.first;
        }
    }
}

static bool HasAnyOverride() {
    std::lock_guard<std::mutex> lock(g_overrideMutex);
    return !g_overrides.empty();
}

// Looks an override up by a file path. Called from hook context.
static bool FindOverrideForPath(const std::wstring& path, IconOverride* out) {
    if (path.empty()) {
        return false;
    }
    std::wstring lowered = ToLower(path);

    {
        std::lock_guard<std::mutex> lock(g_overrideMutex);
        if (g_overrides.empty()) {
            return false;
        }
        auto it = g_overrides.find(lowered);
        if (it != g_overrides.end()) {
            *out = it->second;
            return true;
        }
        std::wstring name = BaseName(lowered);
        auto byName = g_overridesByName.find(name);
        if (byName != g_overridesByName.end()) {
            // A bare-file-name key always matches by name; a full-path key only
            // does when the user asked for name matching.
            bool keyIsBareName = byName->second.find(L'\\') == std::wstring::npos;
            if (keyIsBareName || g_settings.matchFileNameOnly) {
                *out = g_overrides[byName->second];
                return true;
            }
        }
    }

    // Shortcuts: match the .lnk itself above, its target here.
    if (Extension(lowered) == L".lnk") {
        std::wstring target;
        {
            std::lock_guard<std::mutex> lock(g_pathMutex);
            auto it = g_linkTargets.find(lowered);
            if (it != g_linkTargets.end()) {
                target = it->second;
            }
        }
        if (!target.empty()) {
            return FindOverrideForPath(target, out);
        }
    }
    return false;
}

static std::wstring SerializeOverride(const IconOverride& ov) {
    const wchar_t sep = 0x1F;
    std::wstring out = ov.display;
    out += sep;
    out += ov.iconPath;
    out += sep;
    out += FormatInt(ov.iconIndex);
    out += sep;
    out += FitName(ov.fit);
    out += sep;
    out += FormatInt(ov.scalePercent);
    out += sep;
    out += FormatInt(ov.offsetXPercent);
    out += sep;
    out += FormatInt(ov.offsetYPercent);
    return out;
}

static bool DeserializeOverride(const std::wstring& text, IconOverride* out) {
    const wchar_t sep = 0x1F;
    std::vector<std::wstring> fields;
    size_t start = 0;
    while (start <= text.size()) {
        size_t pos = text.find(sep, start);
        if (pos == std::wstring::npos) {
            fields.push_back(text.substr(start));
            break;
        }
        fields.push_back(text.substr(start, pos - start));
        start = pos + 1;
    }
    if (fields.size() < 2 || fields[0].empty() || fields[1].empty()) {
        return false;
    }

    out->display = fields[0];
    out->key = NormalizeTargetKey(fields[0]);
    out->iconPath = ExpandEnv(fields[1]);
    out->iconIndex = fields.size() > 2 ? ParseInt(fields[2], 0) : 0;
    out->fit = fields.size() > 3 ? ParseFit(fields[3]) : FitMode::Contain;
    out->scalePercent = fields.size() > 4 ? ParseInt(fields[4], 100) : 100;
    out->offsetXPercent = fields.size() > 5 ? ParseInt(fields[5], 0) : 0;
    out->offsetYPercent = fields.size() > 6 ? ParseInt(fields[6], 0) : 0;
    out->fromSettings = false;
    return !out->key.empty();
}

static void SaveCustomOverrides() {
    std::vector<IconOverride> list;
    {
        std::lock_guard<std::mutex> lock(g_overrideMutex);
        list = g_customOverrides;
    }

    int saved = 0;
    for (const IconOverride& ov : list) {
        std::wstring text = SerializeOverride(ov);
        if (text.size() > 3000) {
            Wh_Log(L"Override for %s is too long to store, skipped", ov.display.c_str());
            continue;
        }
        wchar_t name[32];
        swprintf(name, ARRAYSIZE(name), L"custom%d", saved);
        if (Wh_SetStringValue(name, text.c_str())) {
            saved++;
        }
    }

    int previous = Wh_GetIntValue(L"customCount", 0);
    for (int i = saved; i < previous; i++) {
        wchar_t name[32];
        swprintf(name, ARRAYSIZE(name), L"custom%d", i);
        Wh_DeleteValue(name);
    }
    Wh_SetIntValue(L"customCount", saved);
}

// Merges the settings-declared overrides with the ones stored by the mod and
// publishes the result. Custom overrides win, since they are the ones the user
// just clicked together.
static void PublishOverrides(const std::vector<IconOverride>& fromSettings,
                             const std::vector<IconOverride>& custom) {
    {
        std::lock_guard<std::mutex> lock(g_overrideMutex);
        g_overrides.clear();
        for (const IconOverride& ov : fromSettings) {
            g_overrides[ov.key] = ov;
        }
        for (const IconOverride& ov : custom) {
            g_overrides[ov.key] = ov;
        }
        g_customOverrides = custom;
        RebuildIndexLocked();
    }
    ClearIconCaches();
}

static std::vector<IconOverride> SnapshotOverrides() {
    std::lock_guard<std::mutex> lock(g_overrideMutex);
    std::vector<IconOverride> list;
    list.reserve(g_overrides.size());
    for (const auto& entry : g_overrides) {
        list.push_back(entry.second);
    }
    std::sort(list.begin(), list.end(),
              [](const IconOverride& a, const IconOverride& b) {
                  return ToLower(BaseName(a.display)) < ToLower(BaseName(b.display));
              });
    return list;
}

// ---------------------------------------------------------------------------
// Settings
// ---------------------------------------------------------------------------

template <typename... Args>
static std::wstring ReadStringSetting(PCWSTR format, Args... args) {
    PCWSTR value = Wh_GetStringSetting(format, args...);
    std::wstring result = value ? value : L"";
    if (value) {
        Wh_FreeStringSetting(value);
    }
    return result;
}

static Gesture ParseGesture(const std::wstring& value) {
    std::wstring v = ToLower(Trim(value));
    if (v == L"ctrl") {
        return Gesture::Ctrl;
    }
    if (v == L"alt") {
        return Gesture::Alt;
    }
    if (v == L"ctrlshift") {
        return Gesture::CtrlShift;
    }
    if (v == L"none") {
        return Gesture::None;
    }
    if (v == L"off") {
        return Gesture::Off;
    }
    return Gesture::Shift;
}

static RefreshMode ParseRefreshMode(const std::wstring& value) {
    std::wstring v = ToLower(Trim(value));
    if (v == L"none") {
        return RefreshMode::None;
    }
    if (v == L"themechange") {
        return RefreshMode::ThemeChange;
    }
    if (v == L"both") {
        return RefreshMode::Both;
    }
    return RefreshMode::WindowIcon;
}

// "C:\path\shell32.dll,3" -> path plus index. Only split when what follows the
// comma is a number, so a comma in a file name is left alone.
static void SplitIconSource(const std::wstring& raw, std::wstring* path, int* index) {
    std::wstring text = Trim(raw);
    int discarded = 0;
    if (!index) {
        index = &discarded;
    }
    *index = 0;
    size_t comma = text.find_last_of(L',');
    if (comma != std::wstring::npos && comma + 1 < text.size()) {
        std::wstring tail = Trim(text.substr(comma + 1));
        bool numeric = !tail.empty();
        for (size_t i = 0; i < tail.size() && numeric; i++) {
            if (!iswdigit(tail[i]) && !(i == 0 && tail[i] == L'-')) {
                numeric = false;
            }
        }
        if (numeric) {
            *index = ParseInt(tail, 0);
            text = Trim(text.substr(0, comma));
        }
    }
    *path = ExpandEnv(text);
}

static std::vector<IconOverride> LoadSettingsOverrides() {
    std::vector<IconOverride> list;
    for (int i = 0; i < 128; i++) {
        std::wstring target = ReadStringSetting(L"overrides[%d].target", i);
        std::wstring icon = ReadStringSetting(L"overrides[%d].icon", i);
        if (Trim(target).empty() && Trim(icon).empty()) {
            break;
        }
        if (Trim(target).empty() || Trim(icon).empty()) {
            continue;
        }

        IconOverride ov;
        ov.display = ExpandEnv(Trim(target));
        ov.key = NormalizeTargetKey(target);
        SplitIconSource(icon, &ov.iconPath, &ov.iconIndex);
        ov.fit = ParseFit(ReadStringSetting(L"overrides[%d].fit", i));
        ov.scalePercent = Wh_GetIntSetting(L"overrides[%d].scale", i);
        if (ov.scalePercent <= 0) {
            ov.scalePercent = 100;
        }
        ov.offsetXPercent = Wh_GetIntSetting(L"overrides[%d].offsetX", i);
        ov.offsetYPercent = Wh_GetIntSetting(L"overrides[%d].offsetY", i);
        ov.fromSettings = true;
        if (!ov.key.empty() && !ov.iconPath.empty()) {
            list.push_back(ov);
        }
    }
    return list;
}

static std::vector<IconOverride> LoadCustomOverrides() {
    std::vector<IconOverride> list;
    int count = Wh_GetIntValue(L"customCount", 0);
    count = std::clamp(count, 0, 512);
    for (int i = 0; i < count; i++) {
        wchar_t name[32];
        swprintf(name, ARRAYSIZE(name), L"custom%d", i);
        std::vector<wchar_t> buffer(4096, 0);
        Wh_GetStringValue(name, buffer.data(), buffer.size());
        if (buffer[0] == L'\0') {
            continue;
        }
        IconOverride ov;
        if (DeserializeOverride(buffer.data(), &ov)) {
            list.push_back(ov);
        }
    }
    return list;
}

static void LoadSettings() {
    g_settings.gesture = ParseGesture(ReadStringSetting(L"gesture"));
    g_settings.hotkey = Wh_GetIntSetting(L"hotkey") != 0;
    g_settings.injectContextMenu = Wh_GetIntSetting(L"injectContextMenu") != 0;
    g_settings.taskbarOnly = Wh_GetIntSetting(L"taskbarOnly") != 0;
    g_settings.matchFileNameOnly = Wh_GetIntSetting(L"matchFileNameOnly") != 0;
    g_settings.hookWindowIcons = Wh_GetIntSetting(L"hookWindowIcons") != 0;
    g_settings.hookShellIcons = Wh_GetIntSetting(L"hookShellIcons") != 0;
    g_settings.refresh = ParseRefreshMode(ReadStringSetting(L"refreshMode"));
    g_settings.verboseLog = Wh_GetIntSetting(L"verboseLog") != 0;
}

// ---------------------------------------------------------------------------
// Shortcut and process path resolution (worker thread)
// ---------------------------------------------------------------------------

static std::wstring ResolveShortcutTarget(const std::wstring& linkPath) {
    ComPtr<IShellLinkW> link;
    HRESULT hr = CoCreateInstance(kCLSID_ShellLink, nullptr, CLSCTX_INPROC_SERVER,
                                  kIID_IShellLinkW, (void**)&link);
    if (FAILED(hr)) {
        return L"";
    }

    IPersistFile* persist = nullptr;
    if (FAILED(link->QueryInterface(kIID_IPersistFile, (void**)&persist)) || !persist) {
        return L"";
    }
    hr = persist->Load(linkPath.c_str(), STGM_READ);
    persist->Release();
    if (FAILED(hr)) {
        return L"";
    }

    wchar_t target[MAX_PATH * 2]{};
    // SLGP_RAWPATH keeps the stored path, which is what the shell hands to the
    // icon extraction APIs we hook.
    if (FAILED(link->GetPath(target, ARRAYSIZE(target), nullptr, SLGP_RAWPATH))) {
        return L"";
    }
    return ExpandEnv(target);
}

static std::wstring PinnedTaskbarFolder() {
    wchar_t* appData = nullptr;
    std::wstring folder;
    if (SUCCEEDED(
            SHGetKnownFolderPath(kFOLDERID_RoamingAppData, 0, nullptr, &appData)) &&
        appData) {
        folder = appData;
        folder += L"\\Microsoft\\Internet Explorer\\Quick Launch\\User Pinned\\TaskBar";
        CoTaskMemFree(appData);
    }
    return folder;
}

struct AppCandidate {
    std::wstring label;  // what the user sees
    std::wstring path;   // exe or .lnk path used as the override key
};

static void EnumeratePinnedShortcuts(std::vector<AppCandidate>* out,
                                     std::unordered_map<std::wstring, std::wstring>* links) {
    std::wstring folder = PinnedTaskbarFolder();
    if (folder.empty()) {
        return;
    }
    std::wstring pattern = folder + L"\\*.lnk";
    WIN32_FIND_DATAW find{};
    HANDLE handle = FindFirstFileW(pattern.c_str(), &find);
    if (handle == INVALID_HANDLE_VALUE) {
        return;
    }
    do {
        if (find.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
            continue;
        }
        std::wstring linkPath = folder + L"\\" + find.cFileName;
        std::wstring target = ResolveShortcutTarget(linkPath);
        if (links) {
            (*links)[ToLower(linkPath)] = target;
        }
        if (out) {
            std::wstring name = find.cFileName;
            size_t dot = name.find_last_of(L'.');
            if (dot != std::wstring::npos) {
                name = name.substr(0, dot);
            }
            AppCandidate candidate;
            candidate.path = target.empty() ? linkPath : target;
            candidate.label = name + L"  (pinned)";
            out->push_back(candidate);
        }
    } while (FindNextFileW(handle, &find));
    FindClose(handle);
}

static std::wstring ProcessPathForId(DWORD processId) {
    HANDLE process = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, processId);
    if (!process) {
        return L"";
    }
    wchar_t buffer[MAX_PATH * 2]{};
    DWORD size = ARRAYSIZE(buffer);
    std::wstring result;
    if (QueryFullProcessImageNameW(process, 0, buffer, &size)) {
        result.assign(buffer, size);
    }
    CloseHandle(process);
    return result;
}

static bool IsTaskbarButtonWindow(HWND window) {
    if (!IsWindowVisible(window) || GetWindow(window, GW_OWNER)) {
        return false;
    }
    LONG_PTR exStyle = GetWindowLongPtrW(window, GWL_EXSTYLE);
    if (exStyle & WS_EX_TOOLWINDOW) {
        return false;
    }
    if (GetWindowTextLengthW(window) == 0) {
        return false;
    }
    return true;
}

struct WindowScan {
    std::vector<AppCandidate>* candidates;
    std::unordered_map<DWORD, std::wstring>* paths;
};

static BOOL CALLBACK ScanWindowsProc(HWND window, LPARAM param) {
    WindowScan* scan = (WindowScan*)param;
    if (!IsTaskbarButtonWindow(window)) {
        return TRUE;
    }

    DWORD processId = 0;
    GetWindowThreadProcessId(window, &processId);
    if (processId == 0 || processId == GetCurrentProcessId()) {
        return TRUE;
    }

    std::wstring path;
    auto known = scan->paths->find(processId);
    if (known != scan->paths->end()) {
        path = known->second;
    } else {
        path = ProcessPathForId(processId);
        (*scan->paths)[processId] = path;
    }
    if (path.empty() || !scan->candidates) {
        return TRUE;
    }

    wchar_t title[256]{};
    GetWindowTextW(window, title, ARRAYSIZE(title));

    AppCandidate candidate;
    candidate.path = path;
    candidate.label = BaseName(path) + L"  (" + title + L")";
    scan->candidates->push_back(candidate);
    return TRUE;
}

// Pinned shortcuts plus everything that currently has a taskbar button, with
// duplicates folded together.
static std::vector<AppCandidate> CollectCandidates() {
    std::vector<AppCandidate> candidates;
    std::unordered_map<std::wstring, std::wstring> links;
    EnumeratePinnedShortcuts(&candidates, &links);

    std::unordered_map<DWORD, std::wstring> paths;
    WindowScan scan{&candidates, &paths};
    EnumWindows(ScanWindowsProc, (LPARAM)&scan);

    {
        std::lock_guard<std::mutex> lock(g_pathMutex);
        for (const auto& entry : links) {
            g_linkTargets[entry.first] = entry.second;
        }
        g_processPaths = paths;
    }

    std::unordered_set<std::wstring> seen;
    std::vector<AppCandidate> unique;
    for (const AppCandidate& candidate : candidates) {
        std::wstring key = ToLower(candidate.path);
        if (key.empty() || !seen.insert(key).second) {
            continue;
        }
        unique.push_back(candidate);
    }
    std::sort(unique.begin(), unique.end(),
              [](const AppCandidate& a, const AppCandidate& b) {
                  return ToLower(a.label) < ToLower(b.label);
              });
    return unique;
}

// Resolves the executable behind a window, with a per-process cache so the
// WM_GETICON hook does not query the same process over and over.
static std::wstring PathForWindow(HWND window) {
    DWORD processId = 0;
    GetWindowThreadProcessId(window, &processId);
    if (processId == 0) {
        return L"";
    }

    {
        std::lock_guard<std::mutex> lock(g_pathMutex);
        auto it = g_processPaths.find(processId);
        if (it != g_processPaths.end()) {
            return it->second;
        }
    }

    std::wstring path = ProcessPathForId(processId);
    {
        std::lock_guard<std::mutex> lock(g_pathMutex);
        if (g_processPaths.size() > 512) {
            g_processPaths.clear();
        }
        g_processPaths[processId] = path;
    }
    return path;
}

// ---------------------------------------------------------------------------
// Refreshing the taskbar after a change
// ---------------------------------------------------------------------------

struct PushIconScan {
    const IconOverride* ov;
    HICON small;
    HICON big;
};

static BOOL CALLBACK PushIconProc(HWND window, LPARAM param) {
    PushIconScan* scan = (PushIconScan*)param;
    if (!IsTaskbarButtonWindow(window)) {
        return TRUE;
    }

    std::wstring path = PathForWindow(window);
    IconOverride found;
    if (path.empty() || !FindOverrideForPath(path, &found) ||
        found.key != scan->ov->key) {
        return TRUE;
    }

    // Icon handles are usable across processes in the same session, and the
    // shell notices WM_SETICON and redraws the button.
    if (scan->small) {
        SendNotifyMessageW(window, WM_SETICON, ICON_SMALL, (LPARAM)scan->small);
    }
    if (scan->big) {
        SendNotifyMessageW(window, WM_SETICON, ICON_BIG, (LPARAM)scan->big);
    }
    return TRUE;
}

static void RefreshForOverride(const IconOverride& ov) {
    RefreshMode mode = g_settings.refresh;
    if (mode == RefreshMode::WindowIcon || mode == RefreshMode::Both) {
        PushIconScan scan{};
        scan.ov = &ov;
        scan.small = LookupSharedIcon(ov.key, GetSystemMetrics(SM_CXSMICON),
                                      GetSystemMetrics(SM_CYSMICON));
        scan.big = LookupSharedIcon(ov.key, GetSystemMetrics(SM_CXICON),
                                    GetSystemMetrics(SM_CYICON));
        if (scan.small || scan.big) {
            EnumWindows(PushIconProc, (LPARAM)&scan);
        }
    }

    if (mode == RefreshMode::ThemeChange || mode == RefreshMode::Both) {
        // Nudges the taskbar into reloading its visuals, icons included.
        for (PCWSTR className : {L"Shell_TrayWnd", L"Shell_SecondaryTrayWnd"}) {
            HWND tray = nullptr;
            while ((tray = FindWindowExW(nullptr, tray, className, nullptr)) != nullptr) {
                SendNotifyMessageW(tray, WM_SETTINGCHANGE, 0, (LPARAM)L"ImmersiveColorSet");
            }
        }
    }
}

static void RefreshAll() {
    std::vector<IconOverride> list = SnapshotOverrides();
    for (const IconOverride& ov : list) {
        RefreshForOverride(ov);
    }
}

// ---------------------------------------------------------------------------
// Hooks
// ---------------------------------------------------------------------------

// Icon sizes pre-rendered whenever an override changes, so the hooks almost
// always find a finished bitmap waiting for them.
static const int kPrewarmSizes[] = {16, 20, 24, 28, 32, 40, 48, 56, 64, 96, 128, 256};

static UINT WINAPI PrivateExtractIconsW_Hook(LPCWSTR fileName, int iconIndex,
                                             int cxIcon, int cyIcon, HICON* icons,
                                             UINT* iconIds, UINT iconCount,
                                             UINT flags) {
    if (!g_settings.hookShellIcons || !fileName || !icons || iconCount == 0 ||
        !IconReplacementAllowedHere() || !HasAnyOverride()) {
        return PrivateExtractIconsW_Original(fileName, iconIndex, cxIcon, cyIcon,
                                             icons, iconIds, iconCount, flags);
    }

    IconOverride ov;
    if (!FindOverrideForPath(fileName, &ov)) {
        return PrivateExtractIconsW_Original(fileName, iconIndex, cxIcon, cyIcon,
                                             icons, iconIds, iconCount, flags);
    }

    // With more than one icon requested the sizes come packed two to a word.
    int sizes[2] = {LOWORD(cxIcon) ? LOWORD(cxIcon) : GetSystemMetrics(SM_CXICON),
                    HIWORD(cxIcon)};
    int heights[2] = {LOWORD(cyIcon) ? LOWORD(cyIcon) : GetSystemMetrics(SM_CYICON),
                      HIWORD(cyIcon)};
    UINT wanted = std::min<UINT>(iconCount, sizes[1] && heights[1] ? 2u : 1u);

    UINT produced = 0;
    for (UINT i = 0; i < wanted; i++) {
        Bitmap32Ptr bitmap = LookupComposed(ov.key, sizes[i], heights[i], true);
        if (!bitmap) {
            break;
        }
        HICON icon = CreateIconFromBitmap(*bitmap);
        if (!icon) {
            break;
        }
        icons[produced] = icon;
        if (iconIds) {
            iconIds[produced] = 0;
        }
        produced++;
    }

    if (produced == 0) {
        return PrivateExtractIconsW_Original(fileName, iconIndex, cxIcon, cyIcon,
                                             icons, iconIds, iconCount, flags);
    }

    VerboseLog(L"PrivateExtractIcons: replaced %s at %dpx", fileName, sizes[0]);
    return produced;
}

using SHDefExtractIconW_t = HRESULT(WINAPI*)(LPCWSTR, int, UINT, HICON*, HICON*, UINT);
static SHDefExtractIconW_t SHDefExtractIconW_Original;

static HRESULT WINAPI SHDefExtractIconW_Hook(LPCWSTR iconFile, int index, UINT flags,
                                             HICON* iconLarge, HICON* iconSmall,
                                             UINT iconSize) {
    if (!g_settings.hookShellIcons || !iconFile || !IconReplacementAllowedHere() ||
        !HasAnyOverride()) {
        return SHDefExtractIconW_Original(iconFile, index, flags, iconLarge, iconSmall,
                                          iconSize);
    }

    IconOverride ov;
    if (!FindOverrideForPath(iconFile, &ov)) {
        return SHDefExtractIconW_Original(iconFile, index, flags, iconLarge, iconSmall,
                                          iconSize);
    }

    int large = LOWORD(iconSize) ? LOWORD(iconSize) : GetSystemMetrics(SM_CXICON);
    int small = HIWORD(iconSize) ? HIWORD(iconSize) : GetSystemMetrics(SM_CXSMICON);

    HICON producedLarge = nullptr;
    HICON producedSmall = nullptr;
    if (iconLarge) {
        Bitmap32Ptr bitmap = LookupComposed(ov.key, large, large, true);
        if (bitmap) {
            producedLarge = CreateIconFromBitmap(*bitmap);
        }
    }
    if (iconSmall) {
        Bitmap32Ptr bitmap = LookupComposed(ov.key, small, small, true);
        if (bitmap) {
            producedSmall = CreateIconFromBitmap(*bitmap);
        }
    }

    bool largeOk = !iconLarge || producedLarge;
    bool smallOk = !iconSmall || producedSmall;
    if (!largeOk || !smallOk) {
        if (producedLarge) {
            DestroyIcon(producedLarge);
        }
        if (producedSmall) {
            DestroyIcon(producedSmall);
        }
        return SHDefExtractIconW_Original(iconFile, index, flags, iconLarge, iconSmall,
                                          iconSize);
    }

    if (iconLarge) {
        *iconLarge = producedLarge;
    }
    if (iconSmall) {
        *iconSmall = producedSmall;
    }
    VerboseLog(L"SHDefExtractIcon: replaced %s at %dpx", iconFile, large);
    return S_OK;
}

using SHGetFileInfoW_t = DWORD_PTR(WINAPI*)(LPCWSTR, DWORD, SHFILEINFOW*, UINT, UINT);
static SHGetFileInfoW_t SHGetFileInfoW_Original;

static DWORD_PTR WINAPI SHGetFileInfoW_Hook(LPCWSTR path, DWORD fileAttributes,
                                            SHFILEINFOW* fileInfo, UINT fileInfoSize,
                                            UINT flags) {
    DWORD_PTR result = SHGetFileInfoW_Original(path, fileAttributes, fileInfo,
                                               fileInfoSize, flags);
    if (!result || !path || !fileInfo || !(flags & SHGFI_ICON) ||
        (flags & SHGFI_SYSICONINDEX) || !g_settings.hookShellIcons ||
        !IconReplacementAllowedHere() || !HasAnyOverride()) {
        return result;
    }
    if (fileInfoSize < sizeof(SHFILEINFOW) || !fileInfo->hIcon) {
        return result;
    }

    IconOverride ov;
    if (!FindOverrideForPath(path, &ov)) {
        return result;
    }

    int size = (flags & SHGFI_SMALLICON) ? GetSystemMetrics(SM_CXSMICON)
                                         : GetSystemMetrics(SM_CXICON);
    Bitmap32Ptr bitmap = LookupComposed(ov.key, size, size, true);
    if (!bitmap) {
        return result;
    }
    HICON icon = CreateIconFromBitmap(*bitmap);
    if (!icon) {
        return result;
    }

    DestroyIcon(fileInfo->hIcon);
    fileInfo->hIcon = icon;
    VerboseLog(L"SHGetFileInfo: replaced %s at %dpx", path, size);
    return result;
}

using SendMessageTimeoutW_t = LRESULT(WINAPI*)(HWND, UINT, WPARAM, LPARAM, UINT, UINT,
                                               PDWORD_PTR);
static SendMessageTimeoutW_t SendMessageTimeoutW_Original;

// This is how the taskbar asks a running window for its icon.
static LRESULT WINAPI SendMessageTimeoutW_Hook(HWND window, UINT message,
                                              WPARAM wParam, LPARAM lParam,
                                              UINT flags, UINT timeout,
                                              PDWORD_PTR result) {
    if (message != WM_GETICON || !g_settings.hookWindowIcons ||
        !IconReplacementAllowedHere() || !HasAnyOverride()) {
        return SendMessageTimeoutW_Original(window, message, wParam, lParam, flags,
                                            timeout, result);
    }

    std::wstring path = PathForWindow(window);
    IconOverride ov;
    if (path.empty() || !FindOverrideForPath(path, &ov)) {
        return SendMessageTimeoutW_Original(window, message, wParam, lParam, flags,
                                            timeout, result);
    }

    int size = wParam == ICON_BIG ? GetSystemMetrics(SM_CXICON)
                                  : GetSystemMetrics(SM_CXSMICON);
    HICON icon = LookupSharedIcon(ov.key, size, size);
    if (!icon) {
        return SendMessageTimeoutW_Original(window, message, wParam, lParam, flags,
                                            timeout, result);
    }

    if (result) {
        *result = (DWORD_PTR)icon;
    }
    VerboseLog(L"WM_GETICON: replaced %s at %dpx", path.c_str(), size);
    return TRUE;
}

// ---------------------------------------------------------------------------
// Taskbar gesture
// ---------------------------------------------------------------------------

static bool GestureHeld() {
    bool shift = (GetKeyState(VK_SHIFT) & 0x8000) != 0;
    bool ctrl = (GetKeyState(VK_CONTROL) & 0x8000) != 0;
    bool alt = (GetKeyState(VK_MENU) & 0x8000) != 0;

    switch (g_settings.gesture) {
        case Gesture::Shift:
            return shift && !ctrl && !alt;
        case Gesture::Ctrl:
            return ctrl && !shift && !alt;
        case Gesture::Alt:
            return alt && !ctrl && !shift;
        case Gesture::CtrlShift:
            return ctrl && shift && !alt;
        case Gesture::None:
            return true;
        case Gesture::Off:
        default:
            return false;
    }
}

static std::mutex g_subclassMutex;
static std::unordered_set<HWND> g_subclassed;

static void RequestMenuAt(POINT screenPoint) {
    if (g_workerWindow) {
        PostMessageW(g_workerWindow, IFAB_WM_SHOW_MENU, (WPARAM)screenPoint.x,
                     (LPARAM)screenPoint.y);
    }
}

// Shared by the Windows 10 subclass and the Windows 11 window-procedure
// hook. Returns true when the gesture has consumed the message.
static bool GestureConsumesMessage(UINT message, WPARAM wParam) {
    if (g_unloading) {
        return false;
    }

    switch (message) {
        case WM_RBUTTONDOWN:
        case WM_NCRBUTTONDOWN:
        case WM_CONTEXTMENU:
            // Swallowed; the menu goes up on the matching up-click.
            return GestureHeld();

        case WM_RBUTTONUP:
        case WM_NCRBUTTONUP:
            if (GestureHeld()) {
                POINT point;
                GetCursorPos(&point);
                RequestMenuAt(point);
                return true;
            }
            return false;

        case WM_POINTERDOWN:
            return IFAB_POINTER_IS_RIGHT(wParam) && GestureHeld();

        case WM_POINTERUP:
            if (IFAB_POINTER_IS_RIGHT(wParam) && GestureHeld()) {
                POINT point;
                GetCursorPos(&point);
                RequestMenuAt(point);
                return true;
            }
            return false;
    }
    return false;
}

// Windows 11 routes taskbar mouse input through an InputSite window owned
// by InputHost.dll. That code checks that its GWLP_WNDPROC has not been
// changed and fails fast (0xC0000409 raised inside InputHost.dll) when it
// has, so the window must never be subclassed. Its window procedure is
// function-hooked instead, which leaves GWLP_WNDPROC alone.
static std::mutex g_inputSiteMutex;
static std::unordered_set<void*> g_hookedInputSiteProcs;
static std::atomic<bool> g_whInitialized;

using InputSiteWindowProc_t = LRESULT(CALLBACK*)(HWND, UINT, WPARAM, LPARAM);
static InputSiteWindowProc_t InputSiteWindowProc_Original;

static LRESULT CALLBACK InputSiteWindowProc_Hook(HWND window, UINT message,
                                                 WPARAM wParam, LPARAM lParam) {
    if (GestureConsumesMessage(message, wParam)) {
        return 0;
    }
    return InputSiteWindowProc_Original(window, message, wParam, lParam);
}

static void HookInputSiteWindow(HWND window) {
    if (g_unloading) {
        return;
    }
    void* windowProc = (void*)GetWindowLongPtrW(window, GWLP_WNDPROC);
    if (!windowProc) {
        return;
    }

    std::lock_guard<std::mutex> lock(g_inputSiteMutex);
    if (g_hookedInputSiteProcs.count(windowProc)) {
        return;
    }
    if (!Wh_SetFunctionHook(windowProc, (void*)InputSiteWindowProc_Hook,
                            (void**)&InputSiteWindowProc_Original)) {
        Log(L"Could not hook the InputSite window procedure");
        return;
    }
    // Hooks registered before Wh_ModInit returns are applied by Windhawk
    // itself; later ones (from the rescan timer) have to be applied here.
    if (g_whInitialized.load(std::memory_order_relaxed)) {
        Wh_ApplyHookOperations();
    }
    g_hookedInputSiteProcs.insert(windowProc);
    Log(L"Hooked the Windows 11 InputSite window procedure");
}

static LRESULT CALLBACK TaskbarSubclassProc(HWND window, UINT message, WPARAM wParam,
                                            LPARAM lParam, UINT_PTR, DWORD_PTR) {
    if (g_unloading) {
        return DefSubclassProc(window, message, wParam, lParam);
    }

    if (message == WM_NCDESTROY) {
        std::lock_guard<std::mutex> lock(g_subclassMutex);
        g_subclassed.erase(window);
    } else if (GestureConsumesMessage(message, wParam)) {
        return 0;
    }

    return DefSubclassProc(window, message, wParam, lParam);
}

// SetWindowSubclass has to run on the window's own thread, so borrow that
// thread with a short-lived WH_CALLWNDPROC hook.
static HWND g_subclassTarget;
static bool g_subclassInstall;

static LRESULT CALLBACK SubclassCallWndProc(int code, WPARAM wParam, LPARAM lParam) {
    if (code == HC_ACTION) {
        CWPSTRUCT* info = (CWPSTRUCT*)lParam;
        if (info && info->message == g_msgSubclass && info->hwnd == g_subclassTarget) {
            if (g_subclassInstall) {
                SetWindowSubclass(info->hwnd, TaskbarSubclassProc, 0, 0);
            } else {
                RemoveWindowSubclass(info->hwnd, TaskbarSubclassProc, 0);
            }
        }
    }
    return CallNextHookEx(nullptr, code, wParam, lParam);
}

static bool SetSubclassFromAnyThread(HWND window, bool install) {
    DWORD threadId = GetWindowThreadProcessId(window, nullptr);
    if (threadId == 0) {
        return false;
    }
    if (threadId == GetCurrentThreadId()) {
        return install ? SetWindowSubclass(window, TaskbarSubclassProc, 0, 0)
                       : RemoveWindowSubclass(window, TaskbarSubclassProc, 0);
    }

    g_subclassTarget = window;
    g_subclassInstall = install;
    HHOOK hook = SetWindowsHookExW(WH_CALLWNDPROC, SubclassCallWndProc, nullptr,
                                   threadId);
    if (!hook) {
        Wh_Log(L"SetWindowsHookEx failed for thread %u: %u", (unsigned)threadId,
               (unsigned)GetLastError());
        return false;
    }
    DWORD_PTR unused = 0;
    SendMessageTimeoutW(window, g_msgSubclass, 0, 0, SMTO_ABORTIFHUNG, 3000, &unused);
    UnhookWindowsHookEx(hook);
    g_subclassTarget = nullptr;
    return true;
}

static BOOL CALLBACK SubclassChildProc(HWND window, LPARAM) {
    wchar_t className[128]{};
    if (!GetClassNameW(window, className, ARRAYSIZE(className))) {
        return TRUE;
    }

    // The input surface of the Windows 11 XAML taskbar. Hook its window
    // procedure; subclassing it crashes explorer (see HookInputSiteWindow).
    if (wcscmp(className, L"Windows.UI.Input.InputSite.WindowClass") == 0) {
        HookInputSiteWindow(window);
        return TRUE;
    }

    // The Windows 10 button strip is an ordinary window, so subclassing it
    // is both safe and cleanly removable on unload.
    if (wcscmp(className, L"MSTaskListWClass") == 0) {
        std::unique_lock<std::mutex> lock(g_subclassMutex);
        if (!g_subclassed.count(window)) {
            lock.unlock();
            if (SetSubclassFromAnyThread(window, true)) {
                std::lock_guard<std::mutex> relock(g_subclassMutex);
                g_subclassed.insert(window);
            }
        }
    }
    return TRUE;
}

// Picks up the taskbar's threads and click targets. Called at init and
// periodically afterwards, because explorer recreates these windows.
// The hotkey is global, so only one process can own it. Register it from
// whichever explorer instance owns the taskbar, so the manager window opens
// in the process whose hooks and icon caches actually drive the buttons.
static bool g_hotkeyRegistered;

static void UpdateHotkeyRegistration(bool ownsTaskbar) {
    if (!g_workerWindow) {
        return;
    }
    bool wanted = g_settings.hotkey && ownsTaskbar && !g_unloading;
    if (wanted == g_hotkeyRegistered) {
        return;
    }
    if (wanted) {
        if (RegisterHotKey(g_workerWindow, IFAB_HOTKEY_ID,
                           MOD_CONTROL | MOD_ALT | MOD_SHIFT, L'I')) {
            g_hotkeyRegistered = true;
            Log(L"Manager hotkey Ctrl+Alt+Shift+I registered");
        } else {
            Log(L"Ctrl+Alt+Shift+I is taken; the manager hotkey is not available");
        }
    } else {
        UnregisterHotKey(g_workerWindow, IFAB_HOTKEY_ID);
        g_hotkeyRegistered = false;
    }
}

static void ScanTaskbar() {
    if (g_unloading) {
        return;
    }

    size_t slot = 0;
    DWORD threads[kMaxTaskbarThreads]{};
    for (PCWSTR className : {L"Shell_TrayWnd", L"Shell_SecondaryTrayWnd"}) {
        HWND tray = nullptr;
        while ((tray = FindWindowExW(nullptr, tray, className, nullptr)) != nullptr) {
            // Several explorer.exe instances can be running, and the mod is
            // loaded into all of them. Only one owns the taskbar; the others
            // must keep their hands off it, or every scan burns work and
            // SetWindowsHookEx fails with ERROR_ACCESS_DENIED on a thread
            // that belongs to someone else.
            DWORD processId = 0;
            DWORD threadId = GetWindowThreadProcessId(tray, &processId);
            if (processId != GetCurrentProcessId()) {
                continue;
            }

            bool known = false;
            for (size_t i = 0; i < slot; i++) {
                known = known || threads[i] == threadId;
            }
            if (!known && slot < kMaxTaskbarThreads) {
                threads[slot++] = threadId;
            }
            if (g_settings.gesture != Gesture::Off) {
                EnumChildWindows(tray, SubclassChildProc, 0);
            }
        }
    }

    for (size_t i = 0; i < kMaxTaskbarThreads; i++) {
        g_taskbarThreads[i].store(i < slot ? threads[i] : 0, std::memory_order_relaxed);
    }

    UpdateHotkeyRegistration(slot > 0);
}

static void UnsubclassAll() {
    std::vector<HWND> windows;
    {
        std::lock_guard<std::mutex> lock(g_subclassMutex);
        windows.assign(g_subclassed.begin(), g_subclassed.end());
        g_subclassed.clear();
    }
    for (HWND window : windows) {
        if (IsWindow(window)) {
            SetSubclassFromAnyThread(window, false);
        }
    }
}

// ---------------------------------------------------------------------------
// Classic (HMENU) taskbar context menus
// ---------------------------------------------------------------------------

static POINT g_lastMenuPoint;

using TrackPopupMenuEx_t = BOOL(WINAPI*)(HMENU, UINT, int, int, HWND, LPTPMPARAMS);
static TrackPopupMenuEx_t TrackPopupMenuEx_Original;

static bool LooksLikeTaskbarMenu(HWND owner) {
    if (!owner) {
        return false;
    }
    wchar_t className[128]{};
    GetClassNameW(owner, className, ARRAYSIZE(className));
    if (wcscmp(className, L"MSTaskListWClass") == 0 ||
        wcscmp(className, L"MSTaskSwWClass") == 0 ||
        wcscmp(className, L"Shell_TrayWnd") == 0 ||
        wcscmp(className, L"Shell_SecondaryTrayWnd") == 0 ||
        wcscmp(className, L"TaskListThumbnailWnd") == 0) {
        return true;
    }
    // A jump list is owned by a window on a taskbar thread; also require the
    // menu to be opening over a taskbar, so unrelated explorer menus on that
    // thread are left alone.
    if (!IsTaskbarThread()) {
        return false;
    }
    POINT cursor;
    if (!GetCursorPos(&cursor)) {
        return false;
    }
    HWND under = WindowFromPoint(cursor);
    if (!under) {
        return false;
    }
    HWND root = GetAncestor(under, GA_ROOT);
    wchar_t rootClass[128]{};
    GetClassNameW(root ? root : under, rootClass, ARRAYSIZE(rootClass));
    return wcscmp(rootClass, L"Shell_TrayWnd") == 0 ||
           wcscmp(rootClass, L"Shell_SecondaryTrayWnd") == 0;
}

static BOOL WINAPI TrackPopupMenuEx_Hook(HMENU menu, UINT flags, int x, int y,
                                         HWND owner, LPTPMPARAMS params) {
    if (g_unloading || !g_settings.injectContextMenu || !menu ||
        !LooksLikeTaskbarMenu(owner)) {
        return TrackPopupMenuEx_Original(menu, flags, x, y, owner, params);
    }

    g_lastMenuPoint.x = x;
    g_lastMenuPoint.y = y;

    AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(menu, MF_STRING, IFAB_IDM_CHANGE, L"Change icon (IconFabular)...");
    AppendMenuW(menu, MF_STRING, IFAB_IDM_RESET, L"Reset icon (IconFabular)");

    bool addedReturnCmd = (flags & TPM_RETURNCMD) == 0;
    BOOL result = TrackPopupMenuEx_Original(menu, flags | TPM_RETURNCMD, x, y, owner,
                                            params);

    // Clean up so repeated menus do not accumulate our entries.
    DeleteMenu(menu, IFAB_IDM_RESET, MF_BYCOMMAND);
    DeleteMenu(menu, IFAB_IDM_CHANGE, MF_BYCOMMAND);
    int count = GetMenuItemCount(menu);
    if (count > 0) {
        MENUITEMINFOW info{};
        info.cbSize = sizeof(info);
        info.fMask = MIIM_FTYPE;
        if (GetMenuItemInfoW(menu, count - 1, TRUE, &info) &&
            (info.fType & MFT_SEPARATOR)) {
            DeleteMenu(menu, count - 1, MF_BYPOSITION);
        }
    }

    int command = (int)result;
    if (command == IFAB_IDM_CHANGE || command == IFAB_IDM_RESET ||
        command == IFAB_IDM_MANAGER) {
        if (g_workerWindow) {
            PostMessageW(g_workerWindow, IFAB_WM_SHOW_MANAGER, (WPARAM)command,
                         MAKELPARAM(g_lastMenuPoint.x, g_lastMenuPoint.y));
        }
        return addedReturnCmd ? TRUE : command;
    }

    if (addedReturnCmd) {
        // The caller expected the command to arrive as WM_COMMAND, so deliver it,
        // and report the menu as shown either way.
        if (command != 0 && owner) {
            PostMessageW(owner, WM_COMMAND, (WPARAM)command, 0);
        }
        return TRUE;
    }
    return result;
}

// ---------------------------------------------------------------------------
// Working out which taskbar button was clicked (worker thread)
//
// A Windows 11 taskbar button is a XAML element, not a window, so there is
// nothing to ask for a process id. UI Automation does give us its name, which
// is enough to match against the pinned shortcuts and running apps we know
// about. The manager window always lets the user correct the guess.
// ---------------------------------------------------------------------------

static std::wstring StripRunningSuffix(const std::wstring& name) {
    // "Firefox - 3 running windows" -> "Firefox"
    static const wchar_t* kSeparators[] = {L" - ", L" \u2014 "};
    for (const wchar_t* separator : kSeparators) {
        size_t pos = name.find(separator);
        if (pos != std::wstring::npos && pos > 0) {
            return Trim(name.substr(0, pos));
        }
    }
    return Trim(name);
}

static std::wstring UiaNameAtPoint(POINT point) {
#ifdef ICONFABULAR_HAS_UIA
    ComPtr<IUIAutomation> automation;
    HRESULT hr = CoCreateInstance(kCLSID_CUIAutomation, nullptr, CLSCTX_INPROC_SERVER,
                                  kIID_IUIAutomation, (void**)&automation);
    if (FAILED(hr)) {
        return L"";
    }

    ComPtr<IUIAutomationElement> element;
    if (FAILED(automation->ElementFromPoint(point, &element)) || !element) {
        return L"";
    }

    BStr name;
    element->get_CurrentName(&name);
    std::wstring result = name.str();
    if (result.empty()) {
        BStr automationId;
        element->get_CurrentAutomationId(&automationId);
        result = automationId.str();
    }
    return result;
#else
    (void)point;
    return L"";
#endif
}

// Scores a candidate against the name UI Automation reported. 0 means no match.
static int ScoreCandidate(const AppCandidate& candidate, const std::wstring& wanted) {
    if (wanted.empty()) {
        return 0;
    }
    std::wstring needle = ToLower(wanted);
    std::wstring label = ToLower(candidate.label);
    std::wstring base = ToLower(BaseName(candidate.path));
    std::wstring stem = base;
    size_t dot = stem.find_last_of(L'.');
    if (dot != std::wstring::npos) {
        stem = stem.substr(0, dot);
    }

    if (stem == needle) {
        return 100;
    }
    if (label.rfind(needle, 0) == 0) {
        return 90;
    }
    if (needle.size() >= 3 && label.find(needle) != std::wstring::npos) {
        return 70;
    }
    if (stem.size() >= 3 && needle.find(stem) != std::wstring::npos) {
        return 60;
    }
    return 0;
}

static std::wstring IdentifyTargetAtPoint(POINT point,
                                          const std::vector<AppCandidate>& candidates) {
    std::wstring name = StripRunningSuffix(UiaNameAtPoint(point));
    if (name.empty()) {
        return L"";
    }

    const AppCandidate* best = nullptr;
    int bestScore = 0;
    for (const AppCandidate& candidate : candidates) {
        int score = ScoreCandidate(candidate, name);
        if (score > bestScore) {
            bestScore = score;
            best = &candidate;
        }
    }
    if (!best) {
        Wh_Log(L"No app matched the clicked button named '%s'", name.c_str());
        return L"";
    }
    if (g_settings.verboseLog) {
        Wh_Log(L"Clicked button '%s' matched %s", name.c_str(), best->path.c_str());
    }
    return best->path;
}

// ---------------------------------------------------------------------------
// Composition work (worker thread)
// ---------------------------------------------------------------------------

static Bitmap32Ptr LoadSourceBitmap(const IconOverride& ov, int desiredSize) {
    if (ov.iconPath.empty()) {
        return nullptr;
    }

    // One decode per source and size bucket; .ico frame choice depends on size.
    int bucket = 16;
    for (int size : {16, 32, 48, 64, 128, 256}) {
        bucket = size;
        if (desiredSize <= size) {
            break;
        }
    }

    wchar_t suffix[64];
    swprintf(suffix, ARRAYSIZE(suffix), L"|%d|%llu|%d", ov.iconIndex,
             (unsigned long long)FileStamp(ov.iconPath), bucket);
    std::wstring key = ToLower(ov.iconPath) + suffix;

    auto it = g_sourceCache.find(key);
    if (it != g_sourceCache.end()) {
        return it->second;
    }

    auto bitmap = std::make_shared<Bitmap32>();
    bool ok = IsBinaryIconSource(ov.iconPath)
                  ? DecodeBinaryIcon(ov.iconPath, ov.iconIndex, bitmap.get())
                  : DecodeImageFile(ov.iconPath, bucket, bitmap.get());
    if (ok && IsBinaryIconSource(ov.iconPath)) {
        // WIC already hands back premultiplied pixels; an icon read out of a
        // binary comes back straight, so premultiply it here.
        PremultiplyInPlace(bitmap->pixels);
    }

    Bitmap32Ptr result = ok && bitmap->valid() ? Bitmap32Ptr(bitmap) : nullptr;
    if (g_sourceCache.size() > 64) {
        g_sourceCache.clear();
    }
    g_sourceCache[key] = result;
    if (!result) {
        Wh_Log(L"Could not load icon source %s", ov.iconPath.c_str());
    }
    return result;
}

static Bitmap32Ptr ComposeForOverride(const IconOverride& ov, int cx, int cy) {
    Bitmap32Ptr source = LoadSourceBitmap(ov, std::max(cx, cy));
    if (!source) {
        return nullptr;
    }
    return ComposeBitmap(*source, cx, cy, ov);
}

static void StoreComposed(const std::wstring& cacheKey, Bitmap32Ptr bitmap) {
    std::lock_guard<std::mutex> lock(g_cacheMutex);
    g_composedCache[cacheKey] = bitmap;
    g_composeQueued.erase(cacheKey);
}

// Parses "<key>|<generation>|<cx>x<cy>" back apart.
static bool ParseCacheKey(const std::wstring& cacheKey, std::wstring* overrideKey,
                          unsigned* generation, int* cx, int* cy) {
    size_t sizeSep = cacheKey.find_last_of(L'|');
    if (sizeSep == std::wstring::npos || sizeSep == 0) {
        return false;
    }
    size_t genSep = cacheKey.find_last_of(L'|', sizeSep - 1);
    if (genSep == std::wstring::npos) {
        return false;
    }

    *overrideKey = cacheKey.substr(0, genSep);
    *generation = (unsigned)ParseInt(cacheKey.substr(genSep + 1, sizeSep - genSep - 1), 0);

    std::wstring size = cacheKey.substr(sizeSep + 1);
    size_t cross = size.find(L'x');
    if (cross == std::wstring::npos) {
        return false;
    }
    *cx = ParseInt(size.substr(0, cross), 0);
    *cy = ParseInt(size.substr(cross + 1), 0);
    return *cx > 0 && *cy > 0;
}

static void ProcessComposeQueue() {
    for (;;) {
        std::wstring cacheKey;
        {
            std::lock_guard<std::mutex> lock(g_cacheMutex);
            if (g_composeQueue.empty()) {
                return;
            }
            cacheKey = g_composeQueue.front();
            g_composeQueue.pop_front();
        }

        std::wstring overrideKey;
        unsigned generation = 0;
        int cx = 0, cy = 0;
        if (!ParseCacheKey(cacheKey, &overrideKey, &generation, &cx, &cy)) {
            continue;
        }

        IconOverride ov;
        bool found = false;
        {
            std::lock_guard<std::mutex> lock(g_overrideMutex);
            auto it = g_overrides.find(overrideKey);
            if (it != g_overrides.end()) {
                ov = it->second;
                found = true;
            }
        }

        {
            std::lock_guard<std::mutex> lock(g_cacheMutex);
            if (generation != g_generation) {
                g_composeQueued.erase(cacheKey);
                continue;  // the override changed under us
            }
        }
        if (!found) {
            StoreComposed(cacheKey, nullptr);
            continue;
        }

        StoreComposed(cacheKey, ComposeForOverride(ov, cx, cy));
    }
}

// Renders every override at the sizes the taskbar is likely to ask for, so the
// hooks hit a warm cache instead of letting the original icon through once.
static void PrewarmOverride(const IconOverride& ov) {
    unsigned generation;
    {
        std::lock_guard<std::mutex> lock(g_cacheMutex);
        generation = g_generation;
    }
    for (int size : kPrewarmSizes) {
        std::wstring cacheKey = MakeCacheKey(ov.key, generation, size, size);
        {
            std::lock_guard<std::mutex> lock(g_cacheMutex);
            if (g_composedCache.count(cacheKey)) {
                continue;
            }
        }
        StoreComposed(cacheKey, ComposeForOverride(ov, size, size));
    }
}

static void PrewarmAll() {
    for (const IconOverride& ov : SnapshotOverrides()) {
        PrewarmOverride(ov);
    }
}

// ---------------------------------------------------------------------------
// Manager window (worker thread)
// ---------------------------------------------------------------------------

#define IDC_APP_COMBO 1001
#define IDC_APP_RESCAN 1002
#define IDC_TARGET_EDIT 1003
#define IDC_TARGET_BROWSE 1004
#define IDC_ICON_EDIT 1005
#define IDC_ICON_BROWSE 1006
#define IDC_FIT_COMBO 1007
#define IDC_SCALE_EDIT 1008
#define IDC_OFFX_EDIT 1009
#define IDC_OFFY_EDIT 1010
#define IDC_PREVIEW 1011
#define IDC_APPLY 1012
#define IDC_RESET 1013
#define IDC_REFRESH 1014
#define IDC_LIST 1015
#define IDC_REMOVE 1016
#define IDC_RESTART 1017
#define IDC_CLOSE 1018

#define IDT_PREVIEW 1

static PCWSTR kManagerClass = L"IconFabularManager";
static const int kPreviewSizes[] = {16, 24, 32, 48};

struct ManagerState {
    HWND window;
    HWND appCombo;
    HWND targetEdit;
    HWND iconEdit;
    HWND fitCombo;
    HWND scaleEdit;
    HWND offsetXEdit;
    HWND offsetYEdit;
    HWND list;
    HWND preview;
    HFONT font;
    UINT dpi;
    std::vector<AppCandidate> candidates;
    std::vector<IconOverride> listed;
    HICON previewIcons[ARRAYSIZE(kPreviewSizes)];
};

static ManagerState g_mgr;

static int Dp(int value) {
    return MulDiv(value, g_mgr.dpi ? g_mgr.dpi : 96, 96);
}

static UINT DpiForWindow(HWND window) {
    using GetDpiForWindow_t = UINT(WINAPI*)(HWND);
    static GetDpiForWindow_t getDpiForWindow =
        (GetDpiForWindow_t)(void*)GetProcAddress(GetModuleHandleW(L"user32.dll"),
                                                 "GetDpiForWindow");
    if (getDpiForWindow) {
        UINT dpi = getDpiForWindow(window);
        if (dpi) {
            return dpi;
        }
    }
    HDC screen = GetDC(nullptr);
    UINT dpi = screen ? GetDeviceCaps(screen, LOGPIXELSX) : 96;
    if (screen) {
        ReleaseDC(nullptr, screen);
    }
    return dpi ? dpi : 96;
}

static std::wstring GetText(HWND control) {
    int length = GetWindowTextLengthW(control);
    if (length <= 0) {
        return L"";
    }
    std::vector<wchar_t> buffer((size_t)length + 1, 0);
    GetWindowTextW(control, buffer.data(), length + 1);
    return std::wstring(buffer.data());
}

static void ClearPreviewIcons() {
    for (HICON& icon : g_mgr.previewIcons) {
        if (icon) {
            DestroyIcon(icon);
            icon = nullptr;
        }
    }
}

// Reads the form into an override without touching the live store.
static IconOverride OverrideFromFields() {
    IconOverride ov;
    ov.display = Trim(GetText(g_mgr.targetEdit));
    ov.key = NormalizeTargetKey(ov.display);
    SplitIconSource(GetText(g_mgr.iconEdit), &ov.iconPath, &ov.iconIndex);

    int fit = (int)SendMessageW(g_mgr.fitCombo, CB_GETCURSEL, 0, 0);
    ov.fit = fit == 1 ? FitMode::Cover
             : fit == 2 ? FitMode::Stretch
             : fit == 3 ? FitMode::Center
                        : FitMode::Contain;
    ov.scalePercent = std::clamp(ParseInt(GetText(g_mgr.scaleEdit), 100), 10, 400);
    ov.offsetXPercent = std::clamp(ParseInt(GetText(g_mgr.offsetXEdit), 0), -100, 100);
    ov.offsetYPercent = std::clamp(ParseInt(GetText(g_mgr.offsetYEdit), 0), -100, 100);
    return ov;
}

static void FieldsFromOverride(const IconOverride& ov) {
    std::wstring icon = ov.iconPath;
    if (ov.iconIndex != 0) {
        icon += L"," + FormatInt(ov.iconIndex);
    }
    SetWindowTextW(g_mgr.targetEdit, ov.display.c_str());
    SetWindowTextW(g_mgr.iconEdit, icon.c_str());
    int fit = ov.fit == FitMode::Cover     ? 1
              : ov.fit == FitMode::Stretch ? 2
              : ov.fit == FitMode::Center  ? 3
                                           : 0;
    SendMessageW(g_mgr.fitCombo, CB_SETCURSEL, fit, 0);
    SetWindowTextW(g_mgr.scaleEdit, FormatInt(ov.scalePercent).c_str());
    SetWindowTextW(g_mgr.offsetXEdit, FormatInt(ov.offsetXPercent).c_str());
    SetWindowTextW(g_mgr.offsetYEdit, FormatInt(ov.offsetYPercent).c_str());
}

static void UpdatePreview() {
    ClearPreviewIcons();
    IconOverride ov = OverrideFromFields();
    if (!ov.iconPath.empty()) {
        for (size_t i = 0; i < ARRAYSIZE(kPreviewSizes); i++) {
            int size = Dp(kPreviewSizes[i]);
            Bitmap32Ptr bitmap = ComposeForOverride(ov, size, size);
            if (bitmap) {
                g_mgr.previewIcons[i] = CreateIconFromBitmap(*bitmap);
            }
        }
    }
    if (g_mgr.preview) {
        InvalidateRect(g_mgr.preview, nullptr, TRUE);
    }
}

static void DrawPreview(const DRAWITEMSTRUCT* item) {
    RECT bounds = item->rcItem;
    HDC hdc = item->hDC;

    HBRUSH background = GetSysColorBrush(COLOR_WINDOW);
    FillRect(hdc, &bounds, background);
    FrameRect(hdc, &bounds, GetSysColorBrush(COLOR_3DSHADOW));

    HBRUSH light = CreateSolidBrush(RGB(0xFF, 0xFF, 0xFF));
    HBRUSH dark = CreateSolidBrush(RGB(0xD8, 0xD8, 0xD8));

    int x = Dp(10);
    int square = Dp(8);
    for (size_t i = 0; i < ARRAYSIZE(kPreviewSizes); i++) {
        int size = Dp(kPreviewSizes[i]);
        int y = bounds.top + (bounds.bottom - bounds.top - size) / 2;

        // Transparency checkerboard, so an icon's own padding is visible.
        for (int row = 0; row * square < size; row++) {
            for (int column = 0; column * square < size; column++) {
                RECT cell{x + column * square, y + row * square,
                          std::min(x + (column + 1) * square, x + size),
                          std::min(y + (row + 1) * square, y + size)};
                FillRect(hdc, &cell, ((row + column) % 2) ? dark : light);
            }
        }
        if (g_mgr.previewIcons[i]) {
            DrawIconEx(hdc, x, y, g_mgr.previewIcons[i], size, size, 0, nullptr,
                       DI_NORMAL);
        }
        x += size + Dp(18);
    }

    DeleteObject(light);
    DeleteObject(dark);
}

static void FillAppCombo(bool rescan) {
    if (rescan || g_mgr.candidates.empty()) {
        g_mgr.candidates = CollectCandidates();
    }
    SendMessageW(g_mgr.appCombo, CB_RESETCONTENT, 0, 0);
    for (const AppCandidate& candidate : g_mgr.candidates) {
        SendMessageW(g_mgr.appCombo, CB_ADDSTRING, 0, (LPARAM)candidate.label.c_str());
    }
}

static void FillOverrideList() {
    g_mgr.listed = SnapshotOverrides();
    SendMessageW(g_mgr.list, LB_RESETCONTENT, 0, 0);
    for (const IconOverride& ov : g_mgr.listed) {
        std::wstring line = BaseName(ov.display);
        line += L"  ->  ";
        line += BaseName(ov.iconPath);
        line += L"   [";
        line += FitName(ov.fit);
        line += L" ";
        line += FormatInt(ov.scalePercent);
        line += L"%]";
        if (ov.fromSettings) {
            line += L"  (from settings)";
        }
        SendMessageW(g_mgr.list, LB_ADDSTRING, 0, (LPARAM)line.c_str());
    }
}

static void SelectAppCandidate(int index) {
    if (index < 0 || index >= (int)g_mgr.candidates.size()) {
        return;
    }
    const AppCandidate& candidate = g_mgr.candidates[index];
    SetWindowTextW(g_mgr.targetEdit, candidate.path.c_str());

    // Pre-fill the form if this app already has an override.
    IconOverride existing;
    if (FindOverrideForPath(candidate.path, &existing)) {
        FieldsFromOverride(existing);
        SetWindowTextW(g_mgr.targetEdit, candidate.path.c_str());
    }
    UpdatePreview();
}

static bool BrowseForFile(HWND owner, PCWSTR filter, PCWSTR title,
                          std::wstring* result) {
    wchar_t path[MAX_PATH * 2]{};
    if (!result->empty() && result->size() < ARRAYSIZE(path) - 1) {
        wcscpy(path, result->c_str());
    }

    OPENFILENAMEW ofn{};
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = owner;
    ofn.lpstrFilter = filter;
    ofn.lpstrFile = path;
    ofn.nMaxFile = ARRAYSIZE(path);
    ofn.lpstrTitle = title;
    ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_EXPLORER |
                OFN_NOCHANGEDIR | OFN_DONTADDTORECENT;
    if (!GetOpenFileNameW(&ofn)) {
        return false;
    }
    *result = path;
    return true;
}

static void PublishAndPersist(std::vector<IconOverride> custom) {
    PublishOverrides(LoadSettingsOverrides(), custom);
    SaveCustomOverrides();
    PrewarmAll();
}

static void ApplyFromFields() {
    IconOverride ov = OverrideFromFields();
    if (ov.key.empty()) {
        MessageBoxW(g_mgr.window,
                    L"Pick a taskbar item first, or type the path of an .exe or .lnk.",
                    L"IconFabular", MB_ICONINFORMATION | MB_OK);
        return;
    }
    if (ov.iconPath.empty()) {
        MessageBoxW(g_mgr.window, L"Choose an icon file.", L"IconFabular",
                    MB_ICONINFORMATION | MB_OK);
        return;
    }
    if (!ComposeForOverride(ov, 32, 32)) {
        std::wstring message = L"That file could not be read as an image or icon:\n\n";
        message += ov.iconPath;
        MessageBoxW(g_mgr.window, message.c_str(), L"IconFabular",
                    MB_ICONERROR | MB_OK);
        return;
    }

    std::vector<IconOverride> custom;
    {
        std::lock_guard<std::mutex> lock(g_overrideMutex);
        custom = g_customOverrides;
    }
    custom.erase(std::remove_if(custom.begin(), custom.end(),
                                [&ov](const IconOverride& existing) {
                                    return existing.key == ov.key;
                                }),
                 custom.end());
    custom.push_back(ov);

    PublishAndPersist(std::move(custom));
    RefreshForOverride(ov);
    FillOverrideList();
    UpdatePreview();
}

static void RemoveOverrideForKey(const std::wstring& key) {
    if (key.empty()) {
        return;
    }

    bool wasFromSettings = false;
    {
        std::lock_guard<std::mutex> lock(g_overrideMutex);
        auto it = g_overrides.find(key);
        wasFromSettings = it != g_overrides.end() && it->second.fromSettings;
    }
    if (wasFromSettings) {
        MessageBoxW(g_mgr.window,
                    L"This override comes from the mod's settings page. Remove it "
                    L"there to turn it off.",
                    L"IconFabular", MB_ICONINFORMATION | MB_OK);
        return;
    }

    std::vector<IconOverride> custom;
    {
        std::lock_guard<std::mutex> lock(g_overrideMutex);
        custom = g_customOverrides;
    }
    size_t before = custom.size();
    custom.erase(std::remove_if(custom.begin(), custom.end(),
                                [&key](const IconOverride& existing) {
                                    return existing.key == key;
                                }),
                 custom.end());
    if (custom.size() == before) {
        return;
    }

    PublishAndPersist(std::move(custom));

    // Pushing the old icon back is not ours to do, so ask the taskbar to reload.
    for (PCWSTR className : {L"Shell_TrayWnd", L"Shell_SecondaryTrayWnd"}) {
        HWND tray = nullptr;
        while ((tray = FindWindowExW(nullptr, tray, className, nullptr)) != nullptr) {
            SendNotifyMessageW(tray, WM_SETTINGCHANGE, 0, (LPARAM)L"ImmersiveColorSet");
        }
    }
    if (g_mgr.window) {
        FillOverrideList();
    }
}

static void RestartExplorer(HWND owner) {
    int answer = MessageBoxW(
        owner,
        L"Restart Explorer now?\n\nThis is the surest way to make pinned icons "
        L"reload. Your taskbar and any open File Explorer windows will close and "
        L"come back.",
        L"IconFabular", MB_ICONWARNING | MB_YESNO | MB_DEFBUTTON2);
    if (answer != IDYES) {
        return;
    }

    // Runs detached, so explorer is started again after this process is gone.
    wchar_t command[] =
        L"cmd.exe /c taskkill /f /im explorer.exe & start \"\" explorer.exe";
    STARTUPINFOW startup{};
    startup.cb = sizeof(startup);
    startup.dwFlags = STARTF_USESHOWWINDOW;
    startup.wShowWindow = SW_HIDE;
    PROCESS_INFORMATION process{};
    if (CreateProcessW(nullptr, command, nullptr, nullptr, FALSE,
                       CREATE_NO_WINDOW | DETACHED_PROCESS, nullptr, nullptr,
                       &startup, &process)) {
        CloseHandle(process.hThread);
        CloseHandle(process.hProcess);
    }
}

static HWND AddControl(PCWSTR className, PCWSTR text, DWORD style, int x, int y,
                       int width, int height, int id) {
    HWND control = CreateWindowExW(0, className, text, WS_CHILD | WS_VISIBLE | style,
                                   Dp(x), Dp(y), Dp(width), Dp(height), g_mgr.window,
                                   (HMENU)(INT_PTR)id, g_hinst, nullptr);
    if (control && g_mgr.font) {
        SendMessageW(control, WM_SETFONT, (WPARAM)g_mgr.font, TRUE);
    }
    return control;
}

static void BuildManagerControls() {
    const int labelX = 12, labelW = 112;
    const int fieldX = 128, fieldW = 372;
    const int buttonX = 508, buttonW = 120;
    const int rowH = 26;

    int y = 12;
    AddControl(L"Static", L"Taskbar item:", 0, labelX, y + 4, labelW, 20, 0);
    g_mgr.appCombo = AddControl(L"ComboBox", nullptr,
                                CBS_DROPDOWNLIST | WS_VSCROLL | WS_TABSTOP, fieldX, y,
                                fieldW, 240, IDC_APP_COMBO);
    AddControl(L"Button", L"Rescan", WS_TABSTOP, buttonX, y, buttonW, rowH,
               IDC_APP_RESCAN);

    y += 34;
    AddControl(L"Static", L"Target path:", 0, labelX, y + 4, labelW, 20, 0);
    g_mgr.targetEdit = AddControl(L"Edit", nullptr,
                                  WS_BORDER | WS_TABSTOP | ES_AUTOHSCROLL, fieldX, y,
                                  fieldW, rowH, IDC_TARGET_EDIT);
    AddControl(L"Button", L"Browse app...", WS_TABSTOP, buttonX, y, buttonW, rowH,
               IDC_TARGET_BROWSE);

    y += 34;
    AddControl(L"Static", L"Icon file:", 0, labelX, y + 4, labelW, 20, 0);
    g_mgr.iconEdit = AddControl(L"Edit", nullptr,
                                WS_BORDER | WS_TABSTOP | ES_AUTOHSCROLL, fieldX, y,
                                fieldW, rowH, IDC_ICON_EDIT);
    AddControl(L"Button", L"Browse...", WS_TABSTOP, buttonX, y, buttonW, rowH,
               IDC_ICON_BROWSE);

    y += 34;
    AddControl(L"Static", L"Fit:", 0, labelX, y + 4, labelW, 20, 0);
    g_mgr.fitCombo = AddControl(L"ComboBox", nullptr,
                                CBS_DROPDOWNLIST | WS_TABSTOP, fieldX, y, 150, 120,
                                IDC_FIT_COMBO);
    AddControl(L"Static", L"Scale %:", 0, 292, y + 4, 60, 20, 0);
    g_mgr.scaleEdit = AddControl(L"Edit", L"100",
                                 WS_BORDER | WS_TABSTOP | ES_NUMBER, 354, y, 52, rowH,
                                 IDC_SCALE_EDIT);
    AddControl(L"Static", L"Nudge X:", 0, 418, y + 4, 58, 20, 0);
    g_mgr.offsetXEdit = AddControl(L"Edit", L"0", WS_BORDER | WS_TABSTOP, 478, y, 44,
                                   rowH, IDC_OFFX_EDIT);
    AddControl(L"Static", L"Y:", 0, 532, y + 4, 16, 20, 0);
    g_mgr.offsetYEdit = AddControl(L"Edit", L"0", WS_BORDER | WS_TABSTOP, 552, y, 44,
                                   rowH, IDC_OFFY_EDIT);

    y += 36;
    AddControl(L"Static", L"Preview:", 0, labelX, y + 26, labelW, 20, 0);
    g_mgr.preview = AddControl(L"Static", nullptr, SS_OWNERDRAW, fieldX, y, 468, 72,
                               IDC_PREVIEW);

    y += 84;
    AddControl(L"Button", L"Apply", WS_TABSTOP | BS_DEFPUSHBUTTON, fieldX, y, 110, 28,
               IDC_APPLY);
    AddControl(L"Button", L"Reset icon", WS_TABSTOP, fieldX + 118, y, 110, 28,
               IDC_RESET);
    AddControl(L"Button", L"Refresh taskbar", WS_TABSTOP, fieldX + 236, y, 130, 28,
               IDC_REFRESH);

    y += 40;
    AddControl(L"Static", L"Overrides in effect:", 0, labelX, y, 200, 20, 0);

    y += 22;
    g_mgr.list = AddControl(L"ListBox", nullptr,
                            WS_BORDER | WS_VSCROLL | WS_TABSTOP | LBS_NOTIFY, labelX, y,
                            616, 150, IDC_LIST);

    y += 160;
    AddControl(L"Button", L"Remove selected", WS_TABSTOP, labelX, y, 140, 28,
               IDC_REMOVE);
    AddControl(L"Button", L"Restart Explorer", WS_TABSTOP, labelX + 148, y, 140, 28,
               IDC_RESTART);
    AddControl(L"Button", L"Close", WS_TABSTOP, 508, y, 120, 28, IDC_CLOSE);

    for (PCWSTR fit : {L"Contain - fit inside", L"Cover - fill and crop",
                       L"Stretch - fill exactly", L"Center - no scaling"}) {
        SendMessageW(g_mgr.fitCombo, CB_ADDSTRING, 0, (LPARAM)fit);
    }
    SendMessageW(g_mgr.fitCombo, CB_SETCURSEL, 0, 0);
}

static LRESULT CALLBACK ManagerWndProc(HWND window, UINT message, WPARAM wParam,
                                       LPARAM lParam) {
    switch (message) {
        case WM_DRAWITEM: {
            const DRAWITEMSTRUCT* item = (const DRAWITEMSTRUCT*)lParam;
            if (item && item->CtlID == IDC_PREVIEW) {
                DrawPreview(item);
                return TRUE;
            }
            break;
        }

        case WM_TIMER:
            if (wParam == IDT_PREVIEW) {
                KillTimer(window, IDT_PREVIEW);
                UpdatePreview();
                return 0;
            }
            break;

        case WM_COMMAND: {
            int id = LOWORD(wParam);
            int code = HIWORD(wParam);

            if (id == IDC_APP_COMBO && code == CBN_SELCHANGE) {
                SelectAppCandidate((int)SendMessageW(g_mgr.appCombo, CB_GETCURSEL, 0, 0));
                return 0;
            }
            if ((id == IDC_ICON_EDIT || id == IDC_SCALE_EDIT || id == IDC_OFFX_EDIT ||
                 id == IDC_OFFY_EDIT) &&
                code == EN_CHANGE) {
                SetTimer(window, IDT_PREVIEW, 350, nullptr);  // debounce typing
                return 0;
            }
            if (id == IDC_FIT_COMBO && code == CBN_SELCHANGE) {
                UpdatePreview();
                return 0;
            }
            if (id == IDC_LIST && code == LBN_SELCHANGE) {
                int index = (int)SendMessageW(g_mgr.list, LB_GETCURSEL, 0, 0);
                if (index >= 0 && index < (int)g_mgr.listed.size()) {
                    FieldsFromOverride(g_mgr.listed[index]);
                    UpdatePreview();
                }
                return 0;
            }

            if (code != BN_CLICKED && code != 0) {
                break;
            }

            switch (id) {
                case IDC_APP_RESCAN:
                    FillAppCombo(true);
                    FillOverrideList();
                    return 0;

                case IDC_TARGET_BROWSE: {
                    std::wstring path = Trim(GetText(g_mgr.targetEdit));
                    if (BrowseForFile(window,
                                      L"Programs and shortcuts\0*.exe;*.lnk\0"
                                      L"All files\0*.*\0",
                                      L"Pick the app whose icon to replace", &path)) {
                        SetWindowTextW(g_mgr.targetEdit, path.c_str());
                        IconOverride existing;
                        if (FindOverrideForPath(path, &existing)) {
                            FieldsFromOverride(existing);
                            SetWindowTextW(g_mgr.targetEdit, path.c_str());
                        }
                        UpdatePreview();
                    }
                    return 0;
                }

                case IDC_ICON_BROWSE: {
                    std::wstring path;
                    SplitIconSource(GetText(g_mgr.iconEdit), &path, nullptr);
                    if (BrowseForFile(
                            window,
                            L"Images and icons\0*.ico;*.png;*.bmp;*.jpg;*.jpeg;*.gif;"
                            L"*.tif;*.tiff;*.webp\0"
                            L"Files containing icons\0*.exe;*.dll;*.icl\0"
                            L"All files\0*.*\0",
                            L"Pick the new icon", &path)) {
                        SetWindowTextW(g_mgr.iconEdit, path.c_str());
                        UpdatePreview();
                    }
                    return 0;
                }

                case IDC_APPLY:
                    ApplyFromFields();
                    return 0;

                case IDC_RESET:
                    RemoveOverrideForKey(NormalizeTargetKey(GetText(g_mgr.targetEdit)));
                    return 0;

                case IDC_REFRESH:
                    g_sourceCache.clear();  // pick up edited icon files
                    ClearIconCaches();
                    PrewarmAll();
                    RefreshAll();
                    return 0;

                case IDC_REMOVE: {
                    int index = (int)SendMessageW(g_mgr.list, LB_GETCURSEL, 0, 0);
                    if (index >= 0 && index < (int)g_mgr.listed.size()) {
                        RemoveOverrideForKey(g_mgr.listed[index].key);
                    }
                    return 0;
                }

                case IDC_RESTART:
                    RestartExplorer(window);
                    return 0;

                case IDC_CLOSE:
                case IDCANCEL:
                    DestroyWindow(window);
                    return 0;
            }
            break;
        }

        case WM_CLOSE:
            DestroyWindow(window);
            return 0;

        case WM_DESTROY:
            KillTimer(window, IDT_PREVIEW);
            ClearPreviewIcons();
            if (g_mgr.font) {
                DeleteObject(g_mgr.font);
            }
            g_mgr = ManagerState{};
            g_managerWindow = nullptr;
            return 0;
    }
    return DefWindowProcW(window, message, wParam, lParam);
}

static HFONT CreateUiFont(UINT dpi) {
    NONCLIENTMETRICSW metrics{};
    metrics.cbSize = sizeof(metrics);
    if (!SystemParametersInfoW(SPI_GETNONCLIENTMETRICS, sizeof(metrics), &metrics, 0)) {
        return nullptr;
    }
    LOGFONTW font = metrics.lfMessageFont;
    font.lfHeight = MulDiv(font.lfHeight, dpi, 96);
    return CreateFontIndirectW(&font);
}

static void ShowManager(const std::wstring& presetTarget) {
    if (g_managerWindow && IsWindow(g_managerWindow)) {
        if (!presetTarget.empty()) {
            SetWindowTextW(g_mgr.targetEdit, presetTarget.c_str());
            IconOverride existing;
            if (FindOverrideForPath(presetTarget, &existing)) {
                FieldsFromOverride(existing);
                SetWindowTextW(g_mgr.targetEdit, presetTarget.c_str());
            }
            UpdatePreview();
        }
        ShowWindow(g_managerWindow, SW_SHOW);
        SetForegroundWindow(g_managerWindow);
        return;
    }

    g_mgr = ManagerState{};
    g_mgr.dpi = DpiForWindow(GetDesktopWindow());
    g_mgr.font = CreateUiFont(g_mgr.dpi);

    RECT frame{0, 0, Dp(640), Dp(500)};
    AdjustWindowRectEx(&frame, WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU, FALSE, 0);
    int width = frame.right - frame.left;
    int height = frame.bottom - frame.top;
    int x = (GetSystemMetrics(SM_CXSCREEN) - width) / 2;
    int y = (GetSystemMetrics(SM_CYSCREEN) - height) / 2;

    g_mgr.window = CreateWindowExW(
        WS_EX_APPWINDOW, kManagerClass, L"IconFabular - taskbar icons",
        WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX, x, y, width, height,
        nullptr, nullptr, g_hinst, nullptr);
    if (!g_mgr.window) {
        Wh_Log(L"Could not create the manager window: %u", (unsigned)GetLastError());
        return;
    }
    g_managerWindow = g_mgr.window;

    BuildManagerControls();
    FillAppCombo(true);
    FillOverrideList();

    if (!presetTarget.empty()) {
        SetWindowTextW(g_mgr.targetEdit, presetTarget.c_str());
        IconOverride existing;
        if (FindOverrideForPath(presetTarget, &existing)) {
            FieldsFromOverride(existing);
            SetWindowTextW(g_mgr.targetEdit, presetTarget.c_str());
        }
    }

    UpdatePreview();
    ShowWindow(g_mgr.window, SW_SHOW);
    SetForegroundWindow(g_mgr.window);
}

// ---------------------------------------------------------------------------
// The gesture menu (worker thread)
// ---------------------------------------------------------------------------

static void ShowGestureMenu(POINT point) {
    std::vector<AppCandidate> candidates = CollectCandidates();
    std::wstring target = IdentifyTargetAtPoint(point, candidates);

    bool hasOverride = false;
    if (!target.empty()) {
        IconOverride existing;
        hasOverride = FindOverrideForPath(target, &existing);
    }

    HMENU menu = CreatePopupMenu();
    if (!menu) {
        return;
    }

    std::wstring header =
        target.empty() ? L"IconFabular" : L"IconFabular: " + BaseName(target);
    AppendMenuW(menu, MF_STRING | MF_DISABLED, 0, header.c_str());
    AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(menu, MF_STRING, IFAB_IDM_CHANGE, L"Change icon...");
    AppendMenuW(menu, MF_STRING | (hasOverride ? 0 : MF_GRAYED), IFAB_IDM_RESET,
                L"Reset icon");
    AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(menu, MF_STRING, IFAB_IDM_MANAGER, L"All taskbar icons...");

    SetForegroundWindow(g_workerWindow);
    int command = (int)TrackPopupMenuEx(menu,
                                        TPM_RETURNCMD | TPM_RIGHTBUTTON | TPM_NONOTIFY,
                                        point.x, point.y, g_workerWindow, nullptr);
    DestroyMenu(menu);

    switch (command) {
        case IFAB_IDM_CHANGE:
            ShowManager(target);
            break;
        case IFAB_IDM_RESET:
            if (!target.empty()) {
                RemoveOverrideForKey(NormalizeTargetKey(target));
            }
            break;
        case IFAB_IDM_MANAGER:
            ShowManager(L"");
            break;
    }
}

// ---------------------------------------------------------------------------
// Worker thread
//
// Everything that touches COM, the file system or UI lives here, so the
// taskbar's own threads only ever read a finished bitmap out of the cache.
// ---------------------------------------------------------------------------

static PCWSTR kWorkerClass = L"IconFabularWorker";
static HANDLE g_workerReady;

#define IDT_RESCAN 1

static void ReloadEverything() {
    LoadSettings();
    PublishOverrides(LoadSettingsOverrides(), LoadCustomOverrides());
    if (g_settings.gesture == Gesture::Off) {
        UnsubclassAll();
    }
    ScanTaskbar();
    PrewarmAll();
    RefreshAll();
    if (g_managerWindow && IsWindow(g_managerWindow)) {
        FillAppCombo(true);
        FillOverrideList();
    }
}

static LRESULT CALLBACK WorkerWndProc(HWND window, UINT message, WPARAM wParam,
                                      LPARAM lParam) {
    switch (message) {
        case IFAB_WM_SHOW_MENU: {
            POINT point{(int)(LONG)wParam, (int)(LONG)lParam};
            ShowGestureMenu(point);
            return 0;
        }

        case IFAB_WM_SHOW_MANAGER: {
            // Came from a classic context menu: the command plus where it opened.
            POINT point{GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)};
            std::vector<AppCandidate> candidates = CollectCandidates();
            std::wstring target = IdentifyTargetAtPoint(point, candidates);
            if (wParam == IFAB_IDM_RESET) {
                if (!target.empty()) {
                    RemoveOverrideForKey(NormalizeTargetKey(target));
                } else {
                    ShowManager(L"");
                }
            } else {
                ShowManager(target);
            }
            return 0;
        }

        case IFAB_WM_COMPOSE:
            ProcessComposeQueue();
            return 0;

        case IFAB_WM_RESCAN:
            if (wParam) {
                ReloadEverything();
            } else {
                ScanTaskbar();
            }
            return 0;

        case IFAB_WM_REFRESH:
            PrewarmAll();
            RefreshAll();
            return 0;

        case WM_TIMER:
            if (wParam == IDT_RESCAN) {
                // The taskbar's windows come and go, so keep looking for them.
                ScanTaskbar();
                return 0;
            }
            break;

        case WM_HOTKEY:
            if (wParam == IFAB_HOTKEY_ID) {
                ShowManager(L"");
                return 0;
            }
            break;
    }
    return DefWindowProcW(window, message, wParam, lParam);
}

static DWORD WINAPI WorkerThreadProc(LPVOID) {
    CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);

    WNDCLASSEXW workerClass{};
    workerClass.cbSize = sizeof(workerClass);
    workerClass.lpfnWndProc = WorkerWndProc;
    workerClass.hInstance = g_hinst;
    workerClass.lpszClassName = kWorkerClass;
    RegisterClassExW(&workerClass);

    WNDCLASSEXW managerClass{};
    managerClass.cbSize = sizeof(managerClass);
    managerClass.lpfnWndProc = ManagerWndProc;
    managerClass.hInstance = g_hinst;
    managerClass.lpszClassName = kManagerClass;
    managerClass.hbrBackground = (HBRUSH)(COLOR_3DFACE + 1);
    managerClass.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    RegisterClassExW(&managerClass);

    // A real (if invisible) top-level window, because a popup menu needs an
    // owner that can take the foreground.
    g_workerWindow =
        CreateWindowExW(WS_EX_TOOLWINDOW, kWorkerClass, L"IconFabular", WS_POPUP, 0, 0,
                        0, 0, nullptr, nullptr, g_hinst, nullptr);
    if (!g_workerWindow) {
        Wh_Log(L"Could not create the worker window: %u", (unsigned)GetLastError());
        SetEvent(g_workerReady);
        CoUninitialize();
        return 1;
    }

    // The hotkey is registered by ScanTaskbar below, once this process is
    // known to own the taskbar.
    SetTimer(g_workerWindow, IDT_RESCAN, 3000, nullptr);

    // Publishing the overrides is quick; rendering them is not, so let
    // Wh_ModInit continue and do the rendering from the message loop.
    LoadSettings();
    PublishOverrides(LoadSettingsOverrides(), LoadCustomOverrides());
    ScanTaskbar();
    SetEvent(g_workerReady);
    PostMessageW(g_workerWindow, IFAB_WM_REFRESH, 0, 0);

    MSG message;
    while (GetMessageW(&message, nullptr, 0, 0) > 0) {
        TranslateMessage(&message);
        DispatchMessageW(&message);
    }

    if (g_managerWindow && IsWindow(g_managerWindow)) {
        DestroyWindow(g_managerWindow);
    }
    if (g_workerWindow) {
        KillTimer(g_workerWindow, IDT_RESCAN);
        if (g_hotkeyRegistered) {
            UnregisterHotKey(g_workerWindow, IFAB_HOTKEY_ID);
            g_hotkeyRegistered = false;
        }
        DestroyWindow(g_workerWindow);
        g_workerWindow = nullptr;
    }
    UnregisterClassW(kManagerClass, g_hinst);
    UnregisterClassW(kWorkerClass, g_hinst);

    g_sourceCache.clear();
    if (g_wicFactory) {
        g_wicFactory->Release();
        g_wicFactory = nullptr;
    }
    CoUninitialize();
    return 0;
}

// ---------------------------------------------------------------------------
// Hook installation
// ---------------------------------------------------------------------------

static bool HookExport(PCWSTR moduleName, PCSTR functionName, void* hook,
                       void** original) {
    HMODULE module = GetModuleHandleW(moduleName);
    if (!module) {
        module = LoadLibraryW(moduleName);
    }
    if (!module) {
        Wh_Log(L"%s is not loaded", moduleName);
        return false;
    }
    void* target = (void*)GetProcAddress(module, functionName);
    if (!target) {
        Wh_Log(L"%S was not found in %s", functionName, moduleName);
        return false;
    }
    if (!Wh_SetFunctionHook(target, hook, original)) {
        Wh_Log(L"Could not hook %S", functionName);
        return false;
    }
    return true;
}

BOOL Wh_ModInit() {
    Log(L"IconFabular init");

    GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
                           GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                       (PCWSTR)&Wh_ModInit, (HMODULE*)&g_hinst);
    if (!g_hinst) {
        g_hinst = (HINSTANCE)GetModuleHandleW(nullptr);
    }

    g_msgSubclass = RegisterWindowMessageW(L"IconFabular_Subclass");
    LoadSettings();

    INITCOMMONCONTROLSEX controls{};
    controls.dwSize = sizeof(controls);
    controls.dwICC = ICC_STANDARD_CLASSES | ICC_LISTVIEW_CLASSES;
    InitCommonControlsEx(&controls);

    // PrivateExtractIconsW doubles as our way of reading icons out of a binary,
    // so it is hooked even when shell icon replacement is off.
    if (!HookExport(L"user32.dll", "PrivateExtractIconsW",
                    (void*)PrivateExtractIconsW_Hook,
                    (void**)&PrivateExtractIconsW_Original)) {
        return FALSE;
    }
    HookExport(L"shell32.dll", "SHDefExtractIconW", (void*)SHDefExtractIconW_Hook,
               (void**)&SHDefExtractIconW_Original);
    HookExport(L"shell32.dll", "SHGetFileInfoW", (void*)SHGetFileInfoW_Hook,
               (void**)&SHGetFileInfoW_Original);
    HookExport(L"user32.dll", "SendMessageTimeoutW", (void*)SendMessageTimeoutW_Hook,
               (void**)&SendMessageTimeoutW_Original);
    HookExport(L"user32.dll", "TrackPopupMenuEx", (void*)TrackPopupMenuEx_Hook,
               (void**)&TrackPopupMenuEx_Original);

    g_workerReady = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    g_workerThread = CreateThread(nullptr, 0, WorkerThreadProc, nullptr, 0,
                                  &g_workerThreadId);
    if (!g_workerThread) {
        Log(L"Could not start the worker thread");
        return FALSE;
    }
    if (g_workerReady) {
        WaitForSingleObject(g_workerReady, 10000);
    }

    // From here on, hooks added by the rescan timer must be applied here.
    g_whInitialized.store(true, std::memory_order_relaxed);
    return TRUE;
}

void Wh_ModSettingsChanged() {
    Log(L"Settings changed");
    LoadSettings();
    if (g_workerWindow) {
        PostMessageW(g_workerWindow, IFAB_WM_RESCAN, 1, 0);
    }
}

void Wh_ModUninit() {
    Log(L"IconFabular uninit");
    g_unloading = true;

    UnsubclassAll();

    if (g_workerThread) {
        if (g_managerWindow) {
            PostMessageW(g_managerWindow, WM_CLOSE, 0, 0);
        }
        if (g_workerThreadId) {
            PostThreadMessageW(g_workerThreadId, WM_QUIT, 0, 0);
        }
        // The worker may be sitting in a modal loop (a file dialog, a message
        // box). Unloading the module out from under it would take explorer with
        // us, so wait it out rather than giving up after a few seconds.
        for (int waited = 0; waited < 60; waited++) {
            if (WaitForSingleObject(g_workerThread, 500) != WAIT_TIMEOUT) {
                break;
            }
            if (waited == 10) {
                Log(L"Waiting for the worker thread to finish (a dialog is open?)");
            }
        }
        CloseHandle(g_workerThread);
        g_workerThread = nullptr;
    }
    if (g_workerReady) {
        CloseHandle(g_workerReady);
        g_workerReady = nullptr;
    }

    ClearIconCaches();
    {
        std::lock_guard<std::mutex> lock(g_overrideMutex);
        g_overrides.clear();
        g_overridesByName.clear();
        g_customOverrides.clear();
    }
}

#include "about.hpp"

#include <windows.h>
#include <gdiplus.h>

#include <cstring>
#include <string>

#include "build_info.hpp"
#include "dn/version.hpp"
#include "resources.h"

namespace {

constexpr wchar_t kClass[] = L"DNAboutWnd";
constexpr wchar_t kImageClass[] = L"DNAboutImageWnd";

// Control IDs.
enum : int {
    kIdImage = 100,
    kIdOk,
};

// Layout (client-area coordinates).
constexpr int kClientW = 340;
constexpr int kClientH = 344;
constexpr int kMargin = 16;
constexpr int kImageSize = 128;
constexpr int kImageX = (kClientW - kImageSize) / 2;  // centered
constexpr int kImageY = 16;

struct AboutState {
    HWND hwnd = nullptr;               // the dialog window (modal-loop condition)
    Gdiplus::Bitmap* image = nullptr;   // loaded from the embedded PNG (owned)
    HFONT normal_font = nullptr;        // system message font, normal weight
    HFONT bold_font = nullptr;          // system message font, bold weight
};

// Convert an ASCII narrow string (the version, the build date/time) to wide.
std::wstring to_wide(const char* s) {
    const int n = MultiByteToWideChar(CP_ACP, 0, s, -1, nullptr, 0);
    if (n <= 0) return {};
    // n counts the null terminator; size the string WITHOUT it. A trailing null
    // in the returned string would become an embedded null when concatenated
    // (e.g. date + " " + time), truncating the text at the first null.
    std::wstring out(static_cast<std::size_t>(n) - 1, L'\0');
    MultiByteToWideChar(CP_ACP, 0, s, -1, out.data(), n);
    return out;
}

// Load the embedded About image (RCDATA PNG) into a GDI+ bitmap. GDI+ is
// initialized by App::Run before the message loop starts.
//
// Gdiplus::Bitmap::FromResource is NOT used: it fails on this PNG (returns a
// bitmap with Status::GenericFailure and 0x0 dimensions) even though the
// resource bytes are valid and byte-identical to the file on disk (verified
// with a standalone probe). The reliable path is to read the resource bytes
// and decode them through an IStream with Image::FromStream.
Gdiplus::Bitmap* load_about_image(HINSTANCE hInstance) {
    const HRSRC res =
        FindResourceW(hInstance, MAKEINTRESOURCEW(IDR_ABOUT_PNG), RT_RCDATA);
    if (!res) return nullptr;
    const HGLOBAL mem = LoadResource(hInstance, res);
    if (!mem) return nullptr;
    const DWORD size = SizeofResource(hInstance, res);
    const BYTE* data = static_cast<const BYTE*>(LockResource(mem));
    if (!data || size == 0) return nullptr;

    // Copy the bytes into a movable HGLOBAL and wrap it in an IStream that
    // GDI+ can read (the stream takes ownership of the HGLOBAL).
    HGLOBAL copy = GlobalAlloc(GMEM_MOVEABLE, size);
    if (!copy) return nullptr;
    memcpy(static_cast<BYTE*>(GlobalLock(copy)), data, size);
    GlobalUnlock(copy);

    IStream* stream = nullptr;
    if (FAILED(CreateStreamOnHGlobal(copy, TRUE, &stream))) {
        GlobalFree(copy);
        return nullptr;
    }

    Gdiplus::Image* img = Gdiplus::Image::FromStream(stream);
    stream->Release();
    if (!img || img->GetLastStatus() != Gdiplus::Ok) {
        delete img;
        return nullptr;
    }
    return static_cast<Gdiplus::Bitmap*>(img);
}

// Create a font based on the system's message font (Segoe UI 9pt on modern
// Windows), optionally bold. The About window's labels use these so the text
// matches the system UI and only the "Build Information:" heading is bold.
HFONT make_font(bool bold) {
    NONCLIENTMETRICSW ncm{};
    ncm.cbSize = sizeof(ncm);
    SystemParametersInfoW(SPI_GETNONCLIENTMETRICS, sizeof(ncm), &ncm, 0);
    LOGFONTW lf = ncm.lfMessageFont;
    lf.lfWeight = bold ? FW_BOLD : FW_NORMAL;
    return CreateFontIndirectW(&lf);
}

// Window procedure for the image control: paint the GDI+ bitmap (alpha-aware,
// so the PNG's transparent background shows the dialog's button-face color).
// A plain SS_BITMAP static would not honor the alpha channel.
LRESULT CALLBACK image_wndproc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    AboutState* st = reinterpret_cast<AboutState*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    switch (msg) {
    case WM_PAINT: {
        PAINTSTRUCT ps{};
        HDC hdc = BeginPaint(hwnd, &ps);
        if (st && st->image) {
            Gdiplus::Graphics g(hdc);
            g.DrawImage(st->image, 0, 0, kImageSize, kImageSize);
        }
        EndPaint(hwnd, &ps);
        return 0;
    }
    default:
        break;
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}

HWND make_label(HWND parent, const std::wstring& text, int x, int y, int w, int h,
                DWORD style, HFONT font) {
    HWND hwnd = CreateWindowExW(0, L"STATIC", text.c_str(), WS_CHILD | WS_VISIBLE | style, x, y,
                                w, h, parent, nullptr, GetModuleHandleW(nullptr), nullptr);
    SendMessageW(hwnd, WM_SETFONT, reinterpret_cast<WPARAM>(font), TRUE);
    return hwnd;
}

void create_controls(HWND hwnd, AboutState* st) {
    st->normal_font = make_font(false);
    st->bold_font = make_font(true);

    // Image, centered at the top. A dedicated window class (not a subclassed
    // STATIC) so the GDI+ bitmap can be painted alpha-aware.
    HWND img = CreateWindowExW(0, kImageClass, L"", WS_CHILD | WS_VISIBLE, kImageX, kImageY,
                               kImageSize, kImageSize, hwnd,
                               reinterpret_cast<HMENU>(kIdImage),
                               GetModuleHandleW(nullptr), nullptr);
    SetWindowLongPtrW(img, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(st));

    // "Build Information:" heading (left justified, bold), then the version
    // and the build date/time, left justified in the normal weight.
    const int headingY = kImageY + kImageSize + 12;  // 156
    make_label(hwnd, L"Build Information:", kMargin, headingY, kClientW - 2 * kMargin, 18,
               SS_LEFT, st->bold_font);
    const std::wstring version = L"Version " + to_wide(dn::version().c_str());
    make_label(hwnd, version, kMargin, headingY + 22, kClientW - 2 * kMargin, 18, SS_LEFT,
               st->normal_font);
    // Build date/time from the CMake-generated build_info.cpp (refreshed on
    // every build), not the __DATE__/__TIME__ compile-time macros.
    const std::wstring built =
        L"Built " + to_wide(dn::build_date()) + L" " + to_wide(dn::build_time());
    make_label(hwnd, built, kMargin, headingY + 42, kClientW - 2 * kMargin, 18, SS_LEFT,
               st->normal_font);

    // Blank line, then the brief description.
    const int descY = headingY + 42 + 18 + 20;  // 236
    make_label(
        hwnd,
        L"DupNames scans local and network directories for files with similar "
        L"names and helps you delete duplicates from common (non-protected) "
        L"directories.",
        kMargin, descY, kClientW - 2 * kMargin, 48, SS_LEFT, st->normal_font);

    // OK button, centered.
    HWND ok = CreateWindowExW(0, L"BUTTON", L"OK", WS_CHILD | WS_VISIBLE | BS_DEFPUSHBUTTON,
                              (kClientW - 90) / 2, descY + 64, 90, 26, hwnd,
                              reinterpret_cast<HMENU>(kIdOk), GetModuleHandleW(nullptr),
                              nullptr);
    SendMessageW(ok, WM_SETFONT, reinterpret_cast<WPARAM>(st->normal_font), TRUE);
}

LRESULT CALLBACK about_wndproc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    AboutState* st = reinterpret_cast<AboutState*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    switch (msg) {
    case WM_CREATE: {
        auto* cs = reinterpret_cast<CREATESTRUCTW*>(lp);
        st = reinterpret_cast<AboutState*>(cs->lpCreateParams);
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(st));
        st->hwnd = hwnd;
        create_controls(hwnd, st);
        return 0;
    }
    case WM_COMMAND:
        if (LOWORD(wp) == kIdOk) {
            DestroyWindow(hwnd);
            return 0;
        }
        break;
    case WM_CLOSE:
        DestroyWindow(hwnd);
        return 0;
    case WM_DESTROY:
        if (st) {
            st->hwnd = nullptr;
            if (st->normal_font) {
                DeleteObject(st->normal_font);
                st->normal_font = nullptr;
            }
            if (st->bold_font) {
                DeleteObject(st->bold_font);
                st->bold_font = nullptr;
            }
        }
        return 0;
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}

}  // namespace

namespace gui {

void show_about(HWND owner, HINSTANCE hInstance) {
    static bool registered = false;
    if (!registered) {
        WNDCLASSEXW wc = {};
        wc.cbSize = sizeof(wc);
        wc.lpfnWndProc = about_wndproc;
        wc.hInstance = hInstance;
        wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
        wc.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_BTNFACE + 1);
        wc.lpszClassName = kClass;
        if (!RegisterClassExW(&wc)) return;
        // Image control class (paints the GDI+ bitmap; no background brush —
        // image_wndproc paints the whole client area).
        WNDCLASSEXW ic = {};
        ic.cbSize = sizeof(ic);
        ic.lpfnWndProc = image_wndproc;
        ic.hInstance = hInstance;
        ic.hbrBackground = nullptr;
        ic.lpszClassName = kImageClass;
        RegisterClassExW(&ic);
        registered = true;
    }

    AboutState st;
    st.image = load_about_image(hInstance);

    DWORD style = WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_BORDER;
    RECT rc = {0, 0, kClientW, kClientH};
    AdjustWindowRect(&rc, style, FALSE);
    const int winW = rc.right - rc.left;
    const int winH = rc.bottom - rc.top;

    int x = 100, y = 100;
    if (owner && IsWindow(owner)) {
        RECT orc{};
        GetWindowRect(owner, &orc);
        x = orc.left + ((orc.right - orc.left) - winW) / 2;
        y = orc.top + ((orc.bottom - orc.top) - winH) / 2;
    }

    HWND hwnd = CreateWindowExW(0, kClass, L"About DupNames", style, x, y, winW, winH,
                                owner, nullptr, hInstance,
                                reinterpret_cast<LPVOID>(&st));
    if (!hwnd) {
        delete st.image;
        return;
    }

    EnableWindow(owner, FALSE);
    ShowWindow(hwnd, SW_SHOW);
    UpdateWindow(hwnd);

    MSG m;
    while (st.hwnd && IsWindow(st.hwnd)) {
        if (!GetMessageW(&m, nullptr, 0, 0)) break;
        if (IsDialogMessageW(hwnd, &m)) continue;
        TranslateMessage(&m);
        DispatchMessageW(&m);
    }

    EnableWindow(owner, TRUE);
    delete st.image;
}

}  // namespace gui

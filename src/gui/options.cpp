#include "options.hpp"

#include <windows.h>

#include <cwchar>
#include <string>
#include <vector>

namespace {

constexpr wchar_t kClass[] = L"DNOptionsWnd";

// Control IDs.
enum : int {
    kIdMatchThreshold = 100,
    kIdCloseThreshold,
    kIdMergeClose,
    kIdYearLo,
    kIdYearHi,
    kIdWYear,
    kIdWTokens,
    kIdYearCap,
    kIdRecursive,
    kIdSkipHidden,
    kIdInclude,
    kIdExclude,
    kIdJunk,
    kIdOk,
    kIdCancel,
};

// Layout (client-area coordinates).
constexpr int kClientW = 460;
constexpr int kClientH = 484;
constexpr int kMargin = 12;
constexpr int kGroupW = kClientW - 2 * kMargin;  // 436
constexpr int kLabelX = 24;
constexpr int kLabelW = 120;
constexpr int kFieldX = 150;
constexpr int kFieldW = 90;
constexpr int kFieldH = 24;

struct OptionsState {
    dn::Config cfg;
    bool accepted = false;
    HWND hwnd = nullptr;
};

std::wstring to_wide(const std::string& s) {
    if (s.empty()) return {};
    const int n = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), static_cast<int>(s.size()),
                                      nullptr, 0);
    std::wstring out(static_cast<std::size_t>(n), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, s.c_str(), static_cast<int>(s.size()), out.data(), n);
    return out;
}

std::string to_narrow(const std::wstring& s) {
    if (s.empty()) return {};
    const int n = WideCharToMultiByte(CP_UTF8, 0, s.c_str(), static_cast<int>(s.size()),
                                      nullptr, 0, nullptr, nullptr);
    std::string out(static_cast<std::size_t>(n), '\0');
    WideCharToMultiByte(CP_UTF8, 0, s.c_str(), static_cast<int>(s.size()), out.data(), n,
                        nullptr, nullptr);
    return out;
}

std::wstring double_to_str(double v) {
    wchar_t buf[32];
    swprintf(buf, 32, L"%.4f", v);
    std::wstring s(buf);
    while (!s.empty() && s.back() == L'0') s.pop_back();
    if (!s.empty() && s.back() == L'.') s.pop_back();
    return s;
}

std::wstring int_to_str(int v) {
    wchar_t buf[16];
    swprintf(buf, 16, L"%d", v);
    return buf;
}

bool parse_double(const std::wstring& s, double& out) {
    if (s.empty()) return false;
    try {
        std::size_t pos = 0;
        const double v = std::stod(s, &pos);
        if (pos != s.size()) return false;
        out = v;
        return true;
    } catch (...) {
        return false;
    }
}

bool parse_int(const std::wstring& s, int& out) {
    if (s.empty()) return false;
    try {
        std::size_t pos = 0;
        const long v = std::stol(s, &pos);
        if (pos != s.size()) return false;
        out = static_cast<int>(v);
        return true;
    } catch (...) {
        return false;
    }
}

std::wstring get_text(HWND h) {
    const int n = GetWindowTextLengthW(h);
    std::wstring s(static_cast<std::size_t>(n), L'\0');
    if (n > 0) GetWindowTextW(h, s.data(), n + 1);
    return s;
}

std::wstring junk_to_lines(const std::vector<std::string>& junk) {
    std::wstring out;
    for (std::size_t i = 0; i < junk.size(); ++i) {
        if (i) out += L"\r\n";
        out += to_wide(junk[i]);
    }
    return out;
}

std::vector<std::string> lines_to_junk(const std::wstring& s) {
    std::vector<std::string> out;
    std::wstring cur;
    auto flush = [&]() {
        const std::size_t b = cur.find_first_not_of(L" \t\r");
        if (b != std::wstring::npos) {
            const std::size_t e = cur.find_last_not_of(L" \t\r");
            out.push_back(to_narrow(cur.substr(b, e - b + 1)));
        }
        cur.clear();
    };
    for (const wchar_t c : s)
        (c == L'\n' || c == L'\r' || c == L',') ? flush() : cur.push_back(c);
    flush();
    return out;
}

HWND make_label(HWND parent, const wchar_t* text, int x, int y, int w, int h) {
    return CreateWindowExW(0, L"STATIC", text, WS_CHILD | WS_VISIBLE | SS_LEFT, x, y, w, h,
                           parent, nullptr, GetModuleHandleW(nullptr), nullptr);
}

HWND make_edit(HWND parent, int id, int x, int y, int w, int h, DWORD extra = 0) {
    return CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"",
                           WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL | extra, x, y, w, h, parent,
                           reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)),
                           GetModuleHandleW(nullptr), nullptr);
}

HWND make_check(HWND parent, int id, const wchar_t* text, int x, int y, int w, int h) {
    return CreateWindowExW(0, L"BUTTON", text, WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX, x, y, w, h,
                           parent, reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)),
                           GetModuleHandleW(nullptr), nullptr);
}

HWND make_button(HWND parent, int id, const wchar_t* text, int x, int y, int w, int h,
                 bool def) {
    return CreateWindowExW(0, L"BUTTON", text,
                           WS_CHILD | WS_VISIBLE | (def ? BS_DEFPUSHBUTTON : BS_PUSHBUTTON), x, y,
                           w, h, parent, reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)),
                           GetModuleHandleW(nullptr), nullptr);
}

HWND make_group(HWND parent, const wchar_t* text, int x, int y, int w, int h) {
    return CreateWindowExW(0, L"BUTTON", text, WS_CHILD | WS_VISIBLE | BS_GROUPBOX, x, y, w, h,
                           parent, nullptr, GetModuleHandleW(nullptr), nullptr);
}

void create_controls(HWND hwnd, const dn::Config& cfg) {
    // Matching group.
    make_group(hwnd, L"Matching", kMargin, 8, kGroupW, 184);
    make_label(hwnd, L"Match threshold", kLabelX, 30, kLabelW, kFieldH);
    make_edit(hwnd, kIdMatchThreshold, kFieldX, 30, kFieldW, kFieldH);
    make_label(hwnd, L"Close threshold", kLabelX, 56, kLabelW, kFieldH);
    make_edit(hwnd, kIdCloseThreshold, kFieldX, 56, kFieldW, kFieldH);
    make_check(hwnd, kIdMergeClose, L"Merge CLOSE", kLabelX, 82, 150, kFieldH);
    make_label(hwnd, L"Year range (lo-hi)", kLabelX, 108, kLabelW, kFieldH);
    make_edit(hwnd, kIdYearLo, kFieldX, 108, 55, kFieldH);
    make_edit(hwnd, kIdYearHi, kFieldX + 62, 108, 55, kFieldH);
    make_label(hwnd, L"Weights (yr/tok)", kLabelX, 134, kLabelW, kFieldH);
    make_edit(hwnd, kIdWYear, kFieldX, 134, 55, kFieldH);
    make_edit(hwnd, kIdWTokens, kFieldX + 62, 134, 55, kFieldH);
    make_label(hwnd, L"Year cap", kLabelX, 160, kLabelW, kFieldH);
    make_edit(hwnd, kIdYearCap, kFieldX, 160, kFieldW, kFieldH);

    // Scanning group.
    make_group(hwnd, L"Scanning", kMargin, 200, kGroupW, 132);
    make_check(hwnd, kIdRecursive, L"Recursive", kLabelX, 222, 150, kFieldH);
    make_check(hwnd, kIdSkipHidden, L"Skip hidden", kLabelX, 248, 150, kFieldH);
    make_label(hwnd, L"Include", kLabelX, 274, kLabelW, kFieldH);
    make_edit(hwnd, kIdInclude, kFieldX, 274, 280, kFieldH);
    make_label(hwnd, L"Exclude", kLabelX, 300, kLabelW, kFieldH);
    make_edit(hwnd, kIdExclude, kFieldX, 300, 280, kFieldH);

    // Junk tokens group.
    make_group(hwnd, L"Junk tokens (one per line)", kMargin, 340, kGroupW, 96);
    make_edit(hwnd, kIdJunk, kLabelX, 362, 412, 64, ES_MULTILINE | ES_AUTOVSCROLL | WS_VSCROLL);

    // Buttons.
    make_button(hwnd, kIdOk, L"OK", 250, 446, 90, 26, true);
    make_button(hwnd, kIdCancel, L"Cancel", 348, 446, 90, 26, false);

    // Populate from cfg.
    SetWindowTextW(GetDlgItem(hwnd, kIdMatchThreshold), double_to_str(cfg.match_threshold).c_str());
    SetWindowTextW(GetDlgItem(hwnd, kIdCloseThreshold), double_to_str(cfg.close_threshold).c_str());
    SendMessageW(GetDlgItem(hwnd, kIdMergeClose), BM_SETCHECK, cfg.merge_close ? BST_CHECKED : BST_UNCHECKED, 0);
    SetWindowTextW(GetDlgItem(hwnd, kIdYearLo), int_to_str(cfg.year_lo).c_str());
    SetWindowTextW(GetDlgItem(hwnd, kIdYearHi), int_to_str(cfg.year_hi).c_str());
    SetWindowTextW(GetDlgItem(hwnd, kIdWYear), double_to_str(cfg.w_year).c_str());
    SetWindowTextW(GetDlgItem(hwnd, kIdWTokens), double_to_str(cfg.w_tokens).c_str());
    SetWindowTextW(GetDlgItem(hwnd, kIdYearCap), double_to_str(cfg.year_cap).c_str());
    SendMessageW(GetDlgItem(hwnd, kIdRecursive), BM_SETCHECK, cfg.recursive ? BST_CHECKED : BST_UNCHECKED, 0);
    SendMessageW(GetDlgItem(hwnd, kIdSkipHidden), BM_SETCHECK, cfg.skip_hidden ? BST_CHECKED : BST_UNCHECKED, 0);
    SetWindowTextW(GetDlgItem(hwnd, kIdInclude), to_wide(cfg.include).c_str());
    SetWindowTextW(GetDlgItem(hwnd, kIdExclude), to_wide(cfg.exclude).c_str());
    SetWindowTextW(GetDlgItem(hwnd, kIdJunk), junk_to_lines(cfg.junk).c_str());
}

bool validate_and_apply(OptionsState* st) {
    const HWND h = st->hwnd;
    dn::Config c = st->cfg;
    double d;
    int i;

    if (!parse_double(get_text(GetDlgItem(h, kIdMatchThreshold)), d) || d < 0.0 || d > 1.0) {
        MessageBoxW(h, L"Match threshold must be a number from 0 to 1.", L"Invalid value",
                    MB_OK | MB_ICONWARNING);
        SetFocus(GetDlgItem(h, kIdMatchThreshold));
        return false;
    }
    c.match_threshold = d;

    if (!parse_double(get_text(GetDlgItem(h, kIdCloseThreshold)), d) || d < 0.0 || d > 1.0) {
        MessageBoxW(h, L"Close threshold must be a number from 0 to 1.", L"Invalid value",
                    MB_OK | MB_ICONWARNING);
        SetFocus(GetDlgItem(h, kIdCloseThreshold));
        return false;
    }
    c.close_threshold = d;

    c.merge_close =
        (SendMessageW(GetDlgItem(h, kIdMergeClose), BM_GETCHECK, 0, 0) == BST_CHECKED);

    if (!parse_int(get_text(GetDlgItem(h, kIdYearLo)), i)) {
        MessageBoxW(h, L"Year (lo) must be an integer.", L"Invalid value", MB_OK | MB_ICONWARNING);
        SetFocus(GetDlgItem(h, kIdYearLo));
        return false;
    }
    c.year_lo = i;
    if (!parse_int(get_text(GetDlgItem(h, kIdYearHi)), i)) {
        MessageBoxW(h, L"Year (hi) must be an integer.", L"Invalid value", MB_OK | MB_ICONWARNING);
        SetFocus(GetDlgItem(h, kIdYearHi));
        return false;
    }
    c.year_hi = i;
    if (c.year_lo >= c.year_hi) {
        MessageBoxW(h, L"Year (lo) must be less than year (hi).", L"Invalid value",
                    MB_OK | MB_ICONWARNING);
        SetFocus(GetDlgItem(h, kIdYearLo));
        return false;
    }

    if (!parse_double(get_text(GetDlgItem(h, kIdWYear)), d) || d < 0.0 || d > 1.0) {
        MessageBoxW(h, L"Year weight must be a number from 0 to 1.", L"Invalid value",
                    MB_OK | MB_ICONWARNING);
        SetFocus(GetDlgItem(h, kIdWYear));
        return false;
    }
    c.w_year = d;
    if (!parse_double(get_text(GetDlgItem(h, kIdWTokens)), d) || d < 0.0 || d > 1.0) {
        MessageBoxW(h, L"Token weight must be a number from 0 to 1.", L"Invalid value",
                    MB_OK | MB_ICONWARNING);
        SetFocus(GetDlgItem(h, kIdWTokens));
        return false;
    }
    c.w_tokens = d;

    if (!parse_double(get_text(GetDlgItem(h, kIdYearCap)), d) || d < 0.0 || d > 1.0) {
        MessageBoxW(h, L"Year cap must be a number from 0 to 1.", L"Invalid value",
                    MB_OK | MB_ICONWARNING);
        SetFocus(GetDlgItem(h, kIdYearCap));
        return false;
    }
    c.year_cap = d;

    c.recursive = (SendMessageW(GetDlgItem(h, kIdRecursive), BM_GETCHECK, 0, 0) == BST_CHECKED);
    c.skip_hidden =
        (SendMessageW(GetDlgItem(h, kIdSkipHidden), BM_GETCHECK, 0, 0) == BST_CHECKED);
    c.include = to_narrow(get_text(GetDlgItem(h, kIdInclude)));
    c.exclude = to_narrow(get_text(GetDlgItem(h, kIdExclude)));
    c.junk = lines_to_junk(get_text(GetDlgItem(h, kIdJunk)));

    st->cfg = c;
    return true;
}

LRESULT CALLBACK options_wndproc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    OptionsState* st = reinterpret_cast<OptionsState*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    switch (msg) {
    case WM_CREATE: {
        auto* cs = reinterpret_cast<CREATESTRUCTW*>(lParam);
        st = reinterpret_cast<OptionsState*>(cs->lpCreateParams);
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(st));
        st->hwnd = hwnd;
        create_controls(hwnd, st->cfg);
        return 0;
    }
    case WM_COMMAND:
        if (LOWORD(wParam) == kIdOk) {
            if (validate_and_apply(st)) {
                st->accepted = true;
                DestroyWindow(hwnd);
            }
            return 0;
        }
        if (LOWORD(wParam) == kIdCancel) {
            DestroyWindow(hwnd);
            return 0;
        }
        break;
    case WM_CLOSE:
        DestroyWindow(hwnd);
        return 0;
    case WM_DESTROY:
        if (st) st->hwnd = nullptr;
        return 0;
    }
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

}  // namespace

namespace gui {

bool show_options(HWND owner, HINSTANCE hInstance, dn::Config& cfg) {
    static bool registered = false;
    if (!registered) {
        WNDCLASSEXW wc = {};
        wc.cbSize = sizeof(wc);
        wc.lpfnWndProc = options_wndproc;
        wc.hInstance = hInstance;
        wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
        wc.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_BTNFACE + 1);
        wc.lpszClassName = kClass;
        if (!RegisterClassExW(&wc)) return false;
        registered = true;
    }

    OptionsState st;
    st.cfg = cfg;

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

    HWND hwnd = CreateWindowExW(0, kClass, L"Options", style, x, y, winW, winH, owner, nullptr,
                                hInstance, reinterpret_cast<LPVOID>(&st));
    if (!hwnd) return false;

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

    if (st.accepted) cfg = st.cfg;
    return st.accepted;
}

}  // namespace gui

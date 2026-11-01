#include "rename.hpp"

#include <windows.h>

#include <string>

namespace {

constexpr wchar_t kClass[] = L"DNRenameWnd";

// Control IDs.
enum : int {
    kIdName = 100,
    kIdOk,
    kIdCancel,
};

// Layout (client-area coordinates).
constexpr int kClientW = 420;
constexpr int kClientH = 132;
constexpr int kMargin = 12;
constexpr int kFieldH = 24;

struct RenameState {
    std::wstring current_name;
    std::wstring new_name;
    bool accepted = false;
    HWND hwnd = nullptr;
};

std::wstring get_text(HWND h) {
    const int n = GetWindowTextLengthW(h);
    std::wstring s(static_cast<std::size_t>(n), L'\0');
    if (n > 0) GetWindowTextW(h, s.data(), n + 1);
    return s;
}

void create_controls(HWND hwnd, const std::wstring& current_name) {
    CreateWindowExW(0, L"STATIC", L"New file name:", WS_CHILD | WS_VISIBLE | SS_LEFT,
                    kMargin, 16, 300, 16, hwnd, nullptr, GetModuleHandleW(nullptr), nullptr);
    CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", current_name.c_str(),
                    WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL, kMargin, 38,
                    kClientW - 2 * kMargin, kFieldH, hwnd,
                    reinterpret_cast<HMENU>(static_cast<INT_PTR>(kIdName)),
                    GetModuleHandleW(nullptr), nullptr);
    CreateWindowExW(0, L"BUTTON", L"OK", WS_CHILD | WS_VISIBLE | BS_DEFPUSHBUTTON,
                    228, 84, 84, 26, hwnd,
                    reinterpret_cast<HMENU>(static_cast<INT_PTR>(kIdOk)),
                    GetModuleHandleW(nullptr), nullptr);
    CreateWindowExW(0, L"BUTTON", L"Cancel", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                    320, 84, 84, 26, hwnd,
                    reinterpret_cast<HMENU>(static_cast<INT_PTR>(kIdCancel)),
                    GetModuleHandleW(nullptr), nullptr);

    // Select the base name (without the extension) so a typical rename only
    // edits the name, leaving the extension in place.
    const HWND edit = GetDlgItem(hwnd, kIdName);
    const std::size_t dot = current_name.find_last_of(L'.');
    const std::size_t selLen = (dot == std::wstring::npos) ? current_name.size() : dot;
    SetWindowTextW(edit, current_name.c_str());
    SendMessageW(edit, EM_SETSEL, 0, static_cast<LPARAM>(selLen));
    SetFocus(edit);
}

LRESULT CALLBACK rename_wndproc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    RenameState* st = reinterpret_cast<RenameState*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    switch (msg) {
    case WM_CREATE: {
        auto* cs = reinterpret_cast<CREATESTRUCTW*>(lParam);
        st = reinterpret_cast<RenameState*>(cs->lpCreateParams);
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(st));
        st->hwnd = hwnd;
        create_controls(hwnd, st->current_name);
        return 0;
    }
    case WM_COMMAND:
        if (LOWORD(wParam) == kIdOk) {
            st->new_name = get_text(GetDlgItem(hwnd, kIdName));
            st->accepted = true;
            DestroyWindow(hwnd);
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

bool show_rename_dialog(HWND owner, HINSTANCE hInstance,
                        const std::wstring& current_name, std::wstring& new_name) {
    static bool registered = false;
    if (!registered) {
        WNDCLASSEXW wc = {};
        wc.cbSize = sizeof(wc);
        wc.lpfnWndProc = rename_wndproc;
        wc.hInstance = hInstance;
        wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
        wc.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_BTNFACE + 1);
        wc.lpszClassName = kClass;
        if (!RegisterClassExW(&wc)) return false;
        registered = true;
    }

    RenameState st;
    st.current_name = current_name;

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

    HWND hwnd = CreateWindowExW(0, kClass, L"Rename file", style, x, y, winW, winH, owner,
                                nullptr, hInstance, reinterpret_cast<LPVOID>(&st));
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

    if (st.accepted) new_name = st.new_name;
    return st.accepted;
}

}  // namespace gui

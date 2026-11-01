#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include "app.hpp"

#include <objbase.h>
#include <shobjidl.h>
#include <shlobj.h>
#include <shellapi.h>
#include <commctrl.h>

#include <algorithm>
#include <cstdio>
#include <cwchar>
#include <filesystem>
#include <iterator>
#include <string>
#include <system_error>
#include <unordered_set>
#include <utility>
#include <vector>

#include "dn/deletion.hpp"
#include "dn/ini.hpp"
#include "dn/match.hpp"
#include "dn/scanner.hpp"

#include "options.hpp"
#include "rename.hpp"

namespace gui {

namespace {

constexpr wchar_t kClass[] = L"DNMainWnd";
constexpr INT_PTR kIdAdd = 1001;
constexpr INT_PTR kIdRemove = 1002;
constexpr INT_PTR kIdScan = 1003;
constexpr INT_PTR kIdOptions = 1007;
constexpr INT_PTR kIdDelete = 1008;
constexpr INT_PTR kIdDirList = 1004;
constexpr INT_PTR kIdQueue = 1005;
constexpr INT_PTR kIdStatus = 1006;
// Matches-list right-click context menu.
constexpr INT_PTR kIdCtxOpenFile = 2001;
constexpr INT_PTR kIdCtxOpenFolder = 2002;
constexpr INT_PTR kIdCtxRename = 2003;

// CLSID_FileOpenDialog (avoids relying on the SDK exporting the CLSID symbol).
const GUID kClsidFileOpenDialog = {0x43826d1e, 0xe718, 0x42ee,
                                   {0xbc, 0x55, 0xa1, 0xe2, 0x61, 0xc3, 0x7b, 0xfe}};

// Convert a native-narrow string to wide. std::filesystem::path::string() and
// the scanner's FileEntry::name use the system ANSI code page (not UTF-8), so
// CP_ACP is the correct source code page for a lossless round-trip.
std::wstring to_wide(const std::string& s) {
    if (s.empty()) return {};
    const int n = MultiByteToWideChar(CP_ACP, 0, s.c_str(), static_cast<int>(s.size()), nullptr, 0);
    std::wstring out(static_cast<size_t>(n), L'\0');
    MultiByteToWideChar(CP_ACP, 0, s.c_str(), static_cast<int>(s.size()), out.data(), n);
    return out;
}

// Inverse of to_wide: wide -> native-narrow (CP_ACP).
std::string to_narrow(const std::wstring& s) {
    if (s.empty()) return {};
    const int n = WideCharToMultiByte(CP_ACP, 0, s.c_str(), static_cast<int>(s.size()),
                                      nullptr, 0, nullptr, nullptr);
    std::string out(static_cast<size_t>(n), '\0');
    WideCharToMultiByte(CP_ACP, 0, s.c_str(), static_cast<int>(s.size()),
                        out.data(), n, nullptr, nullptr);
    return out;
}

void insert_dir_item(HWND list, int index, const std::wstring& text) {
    LVITEMW item{};
    item.mask = LVIF_TEXT;
    item.iItem = index;
    item.pszText = const_cast<LPWSTR>(text.c_str());
    ListView_InsertItem(list, &item);
}

// Format a byte count with three significant figures, e.g. "1.37 GB",
// "78.4 MB", "110 MB", "512 B" (matches the reference UI).
std::wstring format_size(std::uintmax_t bytes) {
    static const wchar_t* kUnits[] = {L"B", L"KB", L"MB", L"GB", L"TB"};
    if (bytes < 1024)
        return std::to_wstring(bytes) + L" B";
    double value = static_cast<double>(bytes);
    int unit = 0;
    while (value >= 1024.0 && unit < 4) {
        value /= 1024.0;
        ++unit;
    }
    wchar_t buf[40];
    if (value < 10.0)
        swprintf(buf, 39, L"%.2f %s", value, kUnits[unit]);
    else if (value < 100.0)
        swprintf(buf, 39, L"%.1f %s", value, kUnits[unit]);
    else
        swprintf(buf, 39, L"%.0f %s", value, kUnits[unit]);
    return buf;
}

// Format epoch seconds as the user's locale short date + long time
// (e.g. "9/9/2026 10:03:26 PM"); 0/unknown yields "unknown".
std::wstring format_epoch(std::int64_t epoch) {
    if (epoch <= 0) return L"unknown";
    // FILETIME is 100-ns intervals since 1601-01-01; the Unix epoch offset is
    // 11644473600 seconds.
    const std::uint64_t ft100 =
        static_cast<std::uint64_t>(epoch) * 10000000ULL + 116444736000000000ULL;
    FILETIME ft{};
    ft.dwLowDateTime = static_cast<DWORD>(ft100 & 0xFFFFFFFFu);
    ft.dwHighDateTime = static_cast<DWORD>(ft100 >> 32);
    SYSTEMTIME st{};
    if (!FileTimeToSystemTime(&ft, &st)) return L"unknown";
    wchar_t date[64]{};
    wchar_t time[64]{};
    if (GetDateFormatW(LOCALE_USER_DEFAULT, DATE_SHORTDATE, &st, nullptr, date, 64) == 0)
        return L"unknown";
    // A null format + flags 0 yields the locale's default time picture, which
    // for en-US is "h:mm:ss tt" (e.g. "10:03:26 PM") — matching the reference
    // UI. (The old TIME_LONGTIME flag value no longer exists in current SDKs.)
    if (GetTimeFormatW(LOCALE_USER_DEFAULT, 0, &st, nullptr, time, 64) == 0)
        return L"unknown";
    return std::wstring(date) + L" " + time;
}

// Drive label for a path: "H:" for a drive letter, "\\server\share" for UNC.
std::wstring drive_of(const std::wstring& path) {
    if (path.size() >= 2 && path[1] == L':')
        return path.substr(0, 2);
    if (path.size() >= 2 && path[0] == L'\\' && path[1] == L'\\') {
        std::wstring out = L"\\\\";
        int parts = 0;
        for (std::size_t i = 2; i < path.size() && parts < 2; ++i) {
            out += path[i];
            if (path[i] == L'\\') ++parts;
        }
        return out;
    }
    return L"";
}

// True if `r` overlaps the virtual screen (all monitors) by at least 100x100
// px. Guards against restoring a window saved on a now-disconnected monitor.
bool rect_on_screen(const RECT& r) {
    RECT vs{};
    vs.left = GetSystemMetrics(SM_XVIRTUALSCREEN);
    vs.top = GetSystemMetrics(SM_YVIRTUALSCREEN);
    vs.right = vs.left + GetSystemMetrics(SM_CXVIRTUALSCREEN);
    vs.bottom = vs.top + GetSystemMetrics(SM_CYVIRTUALSCREEN);
    const int overlapW = std::min(r.right, vs.right) - std::max(r.left, vs.left);
    const int overlapH = std::min(r.bottom, vs.bottom) - std::max(r.top, vs.top);
    return overlapW >= 100 && overlapH >= 100;
}

// Delete the given files. Local files go to the Recycle Bin (recoverable);
// UNC/network files are removed directly (the Recycle Bin does not support
// network paths). Returns the number of files successfully deleted and appends
// any error text to `error`.
int delete_files(const std::vector<std::wstring>& paths, std::wstring& error) {
    std::vector<std::wstring> local, unc;
    for (const auto& p : paths)
        (p.size() >= 2 && p[0] == L'\\' && p[1] == L'\\' ? unc : local).push_back(p);

    int deleted = 0;
    if (!local.empty()) {
        std::wstring list;
        for (const auto& p : local) { list += p; list += L'\0'; }
        list += L'\0';  // double-null terminate the array
        SHFILEOPSTRUCTW op{};
        op.wFunc = FO_DELETE;
        op.pFrom = list.c_str();
        op.fFlags = FOF_ALLOWUNDO | FOF_NOCONFIRMATION | FOF_SILENT;
        if (SHFileOperationW(&op) == 0 && !op.fAnyOperationsAborted)
            deleted += static_cast<int>(local.size());
        else
            error += L"Could not move some files to the Recycle Bin.\n";
    }
    for (const auto& p : unc) {
        std::error_code ec;
        if (std::filesystem::remove(p, ec)) ++deleted;
        else error += p + L": " + to_wide(ec.message()) + L"\n";
    }
    return deleted;
}

}  // namespace

App::App(HINSTANCE hInstance, const dn::CliArgs& cli)
    : hInstance_(hInstance), cli_(cli),
      ini_path_(cli.ini_path.empty() ? dn::default_ini_path() : cli.ini_path) {}

int App::Run(int nCmdShow) {
    CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);

    INITCOMMONCONTROLSEX icc{};
    icc.dwSize = sizeof(icc);
    icc.dwICC = ICC_LISTVIEW_CLASSES | ICC_TREEVIEW_CLASSES | ICC_STANDARD_CLASSES;
    InitCommonControlsEx(&icc);

    WNDCLASSEXW wc{};
    wc.cbSize = sizeof(wc);
    wc.lpfnWndProc = &App::WndProcThunk;
    wc.hInstance = hInstance_;
    wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    wc.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);
    wc.lpszClassName = kClass;
    if (!RegisterClassExW(&wc)) {
        CoUninitialize();
        return 1;
    }

    LoadWindowLayout();  // restore [Window] rect + column widths from the INI

    int x = CW_USEDEFAULT, y = CW_USEDEFAULT, w = 900, h = 640;
    if (have_window_rect_) {
        x = window_rect_.left;
        y = window_rect_.top;
        w = window_rect_.right - window_rect_.left;
        h = window_rect_.bottom - window_rect_.top;
    }
    hwnd_ = CreateWindowExW(
        0, kClass, L"DupNames", WS_OVERLAPPEDWINDOW,
        x, y, w, h,
        nullptr, nullptr, hInstance_, this);
    if (!hwnd_) {
        CoUninitialize();
        return 1;
    }

    CreateControls();
    RECT rc{};
    GetClientRect(hwnd_, &rc);
    Layout(rc.right - rc.left, rc.bottom - rc.top);
    LoadState();

    ShowWindow(hwnd_, restore_maximized_ ? SW_SHOWMAXIMIZED : nCmdShow);
    UpdateWindow(hwnd_);

    MSG msg{};
    while (GetMessageW(&msg, nullptr, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    CoUninitialize();
    return static_cast<int>(msg.wParam);
}

LRESULT CALLBACK App::WndProcThunk(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    App* self = nullptr;
    if (msg == WM_NCCREATE) {
        const auto* cs = reinterpret_cast<const CREATESTRUCTW*>(lp);
        self = static_cast<App*>(cs->lpCreateParams);
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
    } else {
        self = reinterpret_cast<App*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    }
    if (!self) return DefWindowProcW(hwnd, msg, wp, lp);
    return self->WndProc(hwnd, msg, wp, lp);
}

LRESULT App::WndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
        case WM_COMMAND: {
            const int id = LOWORD(wp);
            // Context-menu commands arrive with a 0 notification code.
            if (id == kIdCtxOpenFile) { OnOpenFile(); return 0; }
            if (id == kIdCtxOpenFolder) { OnOpenFolder(); return 0; }
            if (id == kIdCtxRename) { OnRenameFile(); return 0; }
            if (HIWORD(wp) == BN_CLICKED) {
                if (id == kIdAdd) OnAddFolder();
                else if (id == kIdRemove) OnRemoveFolder();
                else if (id == kIdScan) OnScan();
                else if (id == kIdOptions) OnOptions();
                else if (id == kIdDelete) OnDelete();
            }
            return 0;
        }
        case WM_NOTIFY: {
            const NMHDR* nm = reinterpret_cast<const NMHDR*>(lp);
            // Matches list custom draw: (1) alternating per-group shading —
            // even group numbers get a pale blue band (matching the reference
            // UI); (2) hide the checkbox on protected rows by painting the
            // checkbox cell with the row's background color, so protected files
            // cannot be selected for deletion.
            if (nm->hwndFrom == queue_ && nm->code == NM_CUSTOMDRAW) {
                NMLVCUSTOMDRAW* cd = reinterpret_cast<NMLVCUSTOMDRAW*>(lp);
                if (cd->nmcd.dwDrawStage == CDDS_PREPAINT)
                    return CDRF_NOTIFYITEMDRAW;
                const int row = static_cast<int>(cd->nmcd.dwItemSpec);
                const bool validRow =
                    row >= 0 && row < static_cast<int>(row_group_.size());
                if (cd->nmcd.dwDrawStage == CDDS_ITEMPREPAINT) {
                    if (validRow && row_group_[row] % 2 == 0) {
                        cd->clrTextBk = RGB(216, 233, 244);
                        return CDRF_NEWFONT;  // apply the new background color
                    }
                    return CDRF_DODEFAULT;
                }
                if (cd->nmcd.dwDrawStage == CDDS_ITEMPOSTPAINT) {
                    if (validRow && row < static_cast<int>(row_protected_.size()) &&
                        row_protected_[row]) {
                        const COLORREF bg =
                            (row_group_[row] % 2 == 0) ? RGB(216, 233, 244) : RGB(255, 255, 255);
                        RECT r{};
                        ListView_GetItemRect(queue_, row, &r, LVIR_BOUNDS);
                        // Width of the checkbox cell (the box plus its left
                        // margin), scaled to the control's DPI. The first
                        // column's text starts just past this, so painting it
                        // covers the checkbox without touching the text.
                        const int dpi = GetDeviceCaps(cd->nmcd.hdc, LOGPIXELSX);
                        const int cellW = MulDiv(16, dpi, 96);
                        RECT cover{r.left, r.top, r.left + cellW, r.bottom};
                        HBRUSH br = CreateSolidBrush(bg);
                        FillRect(cd->nmcd.hdc, &cover, br);
                        DeleteObject(br);
                    }
                    return CDRF_DODEFAULT;
                }
                return CDRF_DODEFAULT;
            }
            // Right-click on a matches row: show the context menu.
            if (nm->hwndFrom == queue_ && nm->code == NM_RCLICK)
                OnQueueContextMenu();
            // Clicking the "Type" column of a directory row toggles
            // Common <-> Protected (the user's preferred way to change type).
            if (nm->hwndFrom == dirList_ && nm->code == NM_CLICK) {
                // Resolve which column was clicked. Use the cursor position
                // (reliable at click time) converted to list-view client coords.
                POINT pt{};
                GetCursorPos(&pt);
                ScreenToClient(dirList_, &pt);
                LVHITTESTINFO hti{};
                hti.pt = pt;
                ListView_SubItemHitTest(dirList_, &hti);
                if (hti.iItem >= 0 && hti.iSubItem == 1)
                    OnDirTypeToggle(hti.iItem);
            }
            return 0;
        }
        case WM_SIZE:
            Layout(LOWORD(lp), HIWORD(lp));
            return 0;
        case WM_CLOSE:
            // Save while the child list views are still alive: Windows destroys
            // children before sending WM_DESTROY, so the column-width reads in
            // SaveWindowLayout must happen here, not in WM_DESTROY.
            SaveState();
            return DefWindowProcW(hwnd, msg, wp, lp);
        case WM_DESTROY:
            PostQuitMessage(0);
            return 0;
        default:
            break;
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}

void App::CreateControls() {
    CreateWindowExW(0, L"STATIC", L"Directories:", WS_CHILD | WS_VISIBLE,
                    8, 8, 200, 16, hwnd_, nullptr, hInstance_, nullptr);

    dirList_ = CreateWindowExW(WS_EX_CLIENTEDGE, WC_LISTVIEWW, L"",
                               WS_CHILD | WS_VISIBLE | LVS_REPORT | LVS_SINGLESEL,
                               8, 28, 400, 100, hwnd_, reinterpret_cast<HMENU>(kIdDirList),
                               hInstance_, nullptr);
    LVCOLUMNW col{};
    col.mask = LVCF_TEXT | LVCF_WIDTH;
    col.pszText = const_cast<LPWSTR>(L"Path");
    col.cx = 600;
    ListView_InsertColumn(dirList_, 0, &col);
    LVCOLUMNW colType{};
    colType.mask = LVCF_TEXT | LVCF_WIDTH;
    colType.pszText = const_cast<LPWSTR>(L"Type");
    colType.cx = 100;
    ListView_InsertColumn(dirList_, 1, &colType);
    // Restore persisted column widths (LoadWindowLayout ran before window
    // creation, so these hold the INI values or the defaults above).
    for (int i = 0; i < 2; ++i)
        ListView_SetColumnWidth(dirList_, i, dir_col_widths_[i]);

    addBtn_ = CreateWindowExW(0, L"BUTTON", L"Add...", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                              8, 150, 80, 24, hwnd_, reinterpret_cast<HMENU>(kIdAdd),
                              hInstance_, nullptr);
    removeBtn_ = CreateWindowExW(0, L"BUTTON", L"Remove", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                                 94, 150, 80, 24, hwnd_, reinterpret_cast<HMENU>(kIdRemove),
                                 hInstance_, nullptr);
    scanBtn_ = CreateWindowExW(0, L"BUTTON", L"Scan", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                                800, 150, 82, 24, hwnd_, reinterpret_cast<HMENU>(kIdScan),
                                hInstance_, nullptr);
    optionsBtn_ = CreateWindowExW(0, L"BUTTON", L"Options...", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                                   700, 150, 82, 24, hwnd_, reinterpret_cast<HMENU>(kIdOptions),
                                   hInstance_, nullptr);
    deleteBtn_ = CreateWindowExW(0, L"BUTTON", L"Delete", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                                  788, 150, 82, 24, hwnd_, reinterpret_cast<HMENU>(kIdDelete),
                                  hInstance_, nullptr);

    matchesLabel_ = CreateWindowExW(0, L"STATIC", L"Matches:", WS_CHILD | WS_VISIBLE,
                                    8, 188, 200, 16, hwnd_, nullptr, hInstance_, nullptr);

    // Flat list view of match clusters: one row per cluster member, with a
    // checkbox per row. (A tree view was used here originally, but inserts
    // failed in the user's session; the list view works and matches the
    // reference UI.)
    queue_ = CreateWindowExW(WS_EX_CLIENTEDGE, WC_LISTVIEWW, L"",
                              WS_CHILD | WS_VISIBLE | LVS_REPORT | LVS_SHOWSELALWAYS,
                              8, 208, 400, 300, hwnd_, reinterpret_cast<HMENU>(kIdQueue),
                              hInstance_, nullptr);
    ListView_SetExtendedListViewStyle(queue_, LVS_EX_CHECKBOXES | LVS_EX_FULLROWSELECT);
    const struct { const wchar_t* text; int width; } kCols[] = {
        {L"File Name", 280}, {L"Folder", 290}, {L"Drive", 55},
        {L"Group", 50}, {L"Size", 70}, {L"File modified", 130},
    };
    for (int i = 0; i < static_cast<int>(std::size(kCols)); ++i) {
        LVCOLUMNW qcol{};
        qcol.mask = LVCF_TEXT | LVCF_WIDTH;
        qcol.pszText = const_cast<LPWSTR>(kCols[i].text);
        qcol.cx = kCols[i].width;
        ListView_InsertColumn(queue_, i, &qcol);
    }
    // Restore persisted column widths (see the dirList_ restore above).
    for (int i = 0; i < static_cast<int>(std::size(kCols)); ++i)
        ListView_SetColumnWidth(queue_, i, queue_col_widths_[i]);

    status_ = CreateWindowExW(0, L"STATIC", L"Ready.", WS_CHILD | WS_VISIBLE | SS_LEFT,
                               8, 600, 400, 18, hwnd_, reinterpret_cast<HMENU>(kIdStatus),
                               hInstance_, nullptr);
}

void App::LoadState() {
    // Resolve thresholds (default <- INI <- CLI) and write back any CLI values.
    config_ = dn::resolve_config(ini_path_, cli_);
    // Load the persisted path list (protected + common) into the directory list.
    dirs_ = dn::ini_load_paths(ini_path_);
    RefreshDirList();
    SetStatus(L"Loaded " + std::to_wstring(dirs_.size()) + L" director(y/ies) from " +
              ini_path_);
}

void App::SaveState() {
    dn::ini_save_paths(ini_path_, dirs_);
    dn::ini_save_state(ini_path_, config_);
    SaveWindowLayout();
}

void App::LoadWindowLayout() {
    // Window rect: only restore if all four keys are present and the rect is
    // sane (big enough and at least partly on a connected monitor).
    if (dn::ini_has_key(ini_path_, L"Window", L"X") &&
        dn::ini_has_key(ini_path_, L"Window", L"Y") &&
        dn::ini_has_key(ini_path_, L"Window", L"W") &&
        dn::ini_has_key(ini_path_, L"Window", L"H")) {
        RECT r{};
        r.left = dn::ini_get_int(ini_path_, L"Window", L"X", 0);
        r.top = dn::ini_get_int(ini_path_, L"Window", L"Y", 0);
        r.right = r.left + dn::ini_get_int(ini_path_, L"Window", L"W", 900);
        r.bottom = r.top + dn::ini_get_int(ini_path_, L"Window", L"H", 640);
        const int w = r.right - r.left;
        const int h = r.bottom - r.top;
        if (w >= 320 && h >= 200 && rect_on_screen(r)) {
            window_rect_ = r;
            have_window_rect_ = true;
        }
    }
    restore_maximized_ = dn::ini_get_bool(ini_path_, L"Window", L"Maximized", false);

    // Column widths: a key present with a sane value overrides the default.
    for (int i = 0; i < 2; ++i) {
        const std::wstring key = L"DirCol" + std::to_wstring(i);
        if (dn::ini_has_key(ini_path_, L"Window", key)) {
            const int cw = dn::ini_get_int(ini_path_, L"Window", key, 0);
            if (cw >= 20) dir_col_widths_[i] = cw;
        }
    }
    for (int i = 0; i < 6; ++i) {
        const std::wstring key = L"QueueCol" + std::to_wstring(i);
        if (dn::ini_has_key(ini_path_, L"Window", key)) {
            const int cw = dn::ini_get_int(ini_path_, L"Window", key, 0);
            if (cw >= 20) queue_col_widths_[i] = cw;
        }
    }
}

void App::SaveWindowLayout() {
    if (!hwnd_) return;
    // Save the *normal* (restored) position, not the maximized one, plus a
    // flag so the window reopens maximized if it was.
    WINDOWPLACEMENT wp{};
    wp.length = sizeof(wp);
    if (GetWindowPlacement(hwnd_, &wp)) {
        dn::ini_set_int(ini_path_, L"Window", L"X", wp.rcNormalPosition.left);
        dn::ini_set_int(ini_path_, L"Window", L"Y", wp.rcNormalPosition.top);
        dn::ini_set_int(ini_path_, L"Window", L"W",
                        wp.rcNormalPosition.right - wp.rcNormalPosition.left);
        dn::ini_set_int(ini_path_, L"Window", L"H",
                        wp.rcNormalPosition.bottom - wp.rcNormalPosition.top);
        dn::ini_set_bool(ini_path_, L"Window", L"Maximized",
                         wp.showCmd == SW_SHOWMAXIMIZED);
    }
    for (int i = 0; i < 2; ++i) {
        const int cw = ListView_GetColumnWidth(dirList_, i);
        if (cw > 0)
            dn::ini_set_int(ini_path_, L"Window", L"DirCol" + std::to_wstring(i), cw);
    }
    for (int i = 0; i < 6; ++i) {
        const int cw = ListView_GetColumnWidth(queue_, i);
        if (cw > 0)
            dn::ini_set_int(ini_path_, L"Window", L"QueueCol" + std::to_wstring(i), cw);
    }
}

void App::OnOptions() {
    if (show_options(hwnd_, hInstance_, config_)) {
        dn::ini_save_state(ini_path_, config_);
        SetStatus(L"Options saved.");
    }
}

void App::Layout(int w, int h) {
    if (w < 100 || h < 100) return;
    const int margin = 8;
    const int btnH = 24;
    const int statusH = 24;

    // Directory listview, with the button row below it (no overlap).
    const int listTop = 28;
    const int listH = 134;
    MoveWindow(dirList_, margin, listTop, w - 2 * margin, listH, TRUE);

    const int btnY = listTop + listH + 4;  // 166: 4px below the listview
    MoveWindow(addBtn_, margin, btnY, 80, btnH, TRUE);
    MoveWindow(removeBtn_, margin + 86, btnY, 80, btnH, TRUE);
    MoveWindow(optionsBtn_, w - 270, btnY, 82, btnH, TRUE);
    MoveWindow(scanBtn_, w - 180, btnY, 82, btnH, TRUE);
    MoveWindow(deleteBtn_, w - 90, btnY, 82, btnH, TRUE);

    // "Matches:" label and queue below the button row.
    const int labelTop = 204;
    MoveWindow(matchesLabel_, margin, labelTop, 200, 16, TRUE);
    const int queueTop = labelTop + 16 + 4;  // 224
    MoveWindow(queue_, margin, queueTop, w - 2 * margin, h - queueTop - statusH, TRUE);

    MoveWindow(status_, margin, h - statusH, w - 2 * margin, statusH - 6, TRUE);
}

void App::AddDir(const std::filesystem::path& p) {
    for (const auto& d : dirs_)
        if (d.path == p)
            return;  // already listed
    dirs_.push_back(dn::DirEntry{p, false});
    RefreshDirList();
    SaveState();  // persist immediately, not only on exit
}

void App::OnAddFolder() {
    IFileDialog* dlg = nullptr;
    if (SUCCEEDED(CoCreateInstance(kClsidFileOpenDialog, nullptr, CLSCTX_INPROC_SERVER,
                                   IID_PPV_ARGS(&dlg)))) {
        DWORD opts = 0;
        dlg->GetOptions(&opts);
        dlg->SetOptions(opts | FOS_PICKFOLDERS | FOS_FORCEFILESYSTEM);
        dlg->SetTitle(L"Select a directory to scan");

        if (SUCCEEDED(dlg->Show(hwnd_))) {
            IShellItem* item = nullptr;
            if (SUCCEEDED(dlg->GetResult(&item))) {
                PWSTR path = nullptr;
                if (SUCCEEDED(item->GetDisplayName(SIGDN_FILESYSPATH, &path))) {
                    AddDir(std::filesystem::path(path));
                    CoTaskMemFree(path);
                }
                item->Release();
            }
        }
        dlg->Release();
        return;
    }

    // Fallback: the modern IFileDialog COM class is not registered on this
    // machine (CoCreateInstance -> REGDB_E_CLASSNOTREG). Use the classic
    // folder browser, which is provided by Shell.Application and is available.
    BROWSEINFOW bi{};
    bi.hwndOwner = hwnd_;
    bi.lpszTitle = L"Select a directory to scan";
    bi.ulFlags = BIF_RETURNONLYFSDIRS | BIF_NEWDIALOGSTYLE;
    LPITEMIDLIST pidl = SHBrowseForFolderW(&bi);
    if (pidl == nullptr)
        return;  // user cancelled
    wchar_t path[MAX_PATH]{};
    const BOOL got = SHGetPathFromIDListW(pidl, path);
    CoTaskMemFree(pidl);
    if (got != FALSE)
        AddDir(std::filesystem::path(path));
}

void App::OnRemoveFolder() {
    const int sel = ListView_GetNextItem(dirList_, -1, LVNI_SELECTED);
    if (sel < 0) return;
    dirs_.erase(dirs_.begin() + sel);
    RefreshDirList();
    SaveState();  // persist immediately, not only on exit
}

void App::OnDirTypeToggle(int index) {
    if (index < 0 || index >= static_cast<int>(dirs_.size())) return;
    dirs_[index].protected_ = !dirs_[index].protected_;
    RefreshDirList();
    SaveState();  // persist immediately, not only on exit
    const std::wstring typeStr = dirs_[index].protected_ ? L"Protected" : L"Common";
    SetStatus(L"Type set to " + typeStr + L".");
}

void App::RefreshDirList() {
    ListView_DeleteAllItems(dirList_);
    for (std::size_t i = 0; i < dirs_.size(); ++i) {
        const auto& d = dirs_[i];
        const std::wstring path = to_wide(d.path.string());
        insert_dir_item(dirList_, static_cast<int>(i), path);
        const std::wstring type = d.protected_ ? L"Protected" : L"Common";
        ListView_SetItemText(dirList_, static_cast<int>(i), 1,
                             const_cast<LPWSTR>(type.c_str()));
    }
}

void App::SetStatus(const std::wstring& text) {
    SetWindowTextW(status_, text.c_str());
}

void App::OnScan() {
    if (dirs_.empty()) {
        SetStatus(L"Add at least one directory, then click Scan.");
        return;
    }

    SetStatus(L"Scanning...");
    UpdateWindow(hwnd_);

    entries_ = dn::scan(dirs_, config_, {});
    clusters_ = dn::match(entries_, config_);
    const int shown = PopulateQueue(clusters_);

    SetStatus(L"Scanned " + std::to_wstring(entries_.size()) + L" file(s); " +
              std::to_wstring(shown) + L" match group(s)");
}

int App::PopulateQueue(const std::vector<dn::Cluster>& clusters) {
    ListView_DeleteAllItems(queue_);
    row_group_.clear();
    row_protected_.clear();
    int row = 0;
    int groupsShown = 0;
    for (std::size_t ci = 0; ci < clusters.size(); ++ci) {
        const auto& cl = clusters[ci];
        if (cl.members.size() < 2) continue;  // a lone file is not a duplicate group
        ++groupsShown;
        // Group number is the stable 1-based cluster index; it does not change
        // when members are deleted (only a rescan renumbers).
        const std::wstring group = std::to_wstring(ci + 1);
        for (const auto idx : cl.members) {
            const auto& e = entries_[idx];
            const std::wstring name = to_wide(e.name);
            const std::wstring folder = to_wide(e.path.parent_path().string());
            const std::wstring path = to_wide(e.path.string());
            const std::wstring drive = drive_of(path);
            const std::wstring size = format_size(e.size);
            const std::wstring modified = format_epoch(e.lastModified);

            LVITEMW item{};
            item.mask = LVIF_TEXT | LVIF_PARAM;
            item.iItem = row;
            item.pszText = const_cast<LPWSTR>(name.c_str());
            item.lParam = static_cast<LPARAM>(idx);  // entry index
            if (ListView_InsertItem(queue_, &item) < 0)
                continue;
            row_group_.push_back(static_cast<int>(ci + 1));  // for group banding
            row_protected_.push_back(e.protected_);          // for checkbox hiding
            ListView_SetItemText(queue_, row, 1, const_cast<LPWSTR>(folder.c_str()));
            ListView_SetItemText(queue_, row, 2, const_cast<LPWSTR>(drive.c_str()));
            ListView_SetItemText(queue_, row, 3, const_cast<LPWSTR>(group.c_str()));
            ListView_SetItemText(queue_, row, 4, const_cast<LPWSTR>(size.c_str()));
            ListView_SetItemText(queue_, row, 5, const_cast<LPWSTR>(modified.c_str()));
            ++row;
        }
    }
    return groupsShown;
}

void App::OnDelete() {
    if (entries_.empty() || clusters_.empty()) {
        SetStatus(L"Nothing to delete. Scan first, then check files to remove.");
        return;
    }

    // Collect the checked rows. With LVS_EX_CHECKBOXES the checkbox state is
    // the row's state image: 1 = unchecked, 2 = checked.
    std::vector<bool> selected(entries_.size(), false);
    const int count = ListView_GetItemCount(queue_);
    for (int i = 0; i < count; ++i) {
        LVITEMW item{};
        item.mask = LVIS_STATEIMAGEMASK | LVIF_PARAM;
        item.iItem = i;
        if (!ListView_GetItem(queue_, &item)) continue;
        const int stateImage = (item.state & LVIS_STATEIMAGEMASK) >> 16;
        if (stateImage == 2) {  // checked
            const std::size_t idx = static_cast<std::size_t>(item.lParam);
            if (idx < selected.size()) selected[idx] = true;
        }
    }

    const dn::DeletePlan plan = dn::plan_deletion(entries_, clusters_, selected);
    if (plan.to_delete.empty()) {
        SetStatus(L"Nothing to delete (protected files and the last copy of each "
                  L"group are kept).");
        return;
    }

    std::wstring msg = L"Delete " + std::to_wstring(plan.to_delete.size()) +
                       L" file(s)? They will be moved to the Recycle Bin where "
                       L"possible.\n\n";
    for (const auto idx : plan.to_delete) msg += to_wide(entries_[idx].path.string()) + L"\n";
    if (!plan.skipped_protected.empty())
        msg += L"\nSkipped (protected): " + std::to_wstring(plan.skipped_protected.size()) +
               L" file(s).";
    if (MessageBoxW(hwnd_, msg.c_str(), L"Delete duplicates", MB_YESNO | MB_ICONWARNING) != IDYES)
        return;

    std::vector<std::wstring> paths;
    paths.reserve(plan.to_delete.size());
    for (const auto idx : plan.to_delete) paths.push_back(to_wide(entries_[idx].path.string()));

    std::wstring error;
    const int deleted = delete_files(paths, error);
    RemoveDeletedItems(plan.to_delete);

    std::wstring status = L"Deleted " + std::to_wstring(deleted) + L" file(s)";
    if (!plan.kept.empty())
        status += L"; kept " + std::to_wstring(plan.kept.size()) + L" (last copy)";
    if (!plan.skipped_protected.empty())
        status += L"; skipped " + std::to_wstring(plan.skipped_protected.size()) + L" protected";
    if (!error.empty()) status += L". Errors: " + error;
    SetStatus(status);
}

void App::RemoveDeletedItems(const std::vector<std::size_t>& deleted) {
    const std::unordered_set<std::size_t> deleted_set(deleted.begin(), deleted.end());

    // Reflect the deletion in the cluster model so later deletions apply the
    // keep-one rule against the surviving members. Cluster indices are left
    // untouched, so group numbers stay stable until the next scan.
    for (auto& cl : clusters_)
        cl.members.erase(
            std::remove_if(cl.members.begin(), cl.members.end(),
                           [&](std::size_t m) { return deleted_set.count(m) != 0; }),
            cl.members.end());

    // Re-populate the list view: groups that lost all but one member (or all
    // members) are skipped, and the remaining rows keep their group numbers.
    PopulateQueue(clusters_);
}

std::size_t App::EntryIndexForRow(int row) const {
    // Returns the entries_ index stored in the row's lParam, or entries_.size()
    // as an invalid sentinel when the row is out of range or has no entry.
    if (row < 0 || row >= ListView_GetItemCount(queue_)) return entries_.size();
    LVITEMW item{};
    item.mask = LVIF_PARAM;
    item.iItem = row;
    if (!ListView_GetItem(queue_, &item)) return entries_.size();
    const std::size_t idx = static_cast<std::size_t>(item.lParam);
    return idx < entries_.size() ? idx : entries_.size();
}

void App::OnQueueContextMenu() {
    if (ListView_GetItemCount(queue_) == 0) return;

    // Hit-test the row under the cursor (screen -> client coordinates).
    POINT pt{};
    GetCursorPos(&pt);
    POINT client{pt.x, pt.y};
    ScreenToClient(queue_, &client);
    LVHITTESTINFO ht{};
    ht.pt = client;
    const int row = ListView_SubItemHitTest(queue_, &ht);
    if (row < 0) return;
    context_row_ = row;

    HMENU menu = CreatePopupMenu();
    if (!menu) return;
    AppendMenuW(menu, MF_STRING, kIdCtxOpenFile, L"Open File");
    AppendMenuW(menu, MF_STRING, kIdCtxOpenFolder, L"Open Folder in Explorer");
    AppendMenuW(menu, MF_STRING, kIdCtxRename, L"Rename");
    // TPM_RIGHTBUTTON: the menu was opened by a right-click, so it dismisses
    // when the right button is released.
    TrackPopupMenu(menu, TPM_RIGHTBUTTON, pt.x, pt.y, 0, hwnd_, nullptr);
    DestroyMenu(menu);
}

void App::OnOpenFile() {
    const std::size_t idx = EntryIndexForRow(context_row_);
    if (idx >= entries_.size()) return;
    const std::wstring path = to_wide(entries_[idx].path.string());
    // ShellExecuteW returns an HINSTANCE > 32 on success, an error code <= 32
    // on failure (e.g. 2 = file not found, 3 = access denied).
    HINSTANCE res = ShellExecuteW(hwnd_, L"open", path.c_str(), nullptr, nullptr,
                                  SW_SHOWNORMAL);
    if (reinterpret_cast<INT_PTR>(res) <= 32)
        SetStatus(L"Could not open " + path + L".");
    else
        SetStatus(L"Opened " + path + L".");
}

void App::OnOpenFolder() {
    const std::size_t idx = EntryIndexForRow(context_row_);
    if (idx >= entries_.size()) return;
    const std::wstring path = to_wide(entries_[idx].path.string());
    // Open the file's folder in Explorer with the file itself selected, so it
    // can be examined at a glance.
    const std::wstring args = L"/select,\"" + path + L"\"";
    HINSTANCE res = ShellExecuteW(hwnd_, L"open", L"explorer.exe", args.c_str(),
                                  nullptr, SW_SHOWNORMAL);
    if (reinterpret_cast<INT_PTR>(res) <= 32)
        SetStatus(L"Could not open the folder in Explorer.");
    else
        SetStatus(L"Opened the folder in Explorer.");
}

void App::OnRenameFile() {
    const std::size_t idx = EntryIndexForRow(context_row_);
    if (idx >= entries_.size()) return;
    const std::wstring current = to_wide(entries_[idx].name);

    std::wstring new_name;
    if (!show_rename_dialog(hwnd_, hInstance_, current, new_name))
        return;  // user cancelled

    // Validate the new name: non-empty, no characters illegal in a file name.
    if (new_name.empty()) {
        SetStatus(L"Rename cancelled: the file name is empty.");
        return;
    }
    static const wchar_t kInvalid[] = L"\\/:*?\"<>|";
    for (const wchar_t c : new_name) {
        if (wcschr(kInvalid, c) != nullptr) {
            SetStatus(L"Rename cancelled: the file name contains invalid characters.");
            return;
        }
    }

    const std::wstring target =
        to_wide(entries_[idx].path.parent_path().string()) + L"\\" + new_name;
    std::error_code ec;
    if (std::filesystem::exists(target, ec)) {
        SetStatus(L"Rename cancelled: " + new_name + L" already exists in that folder.");
        return;
    }

    std::filesystem::rename(entries_[idx].path, target, ec);  // void + error_code
    if (ec) {
        SetStatus(L"Rename failed: " + to_wide(ec.message()));
        return;
    }

    // Update the model and the row's File Name cell. The match results are now
    // stale for this file until the next scan.
    entries_[idx].path = target;
    entries_[idx].name = to_narrow(new_name);
    LVITEMW item{};
    item.mask = LVIF_TEXT;
    item.iItem = context_row_;
    item.pszText = const_cast<LPWSTR>(new_name.c_str());
    ListView_SetItem(queue_, &item);
    SetStatus(L"Renamed to " + new_name + L".");
}

}  // namespace gui

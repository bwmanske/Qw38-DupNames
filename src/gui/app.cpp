#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include "app.hpp"

#include <objbase.h>
#include <shobjidl.h>
#include <commctrl.h>

#include <algorithm>
#include <filesystem>
#include <string>
#include <utility>

#include "dn/ini.hpp"
#include "dn/match.hpp"
#include "dn/scanner.hpp"

#include "options.hpp"

namespace gui {

namespace {

constexpr wchar_t kClass[] = L"DNMainWnd";
constexpr INT_PTR kIdAdd = 1001;
constexpr INT_PTR kIdRemove = 1002;
constexpr INT_PTR kIdScan = 1003;
constexpr INT_PTR kIdOptions = 1007;
constexpr INT_PTR kIdDirList = 1004;
constexpr INT_PTR kIdQueue = 1005;
constexpr INT_PTR kIdStatus = 1006;
constexpr int kTopPanelH = 180;  // height of the directory panel

// CLSID_FileOpenDialog (avoids relying on the SDK exporting the CLSID symbol).
const GUID kClsidFileOpenDialog = {0x43826d1e, 0xe718, 0x42ee,
                                   {0xbc, 0x55, 0xa1, 0xe2, 0x61, 0xc3, 0x7b, 0xfe}};

std::wstring to_wide(const std::string& s) {
    if (s.empty()) return {};
    const int n = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), static_cast<int>(s.size()), nullptr, 0);
    std::wstring out(static_cast<size_t>(n), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, s.c_str(), static_cast<int>(s.size()), out.data(), n);
    return out;
}

void insert_dir_item(HWND list, int index, const std::wstring& text) {
    LVITEMW item{};
    item.mask = LVIF_TEXT;
    item.iItem = index;
    item.pszText = const_cast<LPWSTR>(text.c_str());
    ListView_InsertItem(list, &item);
}

HTREEITEM insert_tree_item(HWND tree, HTREEITEM parent, const std::wstring& text) {
    TVITEMW item{};
    item.mask = TVIF_TEXT;
    item.hItem = parent;  // TVI_ROOT for a top-level item, else the parent item
    item.pszText = const_cast<LPWSTR>(text.c_str());
    return TreeView_InsertItem(tree, &item);
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

    hwnd_ = CreateWindowExW(
        0, kClass, L"DupNames", WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT, 900, 640,
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

    ShowWindow(hwnd_, nCmdShow);
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
            if (HIWORD(wp) == BN_CLICKED) {
                if (id == kIdAdd) OnAddFolder();
                else if (id == kIdRemove) OnRemoveFolder();
                else if (id == kIdScan) OnScan();
                else if (id == kIdOptions) OnOptions();
            }
            return 0;
        }
        case WM_SIZE:
            Layout(LOWORD(lp), HIWORD(lp));
            return 0;
        case WM_DESTROY:
            SaveState();
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

    CreateWindowExW(0, L"STATIC", L"Matches:", WS_CHILD | WS_VISIBLE,
                    8, 188, 200, 16, hwnd_, nullptr, hInstance_, nullptr);

    queue_ = CreateWindowExW(WS_EX_CLIENTEDGE, WC_TREEVIEWW, L"",
                             WS_CHILD | WS_VISIBLE | TVS_HASLINES | TVS_HASBUTTONS |
                                 TVS_SHOWSELALWAYS,
                             8, 208, 400, 300, hwnd_, reinterpret_cast<HMENU>(kIdQueue),
                             hInstance_, nullptr);

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
}

void App::OnOptions() {
    if (show_options(hwnd_, hInstance_, config_)) {
        dn::ini_save_state(ini_path_, config_);
        SetStatus(L"Options saved.");
    }
}

void App::Layout(int w, int h) {
    if (w < 100 || h < 100) return;
    const int queueTop = kTopPanelH + 28;
    const int statusH = 24;
    MoveWindow(dirList_, 8, 28, w - 16, kTopPanelH - 40, TRUE);
    MoveWindow(addBtn_, 8, kTopPanelH - 30, 80, 24, TRUE);
    MoveWindow(removeBtn_, 94, kTopPanelH - 30, 80, 24, TRUE);
    MoveWindow(optionsBtn_, w - 180, kTopPanelH - 30, 82, 24, TRUE);
    MoveWindow(scanBtn_, w - 90, kTopPanelH - 30, 82, 24, TRUE);
    MoveWindow(queue_, 8, queueTop, w - 16, h - queueTop - statusH, TRUE);
    MoveWindow(status_, 8, h - statusH, w - 16, statusH - 6, TRUE);
}

void App::OnAddFolder() {
    IFileDialog* dlg = nullptr;
    if (FAILED(CoCreateInstance(kClsidFileOpenDialog, nullptr, CLSCTX_INPROC_SERVER,
                                IID_PPV_ARGS(&dlg))))
        return;

    DWORD opts = 0;
    dlg->GetOptions(&opts);
    dlg->SetOptions(opts | FOS_PICKFOLDERS | FOS_FORCEFILESYSTEM);
    dlg->SetTitle(L"Select a directory to scan");

    if (SUCCEEDED(dlg->Show(hwnd_))) {
        IShellItem* item = nullptr;
        if (SUCCEEDED(dlg->GetResult(&item))) {
            PWSTR path = nullptr;
            if (SUCCEEDED(item->GetDisplayName(SIGDN_FILESYSPATH, &path))) {
                const std::filesystem::path p(path);
                bool exists = false;
                for (const auto& d : dirs_)
                    if (d.path == p) {
                        exists = true;
                        break;
                    }
                if (!exists) {
                    dirs_.push_back(dn::DirEntry{p, false});
                    RefreshDirList();
                }
                CoTaskMemFree(path);
            }
            item->Release();
        }
    }
    dlg->Release();
}

void App::OnRemoveFolder() {
    const int sel = ListView_GetNextItem(dirList_, -1, LVNI_SELECTED);
    if (sel < 0) return;
    dirs_.erase(dirs_.begin() + sel);
    RefreshDirList();
}

void App::RefreshDirList() {
    ListView_DeleteAllItems(dirList_);
    for (std::size_t i = 0; i < dirs_.size(); ++i) {
        const auto& d = dirs_[i];
        const std::wstring label = to_wide(d.path.string()) +
                                   (d.protected_ ? L"  [protected]" : L"");
        insert_dir_item(dirList_, static_cast<int>(i), label);
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
    const auto clusters = dn::match(entries_, config_);
    PopulateQueue(clusters);

    SetStatus(L"Scanned " + std::to_wstring(entries_.size()) + L" file(s); " +
              std::to_wstring(clusters.size()) + L" match group(s).");
}

void App::PopulateQueue(const std::vector<dn::Cluster>& clusters) {
    TreeView_DeleteAllItems(queue_);
    for (const auto& cl : clusters) {
        const auto& anchor = entries_[cl.anchor];
        const std::wstring header = to_wide(anchor.name) + L"  (" +
                                    std::to_wstring(cl.members.size()) + L")";
        HTREEITEM parent = insert_tree_item(queue_, TVI_ROOT, header);
        for (const auto idx : cl.members) {
            const auto& e = entries_[idx];
            const std::wstring child = to_wide(e.name) + L"  -  " +
                                       to_wide(e.path.parent_path().string());
            insert_tree_item(queue_, parent, child);
        }
    }
    TreeView_Expand(queue_, TVI_ROOT, TVE_EXPAND);
}

}  // namespace gui

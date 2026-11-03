#pragma once

#include <windows.h>
#include <commctrl.h>  // HIMAGELIST (queue checkbox state list)

#include <string>
#include <vector>

#include "dn/cli.hpp"
#include "dn/types.hpp"

namespace gui {

// The DupNames main window: a directory list (loaded from and saved to the INI
// path list), a Scan action, and a flat list view of MATCH clusters (one row
// per cluster member, grouped by stable 1-based group number).
// Thresholds come from the INI [InitState] with command-line overrides.
class App {
public:
    App(HINSTANCE hInstance, const dn::CliArgs& cli);

    // Register classes, create + show the window, and run the message loop.
    // Returns the process exit code.
    int Run(int nCmdShow);

private:
    static LRESULT CALLBACK WndProcThunk(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp);
    LRESULT WndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp);

    void CreateControls();
    void LoadState();   // resolve config (INI + CLI) and load the path list
    void SaveState();   // persist the current path list to the INI
    void LoadWindowLayout();  // read [Window] (rect + column widths) from the INI
    void SaveWindowLayout();  // write [Window] (rect + column widths) to the INI
    void Layout(int w, int h);
    void OnAddFolder();
    void AddDir(const std::filesystem::path& p);  // add if not already listed
    void OnRemoveFolder();
    void OnDirTypeToggle(int index);  // click on Type column: Common <-> Protected
    void OnScan();
    void OnOptions();
    void OnDelete();
    void RefreshDirList();
    void SetStatus(const std::wstring& text);
    int PopulateQueue(const std::vector<dn::Cluster>& clusters);  // returns # groups shown
    void RemoveDeletedItems(const std::vector<std::size_t>& deleted);

    // Right-click context menu on the matches list.
    void OnQueueContextMenu();  // hit-test the row and show the popup menu
    void OnOpenFile();          // open the row's file with its default app
    void OnOpenFolder();        // open the row's folder in Explorer (file selected)
    void OnRenameFile();        // rename the row's file (dialog + filesystem rename)
    void OnCopyFilesToClipboard();  // selected rows' full paths, one per line
    void OnCopyCsvToClipboard();    // selected rows as CSV (all columns)
    std::size_t EntryIndexForRow(int row) const;  // entry index for a queue row
    void ToggleRowCheckbox(int row);  // flip a non-protected row's checkbox state
    std::vector<int> SelectedRows() const;  // currently selected queue rows

    HINSTANCE hInstance_ = nullptr;
    HWND hwnd_ = nullptr;
    HWND dirList_ = nullptr;  // SysListView32: added directories
    HWND queue_ = nullptr;    // SysListView32: match clusters (flat, one row per member)
    HWND addBtn_ = nullptr;
    HWND removeBtn_ = nullptr;
    HWND scanBtn_ = nullptr;
    HWND optionsBtn_ = nullptr;
    HWND deleteBtn_ = nullptr;
    HWND matchesLabel_ = nullptr;  // "Matches:" static above the queue
    HWND status_ = nullptr;

    dn::CliArgs cli_;                  // parsed command-line options
    std::wstring ini_path_;            // INI in use (--ini FILE or the default)
    dn::Config config_;                // effective config (default <- INI <- CLI)
    std::vector<dn::DirEntry> dirs_;   // path list (loaded from / saved to INI)
    std::vector<dn::FileEntry> entries_;  // last scan results
    std::vector<dn::Cluster> clusters_;   // last scan's clusters (for deletion)

    // [Window] layout persistence (restored in LoadWindowLayout, saved in
    // SaveWindowLayout). Column widths default to the CreateControls values.
    RECT window_rect_{};               // restored window rect (if valid)
    bool have_window_rect_ = false;
    bool restore_maximized_ = false;
    int dir_col_widths_[2] = {600, 100};
    int queue_col_widths_[7] = {280, 290, 55, 50, 70, 130, 60};
    std::vector<int> row_group_;       // 1-based group number per queue row (banding)
    std::vector<bool> row_protected_;  // true if the row's file is in a protected dir
    int context_row_ = -1;             // queue row targeted by the context menu
    HIMAGELIST stateList_ = nullptr;   // queue checkbox state list (blank/unchecked/checked)
};

}  // namespace gui

#pragma once

#include <windows.h>

#include <string>
#include <vector>

#include "dn/cli.hpp"
#include "dn/types.hpp"

namespace gui {

// The DupNames main window: a directory list (loaded from and saved to the INI
// path list), a Scan action, and a queue (tree view) of MATCH clusters.
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
    void Layout(int w, int h);
    void OnAddFolder();
    void OnRemoveFolder();
    void OnScan();
    void RefreshDirList();
    void SetStatus(const std::wstring& text);
    void PopulateQueue(const std::vector<dn::Cluster>& clusters);

    HINSTANCE hInstance_ = nullptr;
    HWND hwnd_ = nullptr;
    HWND dirList_ = nullptr;  // SysListView32: added directories
    HWND queue_ = nullptr;    // SysTreeView32: match clusters
    HWND addBtn_ = nullptr;
    HWND removeBtn_ = nullptr;
    HWND scanBtn_ = nullptr;
    HWND status_ = nullptr;

    dn::CliArgs cli_;                  // parsed command-line options
    std::wstring ini_path_;            // INI in use (--ini FILE or the default)
    dn::Config config_;                // effective config (default <- INI <- CLI)
    std::vector<dn::DirEntry> dirs_;   // path list (loaded from / saved to INI)
    std::vector<dn::FileEntry> entries_;  // last scan results
};

}  // namespace gui

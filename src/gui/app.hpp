#pragma once

#include <windows.h>

#include <vector>

#include "dn/types.hpp"

namespace gui {

// The DupNames main window (M1 vertical slice): an in-memory directory list,
// a Scan action, and a queue (tree view) of MATCH clusters. No persistence yet
// (path-list / INI is P4).
class App {
public:
    explicit App(HINSTANCE hInstance);

    // Register classes, create + show the window, and run the message loop.
    // Returns the process exit code.
    int Run(int nCmdShow);

private:
    static LRESULT CALLBACK WndProcThunk(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp);
    LRESULT WndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp);

    void CreateControls();
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

    std::vector<dn::DirEntry> dirs_;    // in-memory path list (no persistence yet)
    std::vector<dn::FileEntry> entries_;  // last scan results
};

}  // namespace gui

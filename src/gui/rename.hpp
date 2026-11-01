#pragma once

#include <windows.h>

#include <string>

namespace gui {

// Modal rename dialog: a single edit box pre-filled with `current_name`.
// Returns true if the user accepted, with the new file name in `new_name`.
// Returns false if the user cancelled (or the dialog could not be created).
bool show_rename_dialog(HWND owner, HINSTANCE hInstance,
                        const std::wstring& current_name, std::wstring& new_name);

}  // namespace gui

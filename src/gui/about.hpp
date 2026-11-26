#pragma once

#include <windows.h>

namespace gui {

// Show a modal About dialog owned by `owner`: the app image (the embedded
// DeDupNames_128.png) centered at the top, a "Build Information" heading with
// the project version and the build date/time (left justified), a blank line,
// and a brief description of the app.
void show_about(HWND owner, HINSTANCE hInstance);

}  // namespace gui

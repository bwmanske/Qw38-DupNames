#pragma once

#include <windows.h>

#include "dn/types.hpp"

namespace gui {

// Show a modal Options dialog owned by `owner`. The dialog is populated from
// `cfg`; if the user clicks OK (after validation passes) the edited values are
// written back into `cfg` and true is returned. If the user cancels or closes
// the dialog, `cfg` is left unchanged and false is returned. The caller is
// responsible for persisting `cfg` to the INI when true is returned.
bool show_options(HWND owner, HINSTANCE hInstance, dn::Config& cfg);

}  // namespace gui

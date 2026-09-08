#include <windows.h>

#include "gui/app.hpp"

int APIENTRY wWinMain(HINSTANCE hInstance, HINSTANCE, PWSTR, int nCmdShow) {
    gui::App app(hInstance);
    return app.Run(nCmdShow);
}

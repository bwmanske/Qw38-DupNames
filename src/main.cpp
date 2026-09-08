#include <windows.h>
#include <shellapi.h>

#include <vector>

#include "dn/cli.hpp"
#include "gui/app.hpp"

int APIENTRY wWinMain(HINSTANCE hInstance, HINSTANCE, PWSTR, int nCmdShow) {
    // Split the command line into tokens (handles quoting), dropping the
    // program name, then parse the DupNames options.
    int argc = 0;
    LPWSTR* argv = CommandLineToArgvW(GetCommandLineW(), &argc);
    std::vector<std::wstring> args;
    if (argv) {
        args.assign(argv + 1, argv + argc);
        LocalFree(argv);
    }

    const dn::CliArgs cli = dn::parse_cli(args);
    if (cli.error) {
        const std::wstring msg =
            cli.error_msg +
            L"\n\nUsage: DupNames.exe [--ini FILE] [--match VALUE] [--close VALUE]";
        MessageBoxW(nullptr, msg.c_str(), L"DupNames", MB_OK | MB_ICONWARNING);
        return 1;
    }

    gui::App app(hInstance, cli);
    return app.Run(nCmdShow);
}

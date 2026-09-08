#include <doctest.h>

#include <atomic>
#include <filesystem>
#include <string>
#include <system_error>
#include <vector>

#include "dn/ini.hpp"

using namespace dn;

namespace {

// A unique temp INI file, removed on scope exit (even if a CHECK/REQUIRE fails).
struct TempIni {
    std::wstring path;
    TempIni() {
        static std::atomic<int> n{0};
        const auto dir = std::filesystem::temp_directory_path();
        path = (dir / (L"dn_test_ini_" + std::to_wstring(n++) + L".ini")).wstring();
    }
    ~TempIni() {
        std::error_code ec;
        std::filesystem::remove(path, ec);
    }
};

}  // namespace

TEST_CASE("ini: round-trip state thresholds") {
    TempIni t;
    ini_set_double(t.path, L"InitState", L"MatchThreshold", 0.9);
    ini_set_double(t.path, L"InitState", L"CloseThreshold", 0.7);
    CHECK(ini_get_double(t.path, L"InitState", L"MatchThreshold", 0.0) == doctest::Approx(0.9));
    CHECK(ini_get_double(t.path, L"InitState", L"CloseThreshold", 0.0) == doctest::Approx(0.7));
}

TEST_CASE("ini: missing file returns defaults") {
    const std::wstring missing = L"C:\\definitely\\not\\a\\real\\path\\x.ini";
    CHECK(ini_get_double(missing, L"InitState", L"MatchThreshold", 0.85) == doctest::Approx(0.85));
    CHECK(ini_get_string(missing, L"PathList", L"CommonPath1", L"fallback") == L"fallback");
    CHECK(ini_section_keys(missing, L"PathList").empty());
}

TEST_CASE("ini: absent key in a present section returns default") {
    TempIni t;
    ini_set_double(t.path, L"InitState", L"MatchThreshold", 0.5);
    // CloseThreshold was never written.
    CHECK(ini_get_double(t.path, L"InitState", L"CloseThreshold", 0.6) == doctest::Approx(0.6));
}

TEST_CASE("ini: section keys are enumerated in file order") {
    TempIni t;
    ini_set_string(t.path, L"PathList", L"CommonPath1", L"E:\\a");
    ini_set_string(t.path, L"PathList", L"ProtectedPath1", L"D:\\b");
    const auto keys = ini_section_keys(t.path, L"PathList");
    REQUIRE(keys.size() == 2);
    CHECK(keys[0] == L"CommonPath1");
    CHECK(keys[1] == L"ProtectedPath1");
}

TEST_CASE("ini: non-contiguous numbering is preserved and ordered") {
    TempIni t;
    ini_set_string(t.path, L"PathList", L"ProtectedPath3", L"D:\\three");
    ini_set_string(t.path, L"PathList", L"ProtectedPath1", L"D:\\one");
    const auto dirs = ini_load_paths(t.path);
    REQUIRE(dirs.size() == 2);
    CHECK(dirs[0].path.wstring() == L"D:\\one");
    CHECK(dirs[1].path.wstring() == L"D:\\three");
    CHECK(dirs[0].protected_);
    CHECK(dirs[1].protected_);
}

TEST_CASE("ini: UNC paths round-trip") {
    TempIni t;
    ini_set_string(t.path, L"PathList", L"CommonPath1", L"\\\\fileserver\\share\\inbox");
    ini_set_string(t.path, L"PathList", L"ProtectedPath1", L"\\\\fileserver\\archive\\protected");
    const auto dirs = ini_load_paths(t.path);
    REQUIRE(dirs.size() == 2);
    CHECK(dirs[0].path.wstring() == L"\\\\fileserver\\archive\\protected");
    CHECK(dirs[0].protected_);
    CHECK(dirs[1].path.wstring() == L"\\\\fileserver\\share\\inbox");
    CHECK_FALSE(dirs[1].protected_);
}

TEST_CASE("ini: load/save paths round-trip with protection flags") {
    TempIni t;
    const std::vector<DirEntry> in = {
        DirEntry{std::filesystem::path(L"D:\\Media"), true},
        DirEntry{std::filesystem::path(L"E:\\Downloads"), false},
        DirEntry{std::filesystem::path(L"F:\\More"), false},
    };
    ini_save_paths(t.path, in);
    const auto out = ini_load_paths(t.path);
    REQUIRE(out.size() == 3);
    CHECK(out[0].path.wstring() == L"D:\\Media");
    CHECK(out[0].protected_);
    CHECK(out[1].path.wstring() == L"E:\\Downloads");
    CHECK_FALSE(out[1].protected_);
    CHECK(out[2].path.wstring() == L"F:\\More");
    CHECK_FALSE(out[2].protected_);
}

TEST_CASE("ini: save removes stale keys") {
    TempIni t;
    ini_save_paths(t.path, {DirEntry{std::filesystem::path(L"A"), false},
                            DirEntry{std::filesystem::path(L"B"), false},
                            DirEntry{std::filesystem::path(L"C"), false}});
    ini_save_paths(t.path, {DirEntry{std::filesystem::path(L"Z"), false}});
    const auto out = ini_load_paths(t.path);
    REQUIRE(out.size() == 1);
    CHECK(out[0].path.wstring() == L"Z");
    CHECK(ini_section_keys(t.path, L"PathList").size() == 1);
}

TEST_CASE("ini: default path lives under the roaming app data folder") {
    const std::wstring p = default_ini_path();
    CHECK_FALSE(p.empty());
    CHECK(p.size() > std::wstring(L"\\DupNames\\DupNames.ini").size());
    CHECK(p.compare(p.size() - std::wstring(L"\\DupNames\\DupNames.ini").size(),
                    std::wstring(L"\\DupNames\\DupNames.ini").size(),
                    L"\\DupNames\\DupNames.ini") == 0);
}

TEST_CASE("ini: resolve_config precedence default < INI < CLI") {
    TempIni t;
    // 1) No INI, no CLI -> built-in defaults.
    {
        const auto cfg = resolve_config(t.path, CliArgs{});
        CHECK(cfg.match_threshold == doctest::Approx(0.85));
        CHECK(cfg.close_threshold == doctest::Approx(0.60));
    }
    // 2) INI present, no CLI -> INI wins over default.
    ini_set_double(t.path, L"InitState", L"MatchThreshold", 0.77);
    {
        const auto cfg = resolve_config(t.path, CliArgs{});
        CHECK(cfg.match_threshold == doctest::Approx(0.77));
        CHECK(cfg.close_threshold == doctest::Approx(0.60));  // not in INI -> default
    }
    // 3) CLI present -> CLI wins over INI.
    {
        CliArgs cli;
        cli.has_match = true;
        cli.match = 0.95;
        const auto cfg = resolve_config(t.path, cli);
        CHECK(cfg.match_threshold == doctest::Approx(0.95));
    }
}

TEST_CASE("ini: resolve_config writes CLI thresholds back to the INI") {
    TempIni t;
    CliArgs cli;
    cli.has_match = true;
    cli.match = 0.9;
    cli.has_close = true;
    cli.close = 0.7;
    resolve_config(t.path, cli);
    // The values are now persisted in the INI file itself.
    CHECK(ini_get_double(t.path, L"InitState", L"MatchThreshold", 0.0) == doctest::Approx(0.9));
    CHECK(ini_get_double(t.path, L"InitState", L"CloseThreshold", 0.0) == doctest::Approx(0.7));
}

TEST_CASE("ini: resolve_config write-back targets the given INI only") {
    TempIni a;
    TempIni b;
    CliArgs cli;
    cli.has_match = true;
    cli.match = 0.42;
    cli.ini_path = a.path;
    resolve_config(a.path, cli);
    // `a` got the value; `b` (a different file) is untouched.
    CHECK(ini_get_double(a.path, L"InitState", L"MatchThreshold", 0.0) == doctest::Approx(0.42));
    CHECK(ini_get_double(b.path, L"InitState", L"MatchThreshold", 0.0) == 0.0);
}

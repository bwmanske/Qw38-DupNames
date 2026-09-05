#include <doctest.h>

#include <string>
#include <vector>

#include "dn/compare.hpp"
#include "dn/normalize.hpp"
#include "dn/types.hpp"

using namespace dn;

// The 19 acceptance cases from SPEC section 8. Each is normalized and scored
// with the default config; the resulting class must match the expectation.
// Case 11 is specified as "NO MATCH or CLOSE".
TEST_CASE("acceptance: SPEC section 8 (19 cases)") {
    struct C {
        const char* a;
        const char* b;
        MatchClass exp;
        bool close_ok;  // true for the "NO MATCH or CLOSE" case
    };
    const std::vector<C> cases = {
        {"The Matrix (1999)", "the.matrix.1999.extended.1080p.x264", MatchClass::Match, false},
        {"Inception (2010)", "inception.2010.1080p.x264", MatchClass::Match, false},
        {"The Shawshank Redemption (1994)", "shawshank.redemption.1994", MatchClass::Match, false},
        {"The Godfather (1972)", "thegodfater.1972", MatchClass::Match, false},
        {"The Matrix (1999)", "the.matrix.reloaded.2003", MatchClass::NoMatch, false},
        {"Jurassic Park (1993)", "jurassic.park.1993", MatchClass::Match, false},
        {"Blade Runner (1982)", "blade.runner.1982.directors.cut", MatchClass::Match, false},
        {"Blade Runner (1982)", "blade.runner.2017", MatchClass::NoMatch, false},
        {"Interstellar (2014)", "interstellar.2014", MatchClass::Match, false},
        {"Interstellar (2014)", "interstellar.2014.1080p", MatchClass::Match, false},
        {"The Dark Knight (2008)", "dark.knight.rises.2012", MatchClass::NoMatch, true},
        {"Pulp Fiction (1994)", "pulp.fiction.1994", MatchClass::Match, false},
        {"Pulp Fiction (1994)", "pulp.fiction.1994.720p.bluray", MatchClass::Match, false},
        {"Fight Club (1999)", "fight.club.1999", MatchClass::Match, false},
        {"Fight Club (1999)", "fight.club.1999.1080p.x264", MatchClass::Match, false},
        {"The Godfather (1972)", "the.godfather.part.ii.1974", MatchClass::NoMatch, false},
        {"Am\xc3\xa9lie (2001)", "amelie.2001", MatchClass::Match, false},
        {"L\xc3\xa9on: The Professional (1994)", "leon.the.professional.1994", MatchClass::Match, false},
        {"na\xc3\xafve.error.1993", "Naive Error (1993)", MatchClass::Match, false},
    };

    const Config cfg;
    int idx = 0;
    for (const auto& c : cases) {
        ++idx;
        const auto na = normalize(c.a, cfg);
        const auto nb = normalize(c.b, cfg);
        const auto ps = score_pair(na, nb, cfg);
        const bool ok = (ps.class_ == c.exp) || (c.close_ok && ps.class_ == MatchClass::Close);
        if (!ok) {
            MESSAGE("case " << idx << ": '" << c.a << "' vs '" << c.b << "' -> got "
                            << static_cast<int>(ps.class_) << " (score " << ps.score
                            << ", token " << ps.token_score << ", year " << ps.year_score
                            << ", cap " << (ps.year_cap_applied ? 1 : 0) << ")");
        }
        CHECK(ok);
    }
}

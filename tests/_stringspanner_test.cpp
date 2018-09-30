#include "catch.hpp"
#include "../csvmonkey.hpp"


TEST_CASE(PREFIX "initialNullTerminates", "[stringspanner]")
{
    // PCMPISTRI returns 16 to indicate null encountered.
    const char *x = "\x00this,should,never,be,reached";
    csvmonkey::StringSpanner ss(',');
    REQUIRE(ss(x) == 16);
}


TEST_CASE(PREFIX "midNullTerminates", "[stringspanner]")
{
    // PCMPISTRI returns 16 to indicate null encountered.
    const char *x = "derp\x00this,should,never,be,reached";
    csvmonkey::StringSpanner ss(',');
    REQUIRE(ss(x) == 16);
}

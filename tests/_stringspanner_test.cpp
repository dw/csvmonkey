#include <string>

#include "catch.hpp"
#include "csvmonkey.hpp"


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


TEST_CASE(PREFIX "noMatchTerminates0", "[stringspanner]")
{
    // Comma not found.
    const char *x = "derpderpderpderpderp";
    csvmonkey::StringSpanner ss(',');
    REQUIRE(ss(x) == 16);
}


TEST_CASE(PREFIX "noMatchTerminates1", "[stringspanner]")
{
    // No terminator specified.
    const char *x = "derpderpderpderpderp";
    csvmonkey::StringSpanner ss;
    REQUIRE(ss(x) == 16);
}


TEST_CASE(PREFIX "matchAtEachOffset", "[stringspanner]")
{
    const char *x = "derpderpderpderpderp";
    for(int i = 0; i < 16; i++) {
        std::string s(x);
        s[i] = ',';
        INFO("i = " << i);
        csvmonkey::StringSpanner ss(',');
        REQUIRE(ss(s.c_str()) == i);
    }
}


TEST_CASE(PREFIX "matchPos16", "[stringspanner]")
{
    const char *x = "derpderpderpderpderp";
    std::string s(x);
    s[16] = ',';
    csvmonkey::StringSpanner ss(',');
    REQUIRE(ss(s.c_str()) == 16);
}


TEST_CASE(PREFIX "matchPos17", "[stringspanner]")
{
    const char *x = "derpderpderpderpderp";
    std::string s(x);
    s[17] = ',';
    csvmonkey::StringSpanner ss(',');
    REQUIRE(ss(s.c_str()) == 16);
}

// utils/decimal/fixedPoint.hpp: exact decimal-string <-> 1e-8 fixed point, for
// int64 and __int128, plus readFixed on real simdjson fields.
#include <utils/decimal/fixedPoint.hpp>
#include <iostream>
#include <string>

using namespace AstraConnect::Utils;

static int failures = 0;
#define CHECK(cond, msg) do { if(!(cond)) { failures++; std::cout << "FAIL " << __LINE__ << ": " << msg << "\n"; } } while(0)

static void parses(const char* text, Fixed expected) {
    Fixed v = -12345;
    CHECK(parseFixed(text, v) && v == expected, text << " -> " << v << ", expected " << expected);
}

static void rejects(const char* text) {
    Fixed v = -12345;
    CHECK(!parseFixed(text, v) && v == -12345, text << " should be rejected");
}

static void roundTrips(const char* text) {
    Fixed v;
    CHECK(parseFixed(text, v) && formatFixed(v) == text, text << " -> " << formatFixed(v));
}

int main() {
    parses("84523.1", 8452310000000);
    parses("0.001", 100000);
    parses("0.00000001", 1);
    parses("0", 0);
    parses("-0.5", -50000000);
    parses("+2", 200000000);
    parses("43187.00", 4318700000000);
    parses("5.", 500000000);
    parses(".5", 50000000);
    parses("0.0000000000", 0);                       // trailing zeros past 8 decimals
    parses("7.71236425000", 771236425);
    // beyond 8 decimals: round half away from zero on the 9th
    parses("0.000000014", 1);
    parses("0.000000015", 2);
    parses("-0.000000015", -2);
    parses("0.0000000149999", 1);
    // int64 limits: 92233720368.54775807 is INT64_MAX in 1e-8 units
    parses("92233720368.54775807", INT64_MAX);
    parses("-92233720368.54775808", INT64_MIN);
    rejects("92233720368.54775808");
    rejects("92233720368.547758075");                // rounds up past the limit
    rejects("100000000000");
    rejects("");
    rejects("-");
    rejects(".");
    rejects("1.2.3");
    rejects("1e5");
    rejects("abc");
    rejects(" 1");

    roundTrips("84523.1");
    roundTrips("0.0042149");
    roundTrips("0");
    roundTrips("-0.5");
    roundTrips("92233720368.54775807");
    roundTrips("-92233720368.54775808");
    CHECK(formatFixed(Fixed(100000000)) == "1", formatFixed(Fixed(100000000)));

    // 128-bit aggregates: values that overflow int64 in 1e-8 units
    Fixed128 big = 0;
    CHECK(parseFixed("413772355107.94400000", big) && formatFixed(big) == "413772355107.944", "CMC supply " << formatFixed(big));
    CHECK(parseFixed("35857834142", big) && big == Fixed128(35857834142) * FIXED_SCALE, "PEPE daily volume");
    Fixed small;
    CHECK(!parseFixed("413772355107.944", small), "must not fit int64");
    CHECK(parseFixed("-1.5", big) && formatFixed(big) == "-1.5", formatFixed(big));

    // readFixed on simdjson fields, the way the parsers call it
    simdjson::ondemand::parser parser;
    simdjson::padded_string json(std::string(R"({"p":"84523.10","q":"0.003","n":12.5,"big":"413772355107.944","bad":"1.2.3"})"));
    auto doc = parser.iterate(json);
    CHECK(readFixed(doc["p"]) == 8452310000000, "readFixed p");
    CHECK(readFixed(doc["q"]) == 300000, "readFixed q");
    bool threw = false;
    try { (void)readFixed(doc["n"]); } catch(std::exception&) { threw = true; }
    CHECK(threw, "a JSON number is not a decimal string and must throw like get_double_in_string");
    CHECK(formatFixed(readFixed128(doc["big"])) == "413772355107.944", "readFixed128");
    threw = false;
    try { (void)readFixed(doc["bad"]); } catch(std::invalid_argument&) { threw = true; }
    CHECK(threw, "malformed string must throw");

    std::cout << (failures ? "FAILED" : "all checks passed") << std::endl;
    return failures ? 1 : 0;
}

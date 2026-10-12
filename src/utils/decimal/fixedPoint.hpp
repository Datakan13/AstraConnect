#pragma once
#include <cstdint>
#include <stdexcept>
#include <string>
#include <string_view>
#include <simdjson/simdjson.h>

// Prices, quantities and amounts are fixed point: an integer count of 1e-8 units,
// parsed straight from Binance's decimal strings and never through double.
// "84523.1" -> 8452310000000, "0.001" -> 100000.
//
// Per-trade, per-level and per-order values and balances are int64 (Fixed),
// which holds up to about 9.2e10. Aggregates that can exceed that, such as candle
// volume, open interest and circulating supply, are __int128 (Fixed128).
namespace AstraConnect {
namespace Utils {

using Fixed = int64_t;
__extension__ typedef __int128 Fixed128;

inline constexpr int FIXED_DIGITS = 8;
inline constexpr Fixed FIXED_SCALE = 100000000;

namespace detail {
template<typename Int> struct FixedLimits;
template<> struct FixedLimits<int64_t> {
    using Unsigned = uint64_t;
    // magnitude of the most negative value
    static constexpr Unsigned negLimit = Unsigned(1) << 63;
};
template<> struct FixedLimits<Fixed128> {
    __extension__ typedef unsigned __int128 Unsigned;
    static constexpr Unsigned negLimit = Unsigned(1) << 127;
};
} // namespace detail

// "123.45" -> 12345000000. Digits beyond the 8th decimal round half away from zero.
// Returns false, leaving out untouched, on a malformed string or overflow.
template<typename Int>
bool parseFixed(std::string_view s, Int& out) noexcept {
    using U = typename detail::FixedLimits<Int>::Unsigned;
    constexpr U negLimit = detail::FixedLimits<Int>::negLimit;
    size_t i = 0;
    bool negative = false;
    if(i < s.size() && (s[i] == '-' || s[i] == '+')) negative = s[i++] == '-';
    U value = 0;
    int decimals = -1;          // -1 before the point
    bool anyDigit = false;
    bool roundUp = false;
    for(; i < s.size(); i++) {
        const char c = s[i];
        if(c == '.') {
            if(decimals >= 0) return false;
            decimals = 0;
            continue;
        }
        if(c < '0' || c > '9') return false;
        anyDigit = true;
        if(decimals >= FIXED_DIGITS) {
            // only the first dropped digit decides the rounding
            if(decimals == FIXED_DIGITS) roundUp = c >= '5';
            decimals++;
            continue;
        }
        const U digit = U(c - '0');
        if(value > (negLimit - digit) / 10) return false;
        value = value * 10 + digit;
        if(decimals >= 0) decimals++;
    }
    if(!anyDigit) return false;
    for(int k = decimals < 0 ? 0 : decimals; k < FIXED_DIGITS; k++) {
        if(value > negLimit / 10) return false;
        value *= 10;
    }
    if(roundUp) {
        if(value >= negLimit) return false;
        value++;
    }
    if(value > negLimit || (!negative && value == negLimit)) return false;
    out = negative ? Int(U(0) - value) : Int(value);
    return true;
}

// Inverse of parseFixed: shortest exact decimal, "84523.1", "0", "-0.5".
// This is what order requests send, so a price is never rounded on the way out.
template<typename Int>
std::string formatFixed(Int v) {
    using U = typename detail::FixedLimits<Int>::Unsigned;
    const bool negative = v < 0;
    U magnitude = negative ? U(0) - U(v) : U(v);
    U whole = magnitude / U(FIXED_SCALE);
    U fraction = magnitude % U(FIXED_SCALE);

    char digits[48];
    int n = 0;
    do { digits[n++] = char('0' + int(whole % 10)); whole /= 10; } while(whole);
    std::string out;
    if(negative) out += '-';
    while(n) out += digits[--n];
    if(fraction) {
        char decimals[FIXED_DIGITS];
        for(int k = FIXED_DIGITS - 1; k >= 0; k--) { decimals[k] = char('0' + int(fraction % 10)); fraction /= 10; }
        int len = FIXED_DIGITS;
        while(decimals[len - 1] == '0') len--;
        out += '.';
        out.append(decimals, len);
    }
    return out;
}

// For logging and quick maths only: the double nearest to a fixed value.
template<typename Int>
double toDouble(Int v) noexcept { return double(v) / double(FIXED_SCALE); }

// Reads a JSON string field ("43187.00") as fixed point. Throws on a missing or
// non-string field or one that does not parse, like the simdjson .value() calls
// these replace, so the callers' existing catch blocks report it.
template<typename Int, typename Field>
Int readFixedAs(Field&& field) {
    std::string_view text;
    if(auto ec = std::forward<Field>(field).get_string().get(text)) throw simdjson::simdjson_error(ec);
    Int out;
    if(!parseFixed(text, out)) throw std::invalid_argument("not a decimal within range: \"" + std::string(text) + "\"");
    return out;
}

template<typename Field>
Fixed readFixed(Field&& field) { return readFixedAs<Fixed>(std::forward<Field>(field)); }

template<typename Field>
Fixed128 readFixed128(Field&& field) { return readFixedAs<Fixed128>(std::forward<Field>(field)); }

} // namespace Utils
} // namespace AstraConnect

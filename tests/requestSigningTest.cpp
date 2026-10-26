// Request building and HMAC signing for the WebSocket API:
//
//   1. binance_doc_example  — the parameters from Binance's signing example, built
//                             with RequestParameter and signed with hmac_sha256,
//                             give Binance's published signature.
//   2. matches_openssl      — random parameters: hmac_sha256 over the JSON request
//                             equals OpenSSL's HMAC over the equivalent query string.
//   3. request_building     — quoting, the unquoted timestamp, nesting, and the
//                             all-or-nothing rule for empty parameters.
//   4. place_order_params   — PlaceOrderParameters spells every enum the way
//                             Binance expects.
#include <utils/crypto/hmacSha256.hpp>
#include <core/protocol/requestParameter.hpp>
#include <api/futures/order/placeOrderParameters.hpp>
#include "testHarness.hpp"

#include <openssl/evp.h>
#include <random>
#include <string>

using Param = RequestParameter<std::string>;

static std::string referenceHmac(const std::string& key, const std::string& message) {
    unsigned char digest[EVP_MAX_MD_SIZE];
    unsigned int len = 0;
    HMAC(EVP_sha256(), key.data(), int(key.size()),
         reinterpret_cast<const unsigned char*>(message.data()), message.size(), digest, &len);
    static constexpr char hex[] = "0123456789abcdef";
    std::string out;
    for (unsigned i = 0; i < len; ++i) { out += hex[digest[i] >> 4]; out += hex[digest[i] & 0xF]; }
    return out;
}

static void test_binance_doc_example() {
    // https://developers.binance.com/docs/derivatives/usds-margined-futures/general-info
    const std::string key = "NhqPtmdSJYdKjVHjA7PZj4Mge3R5YNiP1e3UZjInClVN65XAbvqqM6A7H5fATj0j";
    Param symbol("symbol", "LTCBTC"), side("side", "BUY"), type("type", "LIMIT"), tif("timeInForce", "GTC"),
        quantity("quantity", "1"), price("price", "0.1"), recvWindow("recvWindow", "5000"),
        timestamp("timestamp", "1499827319559");
    Param params("params");
    params.makeRequestFromRequestParameters(symbol, side, type, tif, quantity, price, recvWindow, timestamp);
    CHECK_CTX(params.request.back() == '}' && params.request.find("\"timestamp\": 1499827319559}") != std::string::npos,
              params.request);
    const std::string signature = hmac_sha256(key, params.request);
    CHECK_CTX(signature == "c8db56825ae71d6d79447849e617115f4a920fa2acdcab2b053c4b2838bd6b71", signature);

    Param sig("signature", signature);
    params.addParameterToRequest(sig);
    const std::string tail = ", \"signature\": \"" + signature + "\"}";
    CHECK_CTX(params.request.size() > tail.size() &&
                  params.request.compare(params.request.size() - tail.size(), tail.size(), tail) == 0,
              params.request);
}

static void test_matches_openssl() {
    std::mt19937 rng(7);
    const std::string alphabet = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789.-_";
    auto word = [&](size_t min, size_t max) {
        std::string s(min + rng() % (max - min + 1), ' ');
        for (char& c : s) c = alphabet[rng() % alphabet.size()];
        return s;
    };
    for (int i = 0; i < 2000; ++i) {
        const std::string key = word(1, 80);
        Param a("apiKey", word(1, 64)), b("price", word(1, 12)), c("symbol", word(1, 12)),
            ts("timestamp", std::to_string(1700000000000 + rng() % 1000000));
        Param out("params");
        out.makeRequestFromRequestParameters(a, b, c, ts);
        const std::string query = "apiKey=" + a.request + "&price=" + b.request + "&symbol=" + c.request +
                                  "&timestamp=" + ts.request;
        const std::string got = hmac_sha256(key, out.request);
        CHECK_CTX(got == referenceHmac(key, query), out.request);
        if (got != referenceHmac(key, query)) return;
    }
    bool threw = false;
    try { hmac_sha256("k", ""); } catch (std::runtime_error&) { threw = true; }
    CHECK_CTX(threw, "an empty request must be rejected");
}

static void test_request_building() {
    Param a("a", "1"), b("b", "two");
    Param out("params");
    out.makeRequestFromRequestParameters(a, b);
    CHECK_CTX(out.request == R"({"a": "1", "b": "two"})", out.request);
    CHECK(out.isConstructed);

    // A constructed parameter nests as raw JSON
    Param method("method", "order.place");
    Param outer("root");
    outer.makeRequestFromRequestParameters(method, out);
    CHECK_CTX(outer.request == R"({"method": "order.place", "params": {"a": "1", "b": "two"}})", outer.request);

    // The free function quotes timestamp like any other value
    CHECK_CTX(makeRequestFromRequestParameters(a, b) == R"({"a": "1", "b": "two"})",
              makeRequestFromRequestParameters(a, b));

    // Any empty field or value empties the whole request
    Param empty("c");
    Param partial("params");
    partial.makeRequestFromRequestParameters(a, empty, b);
    CHECK_CTX(partial.request.empty(), partial.request);
    CHECK(makeRequestFromRequestParameters(a, empty).empty());

    std::string raw = R"({"a": "1"})";
    Param sig("signature", "\"ff\"");
    addParameterToRequest(raw, sig);
    CHECK_CTX(raw == R"({"a": "1", "signature": "ff"})", raw);
    std::string notObject = "x";
    addParameterToRequest(notObject, sig);
    CHECK(notObject == "x");
}

static void test_place_order_params() {
    PlaceOrderParameters p;
    p.prepareOrder(PositionSide::SHORT, OrderSide::SELL, TimeInForce::GTX, OrderType::TAKE_PROFIT_MARKET,
                   "BTCUSDT", "65000.1", "0.002");
    CHECK(p.positionSide.request == "SHORT");
    CHECK(p.side.request == "SELL");
    CHECK(p.timeInForce.request == "GTX");
    CHECK(p.type.request == "TAKE_PROFIT_MARKET" && p.type.requestField == "type");
    CHECK(p.symbol.request == "BTCUSDT");
    CHECK(p.price.request == "65000.1" && p.quantity.request == "0.002");
    const int64_t ts = std::stoll(p.timestamp.request);
    CHECK_CTX(std::llabs(ts - AstraLib::Time::unixTimestampMS()) < 60000, p.timestamp.request);

    // The modify overload sends the type as origType and adds priceMatch
    p.prepareOrder(PositionSide::BOTH, OrderSide::BUY, PriceMatchMode::QUEUE, OrderType::LIMIT, "ETHUSDT", "1", "2");
    CHECK(p.type.requestField == "origType");
    CHECK(p.priceMatch.request == "QUEUE");
    CHECK(p.positionSide.request == "BOTH" && p.side.request == "BUY");

    // Signing the full new-order request is the same as signing its query string
    PlaceOrderParameters n;
    n.prepareOrder(PositionSide::LONG, OrderSide::BUY, TimeInForce::GTC, OrderType::LIMIT, "BTCUSDT", "40000", "0.01");
    Param apiKey("apiKey", "key123");
    Param out("params");
    out.makeRequestFromRequestParameters(apiKey, n.positionSide, n.price, n.quantity, n.side, n.symbol,
                                         n.timeInForce, n.timestamp, n.type);
    const std::string query = "apiKey=key123&positionSide=LONG&price=40000&quantity=0.01&side=BUY&symbol=BTCUSDT"
                              "&timeInForce=GTC&timestamp=" + n.timestamp.request + "&type=LIMIT";
    CHECK_CTX(hmac_sha256("secret", out.request) == referenceHmac("secret", query), out.request);
}

int main() {
    return TestHarness::run({
        {"binance_doc_example", test_binance_doc_example},
        {"matches_openssl",     test_matches_openssl},
        {"request_building",    test_request_building},
        {"place_order_params",  test_place_order_params},
    });
}

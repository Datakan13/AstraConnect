// Live tests for AstraConnect::RawWebsocket and Futures::API::fetchRaw against
// Binance USD-M (no API key needed). Needs network access.
//
//   1. subscribe        — SUBSCRIBE on /public/stream answers {"result":null,"id":1}
//                         and data follows.
//   2. concurrent_send  — sendText from two threads while messages stream in
//                         (run a -fsanitize=thread build for the race check).
//   3. close_is_prompt  — close() returns in under 1 s and no callback arrives after.
//   4. routing          — which of /ws, /public/ws and /market/ws deliver aggTrade,
//                         markPrice and trade (informational, plus the expected ones).
//   5. fetch_raw        — /fapi/v1/time returns 200 and a body; a bad symbol returns
//                         the 4xx with Binance's error body instead of failing.
//
// A network cut (onClose within ~70 s) is not automated: it needs root to drop
// packets. Run with --idle-test while blocking the connection by hand.
#include <api/AstraConnect.hpp>
#include <atomic>
#include <chrono>
#include <iostream>
#include <mutex>
#include <string>
#include <thread>

using namespace std::chrono;
using AstraConnect::RawStreamHandlers;
using AstraConnect::RawWebsocket;

static int failures = 0;
#define CHECK(cond, msg) do { if(!(cond)) { failures++; std::cout << "  FAIL: " << msg << "\n"; } } while(0)

struct Collector {
    std::mutex mutex;
    std::string firstReply;
    std::atomic<int> messages{0};
    std::atomic<int> afterClose{0};
    std::atomic<bool> closed{false};
    std::atomic<bool> closeCalled{false};
    std::string closeReason;

    RawStreamHandlers handlers() {
        RawStreamHandlers h;
        h.onMessage = [this](std::string_view message, uint64_t recvNs) {
            if(closeCalled.load()) afterClose++;
            if(recvNs == 0) afterClose++;   // never valid
            std::lock_guard<std::mutex> guard(mutex);
            if(firstReply.empty()) firstReply = std::string(message);
            messages++;
        };
        h.onClose = [this](const std::string& reason) {
            if(closeCalled.load()) afterClose++;
            std::lock_guard<std::mutex> guard(mutex);
            closeReason = reason;
            closed = true;
        };
        return h;
    }
};

static bool waitFor(const std::function<bool()>& done, milliseconds limit) {
    auto deadline = steady_clock::now() + limit;
    while(!done()) {
        if(steady_clock::now() > deadline) return false;
        std::this_thread::sleep_for(milliseconds(20));
    }
    return true;
}

static void testSubscribeConcurrentClose(APIManager& manager) {
    std::cout << "[subscribe / concurrent_send / close_is_prompt]\n";
    Collector c;
    FetchError err(APIError::SUCCESS);
    auto ws = RawWebsocket::connect(manager, manager.hostFuturesWebsocket, "/public/stream", c.handlers(), err);
    CHECK(ws, "connect failed: " << err.getErrorMsg());
    if(!ws) return;
    CHECK(ws->sendText(R"({"method":"SUBSCRIBE","params":["btcusdt@bookTicker"],"id":1})"), "sendText refused");
    CHECK(waitFor([&]{ return c.messages.load() >= 20; }, seconds(10)), "fewer than 20 messages in 10 s");
    {
        std::lock_guard<std::mutex> guard(c.mutex);
        CHECK(c.firstReply == R"({"result":null,"id":1})", "first reply was " << c.firstReply);
    }

    // Two threads sending while the I/O thread reads. Ids 100..; Binance answers each.
    const int perThread = 4;   // stays under Binance's 10 incoming messages/s
    const int before = c.messages.load();
    std::thread a([&]{ for(int i = 0; i < perThread; i++) { ws->sendText(R"({"method":"LIST_SUBSCRIPTIONS","id":)" + std::to_string(100 + i) + "}"); std::this_thread::sleep_for(milliseconds(150)); } });
    std::thread b([&]{ for(int i = 0; i < perThread; i++) { ws->sendText(R"({"method":"LIST_SUBSCRIPTIONS","id":)" + std::to_string(200 + i) + "}"); std::this_thread::sleep_for(milliseconds(150)); } });
    a.join(); b.join();
    CHECK(waitFor([&]{ return c.messages.load() > before + 2 * perThread; }, seconds(5)), "stream stalled after concurrent sends");
    CHECK(!c.closed.load(), "connection dropped: " << c.closeReason);

    c.closeCalled = true;
    auto t0 = steady_clock::now();
    ws->close();
    auto took = duration_cast<milliseconds>(steady_clock::now() - t0).count();
    std::cout << "  close() took " << took << " ms, " << c.messages.load() << " messages\n";
    CHECK(took < 1000, "close() took " << took << " ms");
    CHECK(!ws->sendText("{}"), "sendText accepted after close");
    const int atClose = c.messages.load();
    std::this_thread::sleep_for(milliseconds(500));
    CHECK(c.messages.load() == atClose && c.afterClose.load() == 0, "callback after close()");
    ws->close();   // idempotent
}

// Counts messages on `path` for `seconds`.
static int countOn(APIManager& manager, const std::string& path, int secs) {
    Collector c;
    FetchError err(APIError::SUCCESS);
    auto ws = RawWebsocket::connect(manager, manager.hostFuturesWebsocket, path, c.handlers(), err);
    if(!ws) { std::cout << "  " << path << ": connect failed: " << err.getErrorMsg() << "\n"; return -1; }
    std::this_thread::sleep_for(seconds(secs));
    ws->close();
    return c.messages.load();
}

static void testRouting(APIManager& manager) {
    std::cout << "[routing]\n";
    struct Row { const char* stream; const char* expectPath; } rows[] = {
        {"btcusdt@aggTrade", "/market"},
        {"btcusdt@markPrice@1s", "/market"},
        {"btcusdt@trade", nullptr},
        {"btcusdt@depth", "/public"},
    };
    for(auto& row : rows) {
        int legacy = countOn(manager, std::string("/ws/") + row.stream, 4);
        int pub = countOn(manager, std::string("/public/ws/") + row.stream, 4);
        int mkt = countOn(manager, std::string("/market/ws/") + row.stream, 4);
        std::cout << "  " << row.stream << ": /ws " << legacy << ", /public " << pub << ", /market " << mkt << "\n";
        if(row.expectPath) {
            int expected = std::string(row.expectPath) == "/market" ? mkt : pub;
            CHECK(expected > 0, row.stream << " silent on " << row.expectPath);
        }
    }
}

static void testFetchRaw(APIManager& manager) {
    std::cout << "[fetch_raw]\n";
    APIManager::Futures::API api(manager);
    std::string body;
    int status = 0;
    FetchError err = api.fetchRaw("/fapi/v1/time", body, status);
    CHECK(!err, "fetchRaw failed: " << err.getErrorMsg());
    CHECK(status == 200 && body.find("serverTime") != std::string::npos, "time: HTTP " << status << " " << body);
    err = api.fetchRaw("/fapi/v1/ticker/price?symbol=NOTASYMBOL", body, status);
    CHECK(!err, "4xx must still be SUCCESS: " << err.getErrorMsg());
    CHECK(status / 100 == 4 && body.find("\"code\"") != std::string::npos, "bad symbol: HTTP " << status << " " << body);
    err = api.fetchRaw("/fapi/v1/exchangeInfo", body, status);
    CHECK(!err && status == 200 && body.size() > 100000, "exchangeInfo: HTTP " << status << ", " << body.size() << " bytes");
    std::cout << "  exchangeInfo " << body.size() << " bytes\n";
}

static void idleTest(APIManager& manager) {
    std::cout << "[idle] connected; cut the network now and wait for onClose (expect <= ~70 s)\n";
    Collector c;
    FetchError err(APIError::SUCCESS);
    auto ws = RawWebsocket::connect(manager, manager.hostFuturesWebsocket, "/public/ws/btcusdt@bookTicker", c.handlers(), err);
    if(!ws) { std::cout << "connect failed: " << err.getErrorMsg() << "\n"; failures++; return; }
    auto t0 = steady_clock::now();
    waitFor([&]{ return c.closed.load(); }, seconds(600));
    std::cout << "  onClose after " << duration_cast<seconds>(steady_clock::now() - t0).count() << " s: " << c.closeReason << "\n";
}

int main(int argc, char** argv) {
    APIManager manager;
    if(argc > 1 && std::string(argv[1]) == "--idle-test") { idleTest(manager); return failures ? 1 : 0; }
    testSubscribeConcurrentClose(manager);
    testFetchRaw(manager);
    testRouting(manager);
    std::cout << (failures ? "FAILED" : "all checks passed") << std::endl;
    return failures ? 1 : 0;
}

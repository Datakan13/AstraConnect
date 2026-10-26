// The shared state the stream and order threads lean on, hammered from several
// threads. Meant to be run under -fsanitize=thread as well as in the normal build.
//
//   1. spinlock_exclusion    — a plain counter under Spinlock adds up exactly.
//   2. shared_parser         — one ThreadSafeParser shared by several threads, the
//                              way the stream holders share APIManager's parser.
//   3. index_pool            — ThreadSafeIndexPool never hands one index to two
//                              holders, and every index comes back.
//   4. order_tracker         — writers replace order responses while readers read
//                              them through OrderAccess; every read sees one
//                              consistent response.
//   5. active_order_queue    — OrderTracker's active order ring, MPMC, exactly once.
//   6. backoff_shutdown_race — shutdown signalled at random points around the wait
//                              always ends the backoff promptly with CLOSED.
//
// ASTRA_STRESS_SCALE=N divides the iteration counts by N.
#include <core/parsing/threadSafeParser.hpp>
#include <api/futures/order/orderTracker.hpp>
#include <utils/net/exponentialBackOff.hpp>
#include <AstraLib/Atomic/cpuRelax.hpp>
#include "testHarness.hpp"

#include <array>
#include <atomic>
#include <chrono>
#include <memory>
#include <random>
#include <string>
#include <thread>
#include <vector>

using AstraConnect::Utils::Fixed;
using AstraConnect::Utils::FIXED_SCALE;
using AstraConnect::Utils::formatFixed;

static void test_spinlock_exclusion() {
    constexpr int THREADS = 8;
    const uint64_t PER = TestHarness::scaled(200000, 5000);
    AstraLib::Atomic::Spinlock lock;
    uint64_t counter = 0;      // deliberately not atomic
    uint64_t pairA = 0, pairB = 0;
    std::vector<std::thread> threads;
    for (int t = 0; t < THREADS; ++t)
        threads.emplace_back([&] {
            for (uint64_t i = 0; i < PER; ++i) {
                AstraLib::Atomic::SpinlockGuard guard(lock);
                counter++;
                CHECK(pairA == pairB);
                pairA++;
                pairB++;
            }
        });
    for (auto& t : threads) t.join();
    CHECK_CTX(counter == THREADS * PER, counter << " != " << THREADS * PER);
    CHECK(pairA == counter && pairB == counter);
}

static void test_shared_parser() {
    constexpr int THREADS = 6;
    const uint64_t PER = TestHarness::scaled(40000, 1000);
    ThreadSafeParser shared;
    std::vector<std::thread> threads;
    for (int t = 0; t < THREADS; ++t)
        threads.emplace_back([&, t] {
            for (uint64_t i = 0; i < PER; ++i) {
                const int64_t id = int64_t(t) * 1000000000 + int64_t(i);
                const Fixed price = Fixed(i % 100000) * FIXED_SCALE + t;
                simdjson::padded_string json(std::string(R"({"E":)") + std::to_string(id) + R"(,"p":")" +
                                             formatFixed(price) + R"(","s":"T)" + std::to_string(t) + R"("})");
                ThreadSafeParserRAII parser(shared);
                auto doc = parser.parser.iterate(json);
                const int64_t gotId = doc["E"].get_int64().value();
                const Fixed gotPrice = AstraConnect::Utils::readFixed(doc["p"]);
                std::string_view symbol = doc["s"].get_string().value();
                CHECK_CTX(gotId == id && gotPrice == price && symbol == "T" + std::to_string(t),
                          "thread " << t << " read another thread's document");
            }
        });
    for (auto& t : threads) t.join();
}

static void test_index_pool() {
    constexpr int SIZE = 64, THREADS = 8;
    const uint64_t PER = TestHarness::scaled(100000, 2000);
    auto pool = std::make_unique<AstraLib::Pools::ThreadSafeIndexPool<SIZE>>();
    std::array<std::atomic<int>, SIZE> holder{};
    for (auto& h : holder) h.store(-1);

    std::vector<std::thread> threads;
    for (int t = 0; t < THREADS; ++t)
        threads.emplace_back([&, t] {
            for (uint64_t i = 0; i < PER; ++i) {
                const int index = pool->getIndex();
                CHECK_CTX(index >= 0 && index < SIZE, "index " << index);
                if (index < 0 || index >= SIZE) return;
                int expected = -1;
                CHECK_CTX(holder[index].compare_exchange_strong(expected, t), "index " << index << " held by "
                                                                                      << expected << " and " << t);
                if (i % 16 == 0) AstraLib::Atomic::cpuRelax();
                holder[index].store(-1);
                pool->returnIndex(index);
            }
        });
    for (auto& t : threads) t.join();

    std::array<int, SIZE> seen{};
    for (int i = 0; i < SIZE; ++i) {
        const int index = pool->getIndex();
        if (index >= 0 && index < SIZE) seen[index]++;
    }
    for (int i = 0; i < SIZE; ++i) CHECK_CTX(seen[i] == 1, "index " << i << " came back " << seen[i] << " times");
}

// A NEW response whose price and quantity are both derived from version, so a torn
// read (fields from two responses) is detectable.
static std::string orderResponse(int64_t orderId, int64_t version) {
    const Fixed price = Fixed(40000 + version % 10000) * FIXED_SCALE;
    const Fixed qty = Fixed(1 + version % 977) * (FIXED_SCALE / 1000);
    return R"({"id":"x","status":200,"result":{"orderId":)" + std::to_string(orderId) +
           R"(,"symbol":"BTCUSDT","status":"NEW","clientOrderId":"c","price":")" + formatFixed(price) +
           R"(","avgPrice":"0","origQty":")" + formatFixed(qty) +
           R"(","executedQty":"0","cumQty":"0","cumQuote":"0","timeInForce":"GTC","type":"LIMIT",)"
           R"("reduceOnly":false,"closePosition":false,"side":"BUY","positionSide":"BOTH","stopPrice":"0",)"
           R"("workingType":"CONTRACT_PRICE","priceProtect":false,"origType":"LIMIT","priceMatch":"NONE",)"
           R"("selfTradePreventionMode":"NONE","goodTillDate":0,"updateTime":)" + std::to_string(version) +
           R"(},"rateLimits":[]})";
}

static void test_order_tracker() {
    constexpr int ORDERS = 16, WRITERS = 3, READERS = 4;
    const uint64_t WRITES = TestHarness::scaled(20000, 500);
    OrderTracker tracker;
    std::vector<std::string> ids;
    simdjson::ondemand::parser setup;
    for (int k = 0; k < ORDERS; ++k) {
        ids.push_back("order-" + std::to_string(k));
        simdjson::padded_string json(orderResponse(k, 0));
        auto doc = setup.iterate(json).value();
        tracker.registerOrderResponse(ids.back(), OrderTypeSent::NEW, doc);
    }

    std::atomic<bool> done{false};
    std::vector<std::thread> threads;
    for (int w = 0; w < WRITERS; ++w)
        threads.emplace_back([&, w] {
            simdjson::ondemand::parser parser;
            for (uint64_t i = 1; i <= WRITES; ++i) {
                const int k = int((i * 7 + w) % ORDERS);
                simdjson::padded_string json(orderResponse(k, int64_t(i) * WRITERS + w));
                auto doc = parser.iterate(json).value();
                tracker.updateOrderResponse(ids[k], OrderTypeSent::NEW, doc);
            }
        });
    for (int r = 0; r < READERS; ++r)
        threads.emplace_back([&, r] {
            OrderInfo::OrderResponse info(tracker);
            uint64_t i = 0;
            while (!done.load(std::memory_order_acquire)) {
                const int k = int((i++ + r) % ORDERS);
                {
                    OrderAccess access(tracker, ids[k]);
                    const int64_t version = access.order.getUpdateTime();
                    const Fixed price = access.order.getPrice();
                    const Fixed qty = access.order.getOrigQty();
                    CHECK_CTX(access.order.getOrderId() == k, ids[k] << " has order id " << access.order.getOrderId());
                    CHECK_CTX(price == Fixed(40000 + version % 10000) * FIXED_SCALE &&
                                  qty == Fixed(1 + version % 977) * (FIXED_SCALE / 1000),
                              ids[k] << ": torn read at version " << version);
                }
                // The per-field getters take their own access each time
                CHECK(info.getSymbol(ids[k]) == "BTCUSDT");
                CHECK(tracker.getOrderStatusint(ids[k]) == 200);
                CHECK(tracker.getOrderTypeStatus(ids[k]) == OrderTypeSent::NEW);
            }
        });
    for (int w = 0; w < WRITERS; ++w) threads[w].join();
    done.store(true, std::memory_order_release);
    for (size_t t = WRITERS; t < threads.size(); ++t) threads[t].join();
    std::string missing = "never-sent";
    CHECK(tracker.getOrderTypeStatus(missing) == OrderTypeSent::NONE);
}

static void test_active_order_queue() {
    constexpr int P = 3, C = 3;
    const uint64_t PER = TestHarness::scaled(100000, 2000);
    auto tracker = std::make_unique<OrderTracker>();
    auto seen = std::unique_ptr<std::atomic<uint8_t>[]>(new std::atomic<uint8_t>[P * PER]);
    for (uint64_t i = 0; i < P * PER; ++i) seen[i].store(0);
    std::atomic<uint64_t> taken{0};

    std::vector<std::thread> threads;
    for (int p = 0; p < P; ++p)
        threads.emplace_back([&, p] {
            for (uint64_t i = 0; i < PER; ++i) tracker->queueActiveOrderId(int64_t(p * PER + i));
        });
    // Blocking dequeue: each consumer takes a fixed share so none waits forever
    for (int c = 0; c < C; ++c)
        threads.emplace_back([&] {
            for (uint64_t i = 0; i < PER; ++i) {
                const int64_t id = tracker->getActiveOrderId();
                CHECK_CTX(id >= 0 && uint64_t(id) < P * PER, "garbage id " << id);
                if (id >= 0 && uint64_t(id) < P * PER) CHECK_CTX(seen[id].fetch_add(1) == 0, "duplicate " << id);
                taken.fetch_add(1, std::memory_order_relaxed);
            }
        });
    for (auto& t : threads) t.join();
    CHECK(taken.load() == P * PER);
    for (uint64_t i = 0; i < P * PER; ++i) CHECK_CTX(seen[i].load() == 1, "lost id " << i);
}

static void test_backoff_shutdown_race() {
    const uint64_t ROUNDS = TestHarness::scaled(400, 40);
    std::mt19937 rng(3);
    double worst = 0;
    for (uint64_t round = 0; round < ROUNDS; ++round) {
        AstraConnect::Utils::BackoffInterrupt wakeup;
        std::atomic<bool> running{true};
        std::atomic<int> attempts{0};
        ConnectionStatus result = ConnectionStatus::SUCCESS;
        const int spin = int(rng() % 20000);
        const auto start = std::chrono::steady_clock::now();
        std::thread holder([&] {
            result = AstraConnect::Utils::exponentialBackOff(
                [&] { attempts++; return ConnectionStatus::FAIL; }, [&] { return running.load(); },
                AstraConnect::Utils::BackoffPolicy{30000, 2, 7}, &wakeup);
        });
        // Shut down at a random point: before, during or after the holder parks
        for (int i = 0; i < spin; ++i) AstraLib::Atomic::cpuRelax();
        running.store(false);
        wakeup.signal();
        holder.join();
        const double seconds = std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count();
        if (seconds > worst) worst = seconds;
        CHECK_CTX(result == ConnectionStatus::CLOSED, "round " << round);
        CHECK_CTX(attempts.load() == 0, "round " << round << ": attempted after shutdown");
        CHECK_CTX(seconds < 5, "round " << round << ": lost wakeup, took " << seconds << "s");
    }
    std::cout << "         " << ROUNDS << " rounds, slowest shutdown " << worst * 1000 << " ms" << std::endl;
}

int main() {
    return TestHarness::run({
        {"spinlock_exclusion",    test_spinlock_exclusion},
        {"shared_parser",         test_shared_parser},
        {"index_pool",            test_index_pool},
        {"order_tracker",         test_order_tracker},
        {"active_order_queue",    test_active_order_queue},
        {"backoff_shutdown_race", test_backoff_shutdown_race},
    });
}

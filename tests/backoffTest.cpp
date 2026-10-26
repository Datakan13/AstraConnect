// utils/net/exponentialBackOff.hpp: the reconnect schedule every stream holder uses.
//
//   1. attempt_counts      — SUCCESS on the n-th try; FAIL after exactly maxTryCount
//                            tries; the one-argument overload never returns CLOSED.
//   2. closed              — keepGoing() false before or during the wait gives CLOSED
//                            and no further attempt.
//   3. interrupt_wakes     — signal() ends a long wait at once, for every waiter.
//   4. interrupt_sticky    — a signal before the wait is still seen; reset() clears it.
//   5. max_delay           — the ceiling is computed in int64.
//
// Timing bounds are deliberately loose so a loaded CI runner or a sanitizer build
// does not fail them: they only tell "returned at once" from "slept a 10s interval".
#include <utils/net/exponentialBackOff.hpp>
#include "testHarness.hpp"

#include <atomic>
#include <chrono>
#include <thread>
#include <vector>

using namespace AstraConnect::Utils;
using Clock = std::chrono::steady_clock;

static double secondsSince(Clock::time_point start) {
    return std::chrono::duration<double>(Clock::now() - start).count();
}

static void test_attempt_counts() {
    for (int maxTries : {1, 2, 4, 7}) {
        const BackoffPolicy policy{1, 2, maxTries};
        int attempts = 0;
        ConnectionStatus s = exponentialBackOff([&] { attempts++; return ConnectionStatus::FAIL; },
                                                [] { return true; }, policy);
        CHECK_CTX(s == ConnectionStatus::FAIL, "maxTryCount " << maxTries);
        CHECK_CTX(attempts == maxTries, "maxTryCount " << maxTries << ": " << attempts << " attempts");
    }

    // Tripling: 1, 3, 9 ms, and 27 reaches the ceiling of 1 * 3^3
    int attempts = 0;
    CHECK(exponentialBackOff([&] { attempts++; return ConnectionStatus::FAIL; }, [] { return true; },
                             BackoffPolicy{1, 3, 3}) == ConnectionStatus::FAIL);
    CHECK_CTX(attempts == 3, attempts);

    attempts = 0;
    ConnectionStatus s = exponentialBackOff(
        [&] { return ++attempts == 3 ? ConnectionStatus::SUCCESS : ConnectionStatus::FAIL; },
        [] { return true; }, BackoffPolicy{1, 2, 7});
    CHECK(s == ConnectionStatus::SUCCESS);
    CHECK_CTX(attempts == 3, attempts);

    // No keepGoing: the overload for holders without a shutdown flag
    attempts = 0;
    s = exponentialBackOff([&] { return ++attempts == 2 ? ConnectionStatus::SUCCESS : ConnectionStatus::FAIL; },
                           BackoffPolicy{1, 2, 7});
    CHECK(s == ConnectionStatus::SUCCESS && attempts == 2);
    attempts = 0;
    s = exponentialBackOff([&] { attempts++; return ConnectionStatus::CLOSED; }, BackoffPolicy{1, 2, 3});
    CHECK_CTX(s == ConnectionStatus::FAIL && attempts == 3, "a CLOSED attempt counts as a failed one");
}

static void test_closed() {
    int attempts = 0;
    ConnectionStatus s = exponentialBackOff([&] { attempts++; return ConnectionStatus::SUCCESS; },
                                            [] { return false; }, BackoffPolicy{1, 2, 7});
    CHECK(s == ConnectionStatus::CLOSED && attempts == 0);

    // keepGoing flips while the first interval runs: checked again after the wait
    std::atomic<int> checks{0};
    s = exponentialBackOff([&] { attempts++; return ConnectionStatus::SUCCESS; },
                           [&] { return checks.fetch_add(1) == 0; }, BackoffPolicy{1, 2, 7});
    CHECK(s == ConnectionStatus::CLOSED && attempts == 0);

    // Stops between retries once keepGoing goes false
    attempts = 0;
    s = exponentialBackOff([&] { attempts++; return ConnectionStatus::FAIL; },
                           [&] { return attempts < 2; }, BackoffPolicy{1, 2, 7});
    CHECK(s == ConnectionStatus::CLOSED && attempts == 2);
}

static void test_interrupt_wakes() {
    // One holder parked in a 10s interval, shut down from another thread
    BackoffInterrupt wakeup;
    std::atomic<bool> running{true};
    std::atomic<int> attempts{0};
    ConnectionStatus result = ConnectionStatus::SUCCESS;
    const auto start = Clock::now();
    std::thread holder([&] {
        result = exponentialBackOff([&] { attempts++; return ConnectionStatus::FAIL; },
                                    [&] { return running.load(); }, BackoffPolicy{10000, 2, 7}, &wakeup);
    });
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    running.store(false);
    wakeup.signal();
    holder.join();
    CHECK(result == ConnectionStatus::CLOSED);
    CHECK(attempts.load() == 0);
    CHECK_CTX(secondsSince(start) < 5, secondsSince(start) << "s");

    // notify_all: every waiter on one handle is released by a single signal
    BackoffInterrupt shared;
    std::atomic<int> woken{0};
    std::vector<std::thread> waiters;
    const auto start2 = Clock::now();
    for (int i = 0; i < 8; ++i)
        waiters.emplace_back([&] { if (shared.waitFor(std::chrono::seconds(30))) woken++; });
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    shared.signal();
    for (auto& t : waiters) t.join();
    CHECK_CTX(woken.load() == 8, woken.load());
    CHECK_CTX(secondsSince(start2) < 5, secondsSince(start2) << "s");
}

static void test_interrupt_sticky() {
    BackoffInterrupt wakeup;
    wakeup.signal();
    auto start = Clock::now();
    CHECK(wakeup.waitFor(std::chrono::seconds(10)));
    CHECK(wakeup.waitFor(std::chrono::seconds(10)));   // stays signalled until reset
    CHECK_CTX(secondsSince(start) < 5, secondsSince(start) << "s");
    wakeup.reset();
    CHECK(!wakeup.waitFor(std::chrono::milliseconds(5)));

    // A signalled handle makes the whole schedule run without sleeping
    int attempts = 0;
    start = Clock::now();
    wakeup.signal();
    ConnectionStatus s = exponentialBackOff([&] { attempts++; return ConnectionStatus::FAIL; },
                                            [] { return true; }, BackoffPolicy{10000, 2, 3}, &wakeup);
    CHECK(s == ConnectionStatus::FAIL && attempts == 3);
    CHECK_CTX(secondsSince(start) < 5, secondsSince(start) << "s");
}

static void test_max_delay() {
    CHECK(BackoffPolicy().maxDelay() == 64000);
    CHECK((BackoffPolicy{500, 2, 40}.maxDelay() == 500LL * (1LL << 40)));
    CHECK((BackoffPolicy{1, 10, 15}.maxDelay() == 1000000000000000LL));
}

int main() {
    return TestHarness::run({
        {"attempt_counts",   test_attempt_counts},
        {"closed",           test_closed},
        {"interrupt_wakes",  test_interrupt_wakes},
        {"interrupt_sticky", test_interrupt_sticky},
        {"max_delay",        test_max_delay},
    });
}

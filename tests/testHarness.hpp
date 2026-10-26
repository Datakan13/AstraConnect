#pragma once
// Shared by the offline tests: a CHECK that is safe to call from any thread, the
// ASTRA_STRESS_SCALE knob, and a runner that prints one line per case.
//
//   CHECK(cond)            counts a failure and prints the condition
//   CHECK_CTX(cond, ctx)   the same, with a streamed context message
//   KNOWN_BUG(cond, ctx)   cond is what *should* hold. While it does not, the case
//                          prints [KNOWN BUG] and still passes. Once it does, the
//                          check fails, so the marker is turned into a CHECK.
//
// ASTRA_STRESS_SCALE=N divides stress iteration counts by N (see scaled()), so
// sanitizer builds can run the same stress cases in reasonable time.
#include <AstraLib/Atomic/spinlock.hpp>

#include <atomic>
#include <cstdint>
#include <cstdlib>
#include <initializer_list>
#include <iostream>

namespace TestHarness {

inline std::atomic<long> failures{0};
inline std::atomic<long> knownBugs{0};
inline AstraLib::Atomic::Spinlock outLock;

inline uint64_t scaled(uint64_t n, uint64_t floor) {
    const char* env = std::getenv("ASTRA_STRESS_SCALE");
    const uint64_t scale = env ? std::strtoull(env, nullptr, 10) : 1;
    const uint64_t v = scale > 1 ? n / scale : n;
    return v < floor ? floor : v;
}

struct Case {
    const char* name;
    void (*fn)();
};

inline int run(std::initializer_list<Case> cases) {
    for (const Case& c : cases) {
        std::cout << "[ RUN  ] " << c.name << std::endl;
        const long before = failures.load();
        c.fn();
        std::cout << (failures.load() == before ? "[  OK  ] " : "[ BAD  ] ") << c.name << std::endl;
    }
    if (knownBugs.load()) std::cout << "\n" << knownBugs.load() << " known bug(s) still present" << std::endl;
    if (failures.load() == 0) { std::cout << "\nall checks passed" << std::endl; return 0; }
    std::cerr << "\n" << failures.load() << " CHECK(s) FAILED" << std::endl;
    return 1;
}

} // namespace TestHarness

#define CHECK_CTX(cond, ctx)                                                 \
    do {                                                                     \
        if (!(cond)) {                                                       \
            TestHarness::failures.fetch_add(1, std::memory_order_relaxed);   \
            AstraLib::Atomic::SpinlockGuard guard(TestHarness::outLock);     \
            std::cerr << "[FAIL] " << __FILE__ << ":" << __LINE__            \
                      << "  CHECK(" #cond ")  " << ctx << "\n";              \
        }                                                                    \
    } while (0)
#define CHECK(cond) CHECK_CTX(cond, "")

#define KNOWN_BUG(cond, ctx)                                                 \
    do {                                                                     \
        AstraLib::Atomic::SpinlockGuard guard(TestHarness::outLock);         \
        if (!(cond)) {                                                       \
            TestHarness::knownBugs.fetch_add(1, std::memory_order_relaxed);  \
            std::cout << "         [KNOWN BUG] " << ctx << "\n";             \
        } else {                                                             \
            TestHarness::failures.fetch_add(1, std::memory_order_relaxed);   \
            std::cerr << "[FAIL] " << __FILE__ << ":" << __LINE__            \
                      << "  KNOWN_BUG now holds, make it a CHECK: " #cond    \
                      << "  " << ctx << "\n";                                \
        }                                                                    \
    } while (0)

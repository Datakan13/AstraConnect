// Non-blocking paths of the vendored AstraLib::Buffers::AtomicRingBuffer:
// tryEnqueue, tryDequeue and approxSize.
//
//   1. full_buffer_rejects     — tryEnqueue on a full buffer returns false and
//                                leaves every queued item intact.
//   2. empty_buffer_rejects    — tryDequeue on an empty buffer returns false and
//                                takes no ticket.
//   3. mixed_blocking_and_try  — enqueue/tryEnqueue/emplaceEnqueue and
//                                dequeue/tryDequeue on one buffer keep FIFO order.
//   4. approx_size             — tracks the queued count, clamped at 0.
//   5. mpsc_slow_consumer      — 4 producers x 1M tryEnqueue against a slow
//                                consumer: every accepted item comes out exactly
//                                once and in per-producer order, and accepted +
//                                rejected adds up.
//   6. mpmc_try_both_sides     — tryEnqueue/tryDequeue on both sides, exactly once.
//
// Also run it built with -fsanitize=thread. ASTRA_STRESS_SCALE=N divides the item
// counts of 5 and 6 by N.
#include <AstraLib/Atomic/cpuRelax.hpp>
#include <AstraLib/Atomic/spinlock.hpp>
#include <AstraLib/Buffers/atomicRingBuffer.hpp>

#include <array>
#include <atomic>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <thread>
#include <vector>

namespace RB = AstraLib::Buffers;

static std::atomic<long> g_failures{0};
static AstraLib::Atomic::Spinlock g_outLock;

#define CHECK_CTX(cond, ctx)                                                 \
    do {                                                                     \
        if (!(cond)) {                                                       \
            g_failures.fetch_add(1, std::memory_order_relaxed);              \
            AstraLib::Atomic::SpinlockGuard guard(g_outLock);                \
            std::cerr << "[FAIL] " << __FILE__ << ":" << __LINE__            \
                      << "  CHECK(" #cond ")  " << ctx << "\n";              \
        }                                                                    \
    } while (0)
#define CHECK(cond) CHECK_CTX(cond, "")

static uint64_t scaled(uint64_t n, uint64_t floor) {
    const char* env = std::getenv("ASTRA_STRESS_SCALE");
    const uint64_t scale = env ? std::strtoull(env, nullptr, 10) : 1;
    const uint64_t v = scale > 1 ? n / scale : n;
    return v < floor ? floor : v;
}

static void test_full_buffer_rejects() {
    RB::AtomicRingBuffer<int, 4> q;
    for (int i = 0; i < 4; ++i) CHECK_CTX(q.tryEnqueue(int(i)), "slot " << i << " should be free");
    CHECK_CTX(!q.tryEnqueue(99), "5th item must be rejected");
    const int copy = 98;
    CHECK_CTX(!q.tryEnqueue(copy), "const& overload must also be rejected");
    CHECK_CTX(q.approxSize() == 4, "approxSize " << q.approxSize());

    for (int i = 0; i < 4; ++i) {
        int v = -1;
        CHECK_CTX(q.tryDequeue(v), "item " << i << " missing");
        CHECK_CTX(v == i, "item " << i << " got " << v);
    }
    int v = -1;
    CHECK_CTX(!q.tryDequeue(v), "rejected items must not have been queued");

    // The rejections took no ticket: the next lap lines up
    CHECK(q.tryEnqueue(7));
    CHECK(q.tryDequeue(v) && v == 7);
}

static void test_empty_buffer_rejects() {
    RB::AtomicRingBuffer<int, 8> q;
    int v = -1;
    for (int i = 0; i < 100; ++i) CHECK_CTX(!q.tryDequeue(v), "fresh buffer returned " << v);
    CHECK_CTX(v == -1, "out must be untouched on failure, got " << v);
    CHECK(q.isEmpty());

    q.enqueue(42);
    CHECK_CTX(q.tryDequeue(v) && v == 42, "got " << v);
    CHECK(q.isEmpty());
}

static void test_mixed_blocking_and_try() {
    RB::AtomicRingBuffer<int, 4> q;
    int next = 0, expect = 0;
    for (int round = 0; round < 50; ++round) {
        q.enqueue(int(next++));
        CHECK(q.tryEnqueue(int(next++)));
        q.emplaceEnqueue(next++);
        int v = -1;
        CHECK(q.tryDequeue(v) && v == expect); expect++;
        v = q.dequeue();
        CHECK_CTX(v == expect, "round " << round << " got " << v << " expected " << expect); expect++;
        CHECK(q.tryDequeue(v) && v == expect); expect++;
    }
    CHECK(q.isEmpty());
}

static void test_approx_size() {
    RB::AtomicRingBuffer<int, 16> q;
    CHECK(q.approxSize() == 0);
    for (int i = 0; i < 10; ++i) q.enqueue(int(i));
    CHECK_CTX(q.approxSize() == 10, "got " << q.approxSize());
    int v;
    for (int i = 0; i < 3; ++i) q.tryDequeue(v);
    CHECK_CTX(q.approxSize() == 7, "got " << q.approxSize());
    while (q.tryDequeue(v)) {}
    CHECK_CTX(q.approxSize() == 0, "got " << q.approxSize());

    // A blocking dequeue on an empty buffer takes its ticket before it waits, so
    // readTicket runs ahead of writeTicket; the size must clamp, not wrap.
    std::thread waiter([&] { (void)q.dequeue(); });
    while (q.isEmpty()) AstraLib::Atomic::cpuRelax();
    CHECK_CTX(q.approxSize() == 0, "got " << q.approxSize());
    q.enqueue(1);
    waiter.join();
    CHECK(q.approxSize() == 0);
}

// Items encode (producer << 32) | sequence, so per-producer order is checkable.
static void test_mpsc_slow_consumer() {
    constexpr int P = 4;
    const uint64_t PER = scaled(1000000, 20000);
    auto q = std::make_unique<RB::AtomicRingBuffer<uint64_t, 1024>>();
    std::array<std::atomic<uint64_t>, P> accepted{}, rejected{};
    std::atomic<int> done{0};

    std::vector<std::thread> prods;
    for (int p = 0; p < P; ++p)
        prods.emplace_back([&, p] {
            uint64_t ok = 0, dropped = 0;
            for (uint64_t i = 0; i < PER; ++i) {
                if (q->tryEnqueue((uint64_t(p) << 32) | i)) ok++;
                else dropped++;
            }
            accepted[p].store(ok, std::memory_order_relaxed);
            rejected[p].store(dropped, std::memory_order_relaxed);
            done.fetch_add(1, std::memory_order_release);
        });

    std::array<int64_t, P> last;
    last.fill(-1);
    std::array<uint64_t, P> received{};
    uint64_t taken = 0;
    for (;;) {
        uint64_t v;
        if (q->tryDequeue(v)) {
            const int p = int(v >> 32);
            const int64_t seq = int64_t(v & 0xffffffffu);
            CHECK_CTX(p >= 0 && p < P, "garbage value " << v);
            if (p >= 0 && p < P) {
                CHECK_CTX(seq > last[p], "producer " << p << " out of order: " << seq << " after " << last[p]);
                last[p] = seq;
                received[p]++;
            }
            // Slow consumer: forces the buffer to fill so tryEnqueue rejects
            if (++taken % 64 == 0) for (int k = 0; k < 20; ++k) AstraLib::Atomic::cpuRelax();
        } else if (done.load(std::memory_order_acquire) == P && q->approxSize() == 0) {
            break;
        }
    }
    for (auto& t : prods) t.join();

    uint64_t totalRejected = 0;
    for (int p = 0; p < P; ++p) {
        const uint64_t a = accepted[p].load(), r = rejected[p].load();
        CHECK_CTX(a + r == PER, "producer " << p << ": " << a << " + " << r << " != " << PER);
        CHECK_CTX(received[p] == a, "producer " << p << ": received " << received[p] << ", accepted " << a);
        totalRejected += r;
    }
    CHECK_CTX(totalRejected > 0, "consumer never fell behind; the full path was not exercised");
    std::cout << "         " << P << "x" << PER << " offered, " << totalRejected << " rejected" << std::endl;
}

static void test_mpmc_try_both_sides() {
    constexpr int P = 3, C = 3;
    const uint64_t PER = scaled(200000, 5000);
    const uint64_t TOTAL = PER * P;
    auto q = std::make_unique<RB::AtomicRingBuffer<uint64_t, 64>>();
    auto seen = std::unique_ptr<std::atomic<uint8_t>[]>(new std::atomic<uint8_t>[TOTAL]);
    for (uint64_t i = 0; i < TOTAL; ++i) seen[i].store(0, std::memory_order_relaxed);
    std::atomic<uint64_t> consumed{0};

    std::vector<std::thread> threads;
    for (int p = 0; p < P; ++p)
        threads.emplace_back([&, p] {
            for (uint64_t i = 0; i < PER; ++i)
                while (!q->tryEnqueue(uint64_t(p) * PER + i)) AstraLib::Atomic::cpuRelax();
        });
    for (int c = 0; c < C; ++c)
        threads.emplace_back([&] {
            uint64_t v;
            while (consumed.load(std::memory_order_relaxed) < TOTAL) {
                if (!q->tryDequeue(v)) { AstraLib::Atomic::cpuRelax(); continue; }
                CHECK_CTX(v < TOTAL, "garbage value " << v);
                if (v < TOTAL) CHECK_CTX(seen[v].fetch_add(1) == 0, "duplicate " << v);
                consumed.fetch_add(1, std::memory_order_relaxed);
            }
        });
    for (auto& t : threads) t.join();
    for (uint64_t i = 0; i < TOTAL; ++i) CHECK_CTX(seen[i].load() == 1, "lost value " << i);
}

int main() {
    struct { const char* name; void (*fn)(); } tests[] = {
        {"full_buffer_rejects",     test_full_buffer_rejects},
        {"empty_buffer_rejects",    test_empty_buffer_rejects},
        {"mixed_blocking_and_try",  test_mixed_blocking_and_try},
        {"approx_size",             test_approx_size},
        {"mpsc_slow_consumer",      test_mpsc_slow_consumer},
        {"mpmc_try_both_sides",     test_mpmc_try_both_sides},
    };
    for (auto& t : tests) {
        std::cout << "[ RUN  ] " << t.name << std::endl;
        long before = g_failures.load();
        t.fn();
        std::cout << (g_failures.load() == before ? "[  OK  ] " : "[ BAD  ] ") << t.name << std::endl;
    }
    if (g_failures.load() == 0) { std::cout << "\nall checks passed" << std::endl; return 0; }
    std::cerr << "\n" << g_failures.load() << " CHECK(s) FAILED" << std::endl;
    return 1;
}

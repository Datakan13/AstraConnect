// SortedBook (api/orderbook/topBook.hpp) at its limits. topBookTest covers ordinary
// drift; this covers what it never reaches:
//
//   1. level_capacity        — MAX_LEVELS levels fit, the next new level is refused
//                              and leaves the book untouched; updates and removals
//                              of existing levels still work when full.
//   2. block_capacity        — levels 128 ticks apart each need a block; the
//                              MAX_BLOCKS + 1st is refused.
//   3. clear_and_reuse       — clear() empties the book and it recentres anywhere.
//   4. rounding_and_edges    — off-tick prices land on the nearest tick, top(0),
//                              removals of unknown levels.
//   5. differential_extremes — random updates against std::map with jumps of up to
//                              millions of ticks and both capacity limits in play,
//                              for several tick sizes.
//   6. concurrent_readers    — one writer and several readers sharing a Spinlock, as
//                              Orderbook does; every read is a sorted, consistent book.
//
// ASTRA_STRESS_SCALE=N divides the step counts of 5 and 6 by N.
#include <api/orderbook/topBook.hpp>
#include <AstraLib/Atomic/spinlock.hpp>
#include "testHarness.hpp"

#include <algorithm>
#include <atomic>
#include <map>
#include <memory>
#include <random>
#include <thread>
#include <vector>

using AstraConnect::Utils::Fixed;
using AstraConnect::Utils::FIXED_SCALE;

template <bool IsBid>
using Book = SortedBook<IsBid>;

constexpr Fixed TICK = FIXED_SCALE / 10;

template <bool IsBid>
static std::unique_ptr<Book<IsBid>> makeBook(Fixed tick = TICK) {
    auto book = std::make_unique<Book<IsBid>>();
    book->setTickSize(tick);
    return book;
}

// Best first and strictly monotone, no empty levels
template <bool IsBid>
static bool sorted(const std::vector<Entry>& got, size_t n) {
    for (size_t i = 0; i < n; ++i) {
        if (got[i].volume <= 0) return false;
        if (i && (IsBid ? got[i].price >= got[i - 1].price : got[i].price <= got[i - 1].price)) return false;
    }
    return true;
}

template <bool IsBid>
static void levelCapacity() {
    auto book = makeBook<IsBid>();
    constexpr int MAX = Book<IsBid>::MAX_LEVELS;
    const Fixed base = 50000 * FIXED_SCALE;
    for (int i = 0; i < MAX; ++i)
        CHECK_CTX(book->update(base + i * TICK, FIXED_SCALE + i), "level " << i << " of " << MAX);
    CHECK(book->levelCount() == size_t(MAX));

    CHECK_CTX(!book->update(base + MAX * TICK, FIXED_SCALE), "a full side must refuse a new level");
    CHECK_CTX(!book->update(base - TICK, FIXED_SCALE), "below the range too");
    CHECK(book->levelCount() == size_t(MAX));
    Entry e;
    CHECK(!book->find(base + MAX * TICK, e));

    // Existing levels are still writable, and a removal frees room
    CHECK(book->update(base, 7 * FIXED_SCALE));
    CHECK(book->find(base, e) && e.volume == 7 * FIXED_SCALE);
    CHECK(book->update(base + 5 * TICK, 0));
    CHECK(book->update(base + MAX * TICK, FIXED_SCALE));
    CHECK(book->levelCount() == size_t(MAX));

    std::vector<Entry> got(MAX + 10);
    const size_t n = book->top(got.data(), got.size());
    CHECK_CTX(n == size_t(MAX), n);
    CHECK(sorted<IsBid>(got, n));
    CHECK(got[0].price == (IsBid ? base + MAX * TICK : base));
}

static void test_level_capacity() {
    levelCapacity<true>();
    levelCapacity<false>();
}

template <bool IsBid>
static void blockCapacity() {
    auto book = makeBook<IsBid>();
    constexpr int BLOCKS = Book<IsBid>::MAX_BLOCKS;
    const Fixed base = 50000 * FIXED_SCALE;
    const Fixed stride = PriceLevel::TICKS * TICK;   // one level per block
    for (int i = 0; i < BLOCKS; ++i) CHECK_CTX(book->update(base + i * stride, FIXED_SCALE), "block " << i);
    CHECK_CTX(!book->update(base + BLOCKS * stride, FIXED_SCALE), "the 1025th block must be refused");
    CHECK(book->levelCount() == size_t(BLOCKS));

    // A new level inside an existing block needs no block
    CHECK(book->update(base + TICK, FIXED_SCALE));

    std::vector<Entry> got(BLOCKS + 10);
    const size_t n = book->top(got.data(), got.size());
    CHECK_CTX(n == size_t(BLOCKS + 1), n);
    CHECK(sorted<IsBid>(got, n));

    // Emptying a block returns it
    CHECK(book->update(base + 5 * stride, 0));
    CHECK(book->update(base + BLOCKS * stride, FIXED_SCALE));
}

static void test_block_capacity() {
    blockCapacity<true>();
    blockCapacity<false>();
}

static void test_clear_and_reuse() {
    auto book = makeBook<true>();
    for (int i = 0; i < 5000; ++i) book->update((60000 + i % 700) * FIXED_SCALE + (i % 10) * TICK, FIXED_SCALE);
    book->clear();
    CHECK(book->levelCount() == 0);
    Entry out[4];
    CHECK(book->top(out, 4) == 0);
    Entry e;
    CHECK(!book->find(60000 * FIXED_SCALE, e));

    // Far from the old window, in both directions
    for (Fixed price : {Fixed(1) * TICK, Fixed(900000) * FIXED_SCALE, Fixed(3) * TICK}) {
        CHECK(book->update(price, FIXED_SCALE));
        CHECK(book->find(price, e) && e.price == price);
    }
    CHECK(book->top(out, 4) == 3);
    CHECK(out[0].price == 900000 * FIXED_SCALE && out[2].price == TICK);
    // A cleared book refills to full capacity
    book->clear();
    int accepted = 0;
    for (int i = 0; i < Book<true>::MAX_LEVELS + 5; ++i) accepted += book->update(70000 * FIXED_SCALE + i * TICK, 1);
    CHECK_CTX(accepted == Book<true>::MAX_LEVELS, accepted);
}

static void test_rounding_and_edges() {
    auto book = makeBook<false>();
    // 100.04 rounds to 100.0, 100.06 to 100.1
    CHECK(book->update(10004000000, FIXED_SCALE));
    Entry e;
    CHECK(book->find(100 * FIXED_SCALE, e) && e.volume == FIXED_SCALE);
    CHECK(book->update(10006000000, 2 * FIXED_SCALE));
    CHECK(book->find(10010000000, e) && e.volume == 2 * FIXED_SCALE);
    CHECK(book->levelCount() == 2);

    Entry out[2];
    CHECK(book->top(out, 0) == 0);
    CHECK(book->update(12345 * FIXED_SCALE, 0));   // unknown level: ignored
    CHECK(book->levelCount() == 2);
    CHECK(book->update(100 * FIXED_SCALE, 0));
    CHECK(book->update(100 * FIXED_SCALE, 0));     // twice
    CHECK(book->levelCount() == 1);

    // A 1e-8 tick, as on low priced pairs
    auto fine = makeBook<true>(1);
    CHECK(fine->update(1234, 5) && fine->update(1235, 6) && fine->update(1233, 7));
    CHECK(fine->top(out, 2) == 2 && out[0].price == 1235 && out[1].price == 1234);
}

template <bool IsBid>
static void differential(uint32_t seed, Fixed tick, uint64_t steps) {
    auto book = makeBook<IsBid>(tick);
    std::map<int64_t, Fixed> reference;   // tick -> volume
    std::mt19937_64 rng(seed);
    int64_t mid = 1 << 22;
    std::vector<Entry> got(Book<IsBid>::MAX_LEVELS);
    uint64_t refusedLevels = 0, refusedBlocks = 0;

    for (uint64_t step = 0; step < steps; ++step) {
        // Mostly local noise, sometimes a wide scatter, rarely a jump far away
        const uint64_t r = rng() % 1000;
        if (r == 0) mid += int64_t(rng() % 4000000) - 2000000;
        if (mid < 1000) mid = 1000 + int64_t(rng() % 1000);
        const int64_t spread = r < 900 ? 300 : r < 990 ? 40000 : 3000000;
        int64_t t = mid + int64_t(rng() % (2 * spread + 1)) - spread;
        if (t < 1) t = 1;

        if (!reference.empty() && rng() % 100 < 48) {
            auto it = reference.lower_bound(t);
            if (it == reference.end()) --it;
            CHECK(book->update(it->first * tick, 0));
            reference.erase(it);
        } else {
            const Fixed volume = Fixed(1 + rng() % 100000);
            const bool known = reference.count(t);
            if (book->update(t * tick, volume)) {
                reference[t] = volume;
            } else {
                CHECK_CTX(!known, "update of an existing level refused at step " << step);
                if (reference.size() == size_t(Book<IsBid>::MAX_LEVELS)) refusedLevels++;
                else refusedBlocks++;
            }
        }

        if (step % 5000 == 0 || step == steps - 1) {
            CHECK_CTX(book->levelCount() == reference.size(), "step " << step);
            const size_t n = book->top(got.data(), got.size());
            if (n != reference.size()) {
                CHECK_CTX(false, "seed " << seed << " step " << step << ": " << n << " levels, expected " << reference.size());
                return;
            }
            size_t i = 0;
            bool ok = true;
            auto compare = [&](auto it) {
                if (ok && (got[i].price != it->first * tick || got[i].volume != it->second)) {
                    CHECK_CTX(false, "seed " << seed << " step " << step << " level " << i);
                    ok = false;
                }
                i++;
            };
            if constexpr (IsBid) { for (auto it = reference.rbegin(); it != reference.rend(); ++it) compare(it); }
            else { for (auto it = reference.begin(); it != reference.end(); ++it) compare(it); }
            if (!ok) return;
            // Spot-check lookups, hits and misses
            for (int k = 0; k < 8 && !reference.empty(); ++k) {
                auto it = reference.lower_bound(mid + int64_t(rng() % 20000) - 10000);
                if (it == reference.end()) continue;
                Entry e;
                CHECK(book->find(it->first * tick, e) && e.volume == it->second);
                if (!reference.count(it->first + 1)) CHECK(!book->find((it->first + 1) * tick, e));
            }
        }
    }
    std::cout << "         " << (IsBid ? "bids" : "asks") << " seed " << seed << " tick " << tick << ": "
              << reference.size() << " levels at the end, refused " << refusedLevels << " for levels, "
              << refusedBlocks << " for blocks" << std::endl;
}

static void test_differential_extremes() {
    const uint64_t steps = TestHarness::scaled(600000, 20000);
    uint32_t seed = 11;
    for (Fixed tick : {Fixed(1), Fixed(10), TICK, 10 * FIXED_SCALE}) {
        differential<true>(seed++, tick, steps);
        differential<false>(seed++, tick, steps);
    }
}

template <bool IsBid>
static void concurrentReaders() {
    const uint64_t writes = TestHarness::scaled(400000, 10000);
    auto book = makeBook<IsBid>();
    AstraLib::Atomic::Spinlock lock;
    std::atomic<bool> done{false};
    std::atomic<uint64_t> reads{0};

    std::vector<std::thread> readers;
    for (int r = 0; r < 3; ++r)
        readers.emplace_back([&] {
            std::vector<Entry> got(20);
            Entry best;
            while (!done.load(std::memory_order_acquire)) {
                size_t n, count;
                {
                    AstraLib::Atomic::SpinlockGuard guard(lock);
                    n = book->top(got.data(), got.size());
                    count = book->levelCount();
                }
                CHECK_CTX(n == std::min<size_t>(count, got.size()), n << " of " << count);
                CHECK(sorted<IsBid>(got, n));
                {
                    AstraLib::Atomic::SpinlockGuard guard(lock);
                    if (book->top(&best, 1) == 1) {
                        Entry same;
                        CHECK(book->find(best.price, same) && same.volume == best.volume);
                    }
                }
                reads.fetch_add(1, std::memory_order_relaxed);
            }
        });

    std::mt19937 rng(5);
    int64_t mid = 800000;
    for (uint64_t i = 0; i < writes; ++i) {
        mid += int(rng() % 7) - 3;
        const int64_t t = mid + int64_t(rng() % 2001) - 1000;
        const Fixed volume = rng() % 2 ? 0 : Fixed(1 + rng() % 1000);
        AstraLib::Atomic::SpinlockGuard guard(lock);
        book->update(t * TICK, volume);
    }
    done.store(true, std::memory_order_release);
    for (auto& t : readers) t.join();
    CHECK(reads.load() > 0);
}

static void test_concurrent_readers() {
    concurrentReaders<true>();
    concurrentReaders<false>();
}

int main() {
    return TestHarness::run({
        {"level_capacity",        test_level_capacity},
        {"block_capacity",        test_block_capacity},
        {"clear_and_reuse",       test_clear_and_reuse},
        {"rounding_and_edges",    test_rounding_and_edges},
        {"differential_extremes", test_differential_extremes},
        {"concurrent_readers",    test_concurrent_readers},
    });
}

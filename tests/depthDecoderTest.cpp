// The depth diff path in front of the orderbook: DepthDecoder, OrderbookArrayHolder
// and OrderbookSnapshotIncoming. None of it needs a connection.
//
//   1. decodes_combined_stream — levels, counts and U/u/pu come out as sent.
//   2. drops_bad_events        — empty, malformed, short pairs, non-string values and
//                                events over DEPTH_EVENT_CAPACITY are dropped, and
//                                the slot goes back every time.
//   3. slot_exhaustion         — with every slot still queued the next event drops
//                                instead of blocking; one release makes room again.
//   4. snapshot_incoming       — overflow is dropped, reset() rewinds both cursors.
//   5. spsc_slot_ownership     — the stream thread decodes while a slow orderbook
//                                thread consumes and releases. A slot is never handed
//                                out while still owned, and every delivered event
//                                still holds its own levels.
//
// ASTRA_STRESS_SCALE=N divides the event count of 5 by N.
#include <api/orderbook/decodeEntries.hpp>
#include <api/orderbook/orderbookSnapshotIncoming.hpp>
#include <AstraLib/Atomic/cpuRelax.hpp>
#include <AstraLib/Buffers/atomicRingBuffer.hpp>
#include "testHarness.hpp"

#include <array>
#include <atomic>
#include <memory>
#include <string>
#include <thread>
#include <vector>

using AstraConnect::Utils::Fixed;
using AstraConnect::Utils::FIXED_SCALE;
using AstraConnect::Utils::formatFixed;

static std::string levels(const std::vector<std::pair<std::string, std::string>>& pairs) {
    std::string out = "[";
    for (size_t i = 0; i < pairs.size(); ++i) {
        if (i) out += ",";
        out += "[\"" + pairs[i].first + "\",\"" + pairs[i].second + "\"]";
    }
    return out + "]";
}

static std::string depthEvent(int64_t U, int64_t u, int64_t pu, const std::string& bids, const std::string& asks) {
    return R"({"stream":"btcusdt@depth","data":{"e":"depthUpdate","E":1700000000000,"T":1700000000000,"s":"BTCUSDT","U":)" +
           std::to_string(U) + R"(,"u":)" + std::to_string(u) + R"(,"pu":)" + std::to_string(pu) +
           R"(,"b":)" + bids + R"(,"a":)" + asks + "}}";
}

static OrderbookArrays decode(DepthDecoder& decoder, simdjson::ondemand::parser& parser, const std::string& json) {
    simdjson::padded_string padded(json);
    return decoder(padded, parser);
}

// Slots that can be acquired right now; leaves the holder as it found it
static int freeSlots(OrderbookArrayHolder& holder) {
    std::vector<int> taken;
    for (int s; (s = holder.acquire()) >= 0;) taken.push_back(s);
    for (int s : taken) holder.release(s);
    return int(taken.size());
}

static void test_decodes_combined_stream() {
    auto holder = std::make_unique<OrderbookArrayHolder>();
    DepthDecoder decoder{holder.get()};
    simdjson::ondemand::parser parser;

    OrderbookArrays a = decode(decoder, parser,
        depthEvent(100, 105, 99, levels({{"84523.10", "1.5"}, {"84523.00", "0"}}), levels({{"84524", "0.003"}})));
    CHECK(!a.dropped());
    CHECK(a.slot >= 0 && a.slot < OrderbookArrayHolder::SLOTS);
    CHECK(a.U == 100 && a.u == 105 && a.pu == 99);
    CHECK_CTX(a.bidCount == 2 && a.askCount == 1, a.bidCount << " bids, " << a.askCount << " asks");
    if (!a.dropped()) {
        CHECK((*a.bidArray)[0].price == 8452310000000 && (*a.bidArray)[0].volume == 150000000);
        CHECK((*a.bidArray)[1].price == 8452300000000 && (*a.bidArray)[1].volume == 0);
        CHECK((*a.askArray)[0].price == 8452400000000 && (*a.askArray)[0].volume == 300000);
        holder->release(a.slot);
    }

    // Empty sides are a valid event, and a third field in a pair is ignored
    OrderbookArrays b = decode(decoder, parser, depthEvent(1, 2, 0, "[]", R"([["5","6","x"]])"));
    CHECK(!b.dropped() && b.bidCount == 0 && b.askCount == 1);
    if (!b.dropped()) holder->release(b.slot);

    // Exactly DEPTH_EVENT_CAPACITY levels fit
    std::vector<std::pair<std::string, std::string>> full;
    for (int i = 0; i < DEPTH_EVENT_CAPACITY; ++i) full.push_back({std::to_string(1000 + i), "1"});
    OrderbookArrays c = decode(decoder, parser, depthEvent(1, 2, 0, levels(full), levels(full)));
    CHECK(!c.dropped() && c.bidCount == DEPTH_EVENT_CAPACITY && c.askCount == DEPTH_EVENT_CAPACITY);
    if (!c.dropped()) {
        CHECK((*c.askArray)[DEPTH_EVENT_CAPACITY - 1].price == Fixed(1000 + DEPTH_EVENT_CAPACITY - 1) * FIXED_SCALE);
        holder->release(c.slot);
    }
    CHECK(freeSlots(*holder) == OrderbookArrayHolder::SLOTS);
}

static void test_drops_bad_events() {
    auto holder = std::make_unique<OrderbookArrayHolder>();
    DepthDecoder decoder{holder.get()};
    simdjson::ondemand::parser parser;

    std::vector<std::pair<std::string, std::string>> over;
    for (int i = 0; i <= DEPTH_EVENT_CAPACITY; ++i) over.push_back({std::to_string(1000 + i), "1"});
    const std::string ok = levels({{"1", "1"}});

    const std::vector<std::pair<const char*, std::string>> bad = {
        {"empty message after a reconnect", ""},
        {"not json", "garbage"},
        {"no data object", R"({"stream":"x"})"},
        {"data not an object", R"({"data":[1,2]})"},
        {"short pair", depthEvent(1, 2, 0, R"([["1"]])", ok)},
        {"empty pair", depthEvent(1, 2, 0, ok, R"([[]])")},
        {"number instead of string", depthEvent(1, 2, 0, R"([[1.5,"1"]])", ok)},
        {"price not a decimal", depthEvent(1, 2, 0, R"([["1e5","1"]])", ok)},
        {"price overflows int64", depthEvent(1, 2, 0, R"([["100000000000","1"]])", ok)},
        {"level not an array", depthEvent(1, 2, 0, R"(["1","2"])", ok)},
        {"bids over capacity", depthEvent(1, 2, 0, levels(over), ok)},
        {"asks over capacity", depthEvent(1, 2, 0, ok, levels(over))},
        {"missing u", R"({"data":{"U":1,"pu":0,"b":[],"a":[]}})"},
        {"negative id", R"({"data":{"U":-1,"u":2,"pu":0,"b":[],"a":[]}})"},
        {"truncated", depthEvent(1, 2, 0, ok, ok).substr(0, 60)},
    };
    for (const auto& [why, json] : bad) {
        OrderbookArrays a = decode(decoder, parser, json);
        CHECK_CTX(a.dropped(), why);
        CHECK_CTX(a.slot == -1, why);
        CHECK_CTX(freeSlots(*holder) == OrderbookArrayHolder::SLOTS, why << ": slot leaked");
    }
}

static void test_slot_exhaustion() {
    auto holder = std::make_unique<OrderbookArrayHolder>();
    DepthDecoder decoder{holder.get()};
    simdjson::ondemand::parser parser;
    const std::string json = depthEvent(1, 2, 0, levels({{"1", "1"}}), "[]");

    std::vector<int> queued;
    std::array<bool, OrderbookArrayHolder::SLOTS> seen{};
    for (int i = 0; i < OrderbookArrayHolder::SLOTS; ++i) {
        OrderbookArrays a = decode(decoder, parser, json);
        CHECK(!a.dropped());
        if (a.dropped()) return;
        CHECK_CTX(!seen[a.slot], "slot " << a.slot << " handed out twice");
        seen[a.slot] = true;
        queued.push_back(a.slot);
    }
    OrderbookArrays full = decode(decoder, parser, json);
    CHECK_CTX(full.dropped(), "every slot is queued, the event must drop");

    holder->release(queued.back());
    queued.pop_back();
    OrderbookArrays again = decode(decoder, parser, json);
    CHECK(!again.dropped());
    if (!again.dropped()) queued.push_back(again.slot);
    for (int s : queued) holder->release(s);
    CHECK(freeSlots(*holder) == OrderbookArrayHolder::SLOTS);
}

static void test_snapshot_incoming() {
    auto snap = std::make_unique<OrderbookSnapshotIncoming>();
    for (int i = 0; i < 9000; ++i) {
        snap->registerEntry(Fixed(100000 - i) * FIXED_SCALE, FIXED_SCALE, true);
        if (i < 3) snap->registerEntry(Fixed(100001 + i) * FIXED_SCALE, 2 * FIXED_SCALE, false);
    }
    int bids = 0;
    for (Entry e = snap->getBid(); e.price != 0; e = snap->getBid()) {
        CHECK_CTX(e.price == Fixed(100000 - bids) * FIXED_SCALE, "bid " << bids << " is " << formatFixed(e.price));
        bids++;
    }
    CHECK_CTX(bids == 8192, "overflow must be dropped, not wrapped: " << bids);
    int asks = 0;
    for (Entry e = snap->getAsk(); e.price != 0; e = snap->getAsk()) asks++;
    CHECK(asks == 3);
    CHECK(snap->getBid().price == 0 && snap->getAsk().price == 0);

    // A resync refills from scratch and reads from the start
    snap->reset();
    CHECK(snap->getBid().price == 0);
    snap->registerEntry(7 * FIXED_SCALE, 1, true);
    snap->registerEntry(8 * FIXED_SCALE, 1, false);
    CHECK(snap->getBid().price == 7 * FIXED_SCALE);
    CHECK(snap->getAsk().price == 8 * FIXED_SCALE);
}

// Event u carries levelsFor(u) bids and asks, derived from u alone, so the consumer
// can tell whether a slot was overwritten while the event was queued.
static int levelsFor(int64_t u) { return 1 + int(u % 9); }
static Fixed bidPrice(int64_t u, int i) { return (Fixed(50000 + u % 5000) * 10 - i) * (FIXED_SCALE / 10); }
static Fixed volumeFor(int64_t u, int i) { return Fixed(u) * 1000 + i; }

static std::string eventFor(int64_t u) {
    std::vector<std::pair<std::string, std::string>> b, a;
    for (int i = 0; i < levelsFor(u); ++i) {
        b.push_back({formatFixed(bidPrice(u, i)), formatFixed(volumeFor(u, i))});
        a.push_back({formatFixed(bidPrice(u, i) + 100 * FIXED_SCALE), formatFixed(volumeFor(u, i))});
    }
    return depthEvent(u, u, u - 1, levels(b), levels(a));
}

static void test_spsc_slot_ownership() {
    const int64_t EVENTS = int64_t(TestHarness::scaled(200000, 5000));
    auto holder = std::make_unique<OrderbookArrayHolder>();
    // The stream holder's bufferOut is 1024 deep, deeper than the 128 slots
    auto queue = std::make_unique<AstraLib::Buffers::AtomicRingBuffer<OrderbookArrays, 1024>>();
    std::array<std::atomic<int>, OrderbookArrayHolder::SLOTS> owned{};
    std::atomic<bool> producerDone{false};
    std::atomic<int64_t> dropped{0}, malformed{0};

    std::thread stream([&] {
        DepthDecoder decoder{holder.get()};
        simdjson::ondemand::parser parser;
        for (int64_t u = 1; u <= EVENTS; ++u) {
            // Every 997th message is cut short, like a frame lost mid-read
            std::string json = eventFor(u);
            const bool broken = u % 997 == 0;
            if (broken) json.resize(json.size() / 2);
            OrderbookArrays a = decode(decoder, parser, json);
            if (a.dropped()) {
                (broken ? malformed : dropped).fetch_add(1, std::memory_order_relaxed);
                continue;
            }
            CHECK_CTX(!broken, "event " << u << " was cut short and still decoded");
            CHECK_CTX(owned[a.slot].exchange(1, std::memory_order_acq_rel) == 0,
                      "slot " << a.slot << " handed out while still queued");
            queue->enqueue(std::move(a));
        }
        producerDone.store(true, std::memory_order_release);
    });

    int64_t delivered = 0, lastU = 0;
    for (;;) {
        OrderbookArrays a;
        if (!queue->tryDequeue(a)) {
            if (producerDone.load(std::memory_order_acquire) && queue->isEmpty()) break;
            AstraLib::Atomic::cpuRelax();
            continue;
        }
        CHECK_CTX(a.u > lastU, "out of order: " << a.u << " after " << lastU);
        lastU = a.u;
        const int n = levelsFor(a.u);
        CHECK_CTX(a.bidCount == n && a.askCount == n, "event " << a.u << ": " << a.bidCount << " levels");
        for (int i = 0; i < n && i < a.bidCount; ++i) {
            const Entry& bid = (*a.bidArray)[i];
            if (bid.price != bidPrice(a.u, i) || bid.volume != volumeFor(a.u, i)) {
                CHECK_CTX(false, "event " << a.u << " level " << i << " overwritten while queued");
                break;
            }
        }
        // Slow consumer for stretches, so the decoder runs out of slots and drops
        if ((a.u / 4096) % 2) for (int k = 0; k < 400; ++k) AstraLib::Atomic::cpuRelax();
        owned[a.slot].store(0, std::memory_order_release);
        holder->release(a.slot);
        delivered++;
    }
    stream.join();

    const int64_t expectedMalformed = EVENTS / 997;
    CHECK_CTX(malformed.load() == expectedMalformed, malformed.load() << " vs " << expectedMalformed);
    CHECK_CTX(delivered + dropped.load() + malformed.load() == EVENTS,
              delivered << " + " << dropped.load() << " + " << malformed.load() << " != " << EVENTS);
    CHECK(freeSlots(*holder) == OrderbookArrayHolder::SLOTS);
    std::cout << "         " << EVENTS << " events, " << delivered << " delivered, " << dropped.load()
              << " dropped for want of a slot" << std::endl;
}

int main() {
    return TestHarness::run({
        {"decodes_combined_stream", test_decodes_combined_stream},
        {"drops_bad_events",        test_drops_bad_events},
        {"slot_exhaustion",         test_slot_exhaustion},
        {"snapshot_incoming",       test_snapshot_incoming},
        {"spsc_slot_ownership",     test_spsc_slot_ownership},
    });
}

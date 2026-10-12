#pragma once
#include <memory>
#include <span>
#include <cctype>
#include <AstraLib/AstraLib.hpp>
#include "model/entry.hpp"
#include "model/exchangeInfo.hpp"
#include "api/orderbook/topBook.hpp"
#include "api/orderbook/orderbookSnapshotIncoming.hpp"
#include "model/error.hpp"
#include "api/orderbook/orderbookArrays.hpp"
#include "api/orderbook/decodeEntries.hpp"
#include "core/streams/websocketStreamHolder.hpp"
#include "manager/apiManager.hpp"
#include "api/futures/api.hpp"
#include "api/common/error/includeErrors.hpp"
#include "core/types/status.hpp"


// Local copy of a futures order book, kept from a REST snapshot plus the depth diff
// stream. Each side is a SortedBook: a diff finds its level by price through the
// side's bitmap blocks, so the book is always sorted and the best levels are a
// highest/lowest-set-bit away.
//
// All reads take the same lock as the update thread and copy out, so the results are
// a consistent view of one moment of the book.
class APIManager::Orderbook {
    // Declared before websocketStream: its thread starts decoding into these slots at once
    std::unique_ptr<OrderbookArrayHolder> arrayHolder;
    WebsocketStreamHolder<OrderbookArrays,DepthDecoder> websocketStream;
    OrderbookSnapshotIncoming orderbookSnapshot;
    APIManager::Futures::API& futuresApi;
    AstraLib::Atomic::Spinlock updatingLock;
    std::thread thread;
    std::string pair;
    AstraLib::Atomic::PaddedAtomic<bool> running{true};
    AstraLib::Atomic::PaddedAtomic<bool> synced{false};
    // Why the book stopped. Owned here; executionLoop has no caller to return to.
    std::unique_ptr<FetchError> lastError;

    // Heap allocated: together they are close to a megabyte
    std::unique_ptr<SortedBook<true>> bidBook;
    std::unique_ptr<SortedBook<false>> askBook;

    // Final update id applied to the book; each event must continue from it.
    int64_t updateID{0};

    static constexpr int snapshotAttempts = 3;
    // Snapshot/stream alignment failures in a row before giving up.
    static constexpr int maxFailedSyncs = 5;

    // Records why we are stopping and halts the loop. Throwing from executionLoop
    // escaped the thread and called std::terminate instead of reaching anyone.
    void stop(FetchError err) {
        lastError = std::make_unique<FetchError>(std::move(err));
        synced.value.store(false,std::memory_order_release);
        running.value.store(false,std::memory_order_release);
    }

    // Returns false when a new level does not fit its side.
    bool processEntry(const Entry& entry, bool isBid) {
        return isBid ? bidBook->update(entry.price, entry.volume) : askBook->update(entry.price, entry.volume);
    }

    // Replaces the whole book with the fetched snapshot. The snapshot used to be
    // applied on top of the old book, so levels removed while we were out of sync
    // stayed forever.
    void applySnapshot() {
        AstraLib::Atomic::SpinlockGuard guard(updatingLock);
        bidBook->clear();
        askBook->clear();
        // A snapshot holds at most 1000 levels per side, so neither side can fill up
        for(Entry entry = orderbookSnapshot.getBid(); entry.price != 0; entry = orderbookSnapshot.getBid()) {
            processEntry(entry,true);
        }
        for(Entry entry = orderbookSnapshot.getAsk(); entry.price != 0; entry = orderbookSnapshot.getAsk()) {
            processEntry(entry,false);
        }
        updateID = orderbookSnapshot.updateId;
    }

    // Fetches a fresh snapshot, retrying with a short pause, and applies it.
    FetchError resync() {
        synced.value.store(false,std::memory_order_release);
        FetchError error = futuresApi.fetchOrderbookSnapshot(pair,orderbookSnapshot);
        for(int attempt = 1; error && attempt < snapshotAttempts; attempt++) {
            if(!running.value.load(std::memory_order_acquire)) return error;
            std::this_thread::sleep_for(std::chrono::milliseconds(250 * attempt));
            error = futuresApi.fetchOrderbookSnapshot(pair,orderbookSnapshot);
        }
        if(error) return error;
        applySnapshot();
        return FetchError(APIError::SUCCESS);
    }

    // Applies one depth event. Returns false if a side ran out of room; the caller
    // resyncs, which also drops levels that drifted in from beyond the snapshot depth.
    bool applyUpdate(const OrderbookArrays& arrays) {
        AstraLib::Atomic::SpinlockGuard guard(updatingLock);
        for(int64_t i = 0; i < arrays.bidCount; i++) {
            if(!processEntry((*arrays.bidArray)[i],true)) return false;
        }
        for(int64_t i = 0; i < arrays.askCount; i++) {
            if(!processEntry((*arrays.askArray)[i],false)) return false;
        }
        updateID = arrays.u;
        return true;
    }

    // Waits until a depth event is buffered. Never blocks shutdown: returns false once
    // the book is stopping, or the stream has given up (recorded as the stop reason).
    bool waitForUpdate() {
        while(websocketStream.bufferOut.isEmpty()) {
            if(!running.value.load(std::memory_order_acquire)) return false;
            if(!websocketStream.isRunning()) {
                FetchError* err = websocketStream.getStatus().second;
                stop(FetchError(APIError::DISCONNECTED, err ? err->getErrorMsg() : std::string("Depth stream closed")));
                return false;
            }
            AstraLib::Atomic::cpuRelax();
        }
        return true;
    }

    // Every event taken from the stream must be released once it has been looked at
    bool nextUpdate(OrderbookArrays& out) {
        if(!waitForUpdate()) return false;
        out = websocketStream.getLatestData();
        return true;
    }

    void release(OrderbookArrays& arrays) {
        if(arrays.slot >= 0) arrayHolder->release(arrays.slot);
        arrays.slot = -1;
    }

    // Binance's procedure: drop events the snapshot already covers (u < lastUpdateId),
    // then the first applied event must straddle it (U <= lastUpdateId <= u).
    // Returns false on a gap between snapshot and stream, a dropped event, or when stopping.
    bool applyFirstUpdate() {
        OrderbookArrays arrays;
        while(nextUpdate(arrays)) {
            if(arrays.dropped()) return false;
            if(arrays.u < updateID) {
                release(arrays);
                continue;
            }
            bool applied = arrays.U <= updateID && applyUpdate(arrays);
            release(arrays);
            return applied;
        }
        return false;
    }

    // The exchange info symbol is upper case; the stream name is lower case
    FetchError loadTickSize() {
        std::string symbol = pair;
        for(char& c : symbol) c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
        PairInfo info{};
        if(FetchError error = futuresApi.fetchExchangeInfoForPair(symbol,info)) return error;
        if(info.tickSize <= 0) return FetchError(APIError::BAD_FIELD,"Exchange info has no tick size for " + symbol);
        bidBook->setTickSize(info.tickSize);
        askBook->setTickSize(info.tickSize);
        return FetchError(APIError::SUCCESS);
    }

    void executionLoop() {
        if(FetchError error = loadTickSize()) {
            stop(std::move(error)); return;
        }
        // Wait for the stream to deliver before taking the snapshot, so the snapshot
        // cannot be older than the first buffered event.
        if(!waitForUpdate()) return;
        int failedSyncs = 0;
        OrderbookArrays arrays;
        while(running.value.load(std::memory_order_acquire)) {
            if(FetchError error = resync()) {
                stop(std::move(error)); return;
            }
            if(!applyFirstUpdate()) {
                if(!running.value.load(std::memory_order_acquire)) return;
                if(++failedSyncs == maxFailedSyncs) {
                    stop(FetchError(APIError::ORDERBOOK_OUTDATED,std::string("Snapshot could not be aligned with the depth stream")));
                    return;
                }
                continue;
            }
            failedSyncs = 0;
            synced.value.store(true,std::memory_order_release);
            // Each event must continue exactly where the previous one ended; any
            // break (a dropped event, a reconnect) falls through to a resync.
            while(nextUpdate(arrays)) {
                bool applied = !arrays.dropped() && arrays.pu == updateID && applyUpdate(arrays);
                release(arrays);
                if(!applied) break;
            }
        }
    }

    public:
    // The reason the book stopped, or nullptr while it is still running.
    // stop() writes lastError before clearing running, so the acquire here makes it visible.
    FetchError* error() {
        if(running.value.load(std::memory_order_acquire)) return nullptr;
        return lastError.get();
    }

    // True while the book is aligned with the stream. False before the first sync,
    // during a resync and after a stop; the levels are not current then.
    bool isSynced() {
        return synced.value.load(std::memory_order_acquire);
    }

    // Copies up to out.size() levels into out, best first (highest bid, lowest ask).
    // Returns how many were written. Prices and volumes are in 1e-8 units, exactly as sent.
    size_t getTopBids(std::span<Entry> out) {
        AstraLib::Atomic::SpinlockGuard guard(updatingLock);
        return bidBook->top(out.data(), out.size());
    }

    size_t getTopAsks(std::span<Entry> out) {
        AstraLib::Atomic::SpinlockGuard guard(updatingLock);
        return askBook->top(out.data(), out.size());
    }

    // False when that side is empty.
    bool getBestBid(Entry& out) {
        return getTopBids(std::span<Entry>(&out, 1)) == 1;
    }

    bool getBestAsk(Entry& out) {
        return getTopAsks(std::span<Entry>(&out, 1)) == 1;
    }

    // Looks a single price up directly in the sorted book.
    PriceValid getEntryWithPrice(AstraConnect::Utils::Fixed price,Entry& out,bool isBid) {
        AstraLib::Atomic::SpinlockGuard guard(updatingLock);
        const bool found = isBid ? bidBook->find(price,out) : askBook->find(price,out);
        return found ? PriceValid::VALID : PriceValid::NOT_VALID;
    }

    Orderbook(APIManager& base_,std::string pair_,APIManager::Futures::API& api) :
    arrayHolder(std::make_unique<OrderbookArrayHolder>()),
    websocketStream(DepthDecoder{arrayHolder.get()},base_.hostFuturesWebsocket,"/public/stream?streams="+ pair_ +"@depth",base_.websocketParser),
    futuresApi(api), pair(pair_),
    bidBook(std::make_unique<SortedBook<true>>()), askBook(std::make_unique<SortedBook<false>>()) {
        thread = std::thread(&Orderbook::executionLoop,this);
    };
    ~Orderbook() {
        running.value.store(false, std::memory_order_release);
        if(thread.joinable()) thread.join();
    }
};

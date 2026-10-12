#pragma once
#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <AstraLib/Atomic/cpuRelax.hpp>
#include <memory>
#include <AstraLib/Atomic/paddedAtomic.hpp>
#include <type_traits>
namespace AstraLib {
namespace Buffers {

    
template<typename T>
// seq == ticket write 
// seq == ticket + 1 read
struct alignas(64) Slot {
    std::atomic<int64_t> seq{0};
    // alignas(64) is what keeps slots off each other's cache lines: every slot starts
    // on a line boundary and sizeof(Slot) rounds up to a multiple of 64, so no two
    // slots ever share one. A payload wider than a line is therefore fine, it just
    // makes each slot span more than one. Bound below is only a sanity guard.
    static_assert(sizeof(T) <= 4096, "Payload too large for ring buffer slot");
    T data;
};

template<typename A,std::size_t SIZE,std::size_t BATCH_SIZE = 50>
class AtomicRingBuffer {
    static_assert(SIZE && !(SIZE & (SIZE - 1)),"Size must be a power of two");
    static_assert(SIZE >= 2,"Size must be at least 2");
    std::array<A,BATCH_SIZE> batchDequeueArray;
    Slot<A> buffer[SIZE];
    AstraLib::Atomic::PaddedAtomic<int64_t> writeTicket{0};
    AstraLib::Atomic::PaddedAtomic<int64_t> readTicket{0};
    private:
    void prepareBuffer(){
        for(int i = 0; i < SIZE;i++) {
            buffer[i].seq.store(i,std::memory_order_relaxed);
        }
    }
    public:
    void enqueue(A&& data) {
        uint64_t ticket = writeTicket.value.fetch_add(1,std::memory_order_acq_rel);
        int index = ticket & (SIZE - 1); 
        while(buffer[index].seq.load(std::memory_order_acquire) != ticket) {
            AstraLib::Atomic::cpuRelax();
        }
        buffer[index].data = std::move(data);
        buffer[index].seq.store(ticket+1,std::memory_order_release);
    }

    void enqueue(const A& data){
        uint64_t ticket = writeTicket.value.fetch_add(1,std::memory_order_acq_rel);
        int index = ticket & (SIZE - 1); 
        while(buffer[index].seq.load(std::memory_order_acquire) != ticket) {
            AstraLib::Atomic::cpuRelax();
        }
        buffer[index].data = data;
        buffer[index].seq.store(ticket+1,std::memory_order_release);
    }

    A dequeue() {
        uint64_t ticket = readTicket.value.fetch_add(1,std::memory_order_acq_rel);
        int index = ticket & (SIZE - 1);
        while(buffer[index].seq.load(std::memory_order_acquire) != ticket + 1) {
            AstraLib::Atomic::cpuRelax();
        }
        A data = std::move(buffer[index].data);
        // SIZE is capacity thus +SIZE for the next iteration
        buffer[index].seq.store(ticket+SIZE,std::memory_order_release);
        return data;
    }
    
    void clearBuffer(){
        writeTicket.value.store(0,std::memory_order_release);
        readTicket.value.store(0,std::memory_order_release);
        //Overwrites all the seq values so it's at the starting position again
        prepareBuffer();
    }
    
    // Takes all the fields of the given struct and directly writes them 
    template<typename... Args>
    void emplaceEnqueue(Args&&... args) {
        uint64_t ticket = writeTicket.value.fetch_add(1,std::memory_order_acq_rel);
        int index = ticket & (SIZE - 1); 
        while(buffer[index].seq.load(std::memory_order_acquire) != ticket) {
            AstraLib::Atomic::cpuRelax();
        }

        if constexpr (std::is_trivially_destructible_v<A>){
            new (&buffer[index].data) A(std::forward<Args>(args)...);
        } else {
            buffer[index].data.~A();
            new (&buffer[index].data) A(std::forward<Args>(args)...);
        }
        buffer[index].seq.store(ticket+1,std::memory_order_release);
    }

    // WARNING: only for single-threaded initialization before any enqueues/dequeues.
    // This bypasses ticket counters and synchronization logic.
    // Use strictly to preconfigure slot memory (e.g., wiring pointers).
    // Does not guarantee atomicity or visibility across threads.
    A* claimPtrNoSync(const int index){
        A* ptr = &buffer[index].data;
        return ptr;
    }

    //WARNING: only use in MPSC paths
    // A max of 50 can be dequeued at a time for default construction.
    // If count is bigger than BATCH_SIZE, it will be reduced to BATCH_SIZE.
    std::array<A,BATCH_SIZE>* batchDequeue(int count) {
        if(count > BATCH_SIZE) count = BATCH_SIZE;
        for(int i = 0; i< count; i++) {
            uint64_t ticket = readTicket.value.fetch_add(1,std::memory_order_acq_rel);
            int index = ticket & (SIZE - 1);
            while(buffer[index].seq.load(std::memory_order_acquire) != ticket + 1) {
                AstraLib::Atomic::cpuRelax();
            }
            batchDequeueArray[i] = std::move(buffer[index].data);
            // SIZE is capacity thus +SIZE for the next iteration
            buffer[index].seq.store(ticket+SIZE,std::memory_order_release);
        }
        return &batchDequeueArray;
    }

    // False if the buffer is full; never spins waiting for space and takes no ticket
    // on failure. Safe to mix with enqueue().
    bool tryEnqueue(const A& data) { return tryEnqueueImpl(data); }
    bool tryEnqueue(A&& data) { return tryEnqueueImpl(std::move(data)); }

    // False if the buffer is empty; never spins waiting for data and takes no ticket
    // on failure. Safe to mix with dequeue().
    bool tryDequeue(A& out) {
        int64_t ticket = readTicket.value.load(std::memory_order_relaxed);
        for(;;) {
            Slot<A>& slot = buffer[ticket & (SIZE - 1)];
            const int64_t diff = slot.seq.load(std::memory_order_acquire) - (ticket + 1);
            if(diff == 0) {
                if(readTicket.value.compare_exchange_weak(ticket, ticket + 1, std::memory_order_acq_rel)) {
                    out = std::move(slot.data);
                    slot.seq.store(ticket+SIZE,std::memory_order_release);
                    return true;
                }
            } else if(diff < 0) {
                // Not published yet: empty, or a producer is mid-write
                return false;
            } else {
                // Another consumer took this ticket first
                ticket = readTicket.value.load(std::memory_order_relaxed);
            }
        }
    }

    // Items currently queued, approximately: writeTicket - readTicket, clamped to [0, SIZE].
    // Blocking enqueue()/dequeue() take their ticket before they wait, so either side
    // can run ahead of what is actually in the slots.
    std::size_t approxSize() const {
        const int64_t read = readTicket.value.load(std::memory_order_acquire);
        const int64_t write = writeTicket.value.load(std::memory_order_acquire);
        if(write <= read) return 0;
        const std::size_t size = static_cast<std::size_t>(write - read);
        return size > SIZE ? SIZE : size;
    }

    bool isEmpty() {
        return writeTicket.value.load(std::memory_order_acquire) == readTicket.value.load(std::memory_order_acquire);
    }

    private:
    template<typename U>
    bool tryEnqueueImpl(U&& data) {
        int64_t ticket = writeTicket.value.load(std::memory_order_relaxed);
        for(;;) {
            Slot<A>& slot = buffer[ticket & (SIZE - 1)];
            const int64_t diff = slot.seq.load(std::memory_order_acquire) - ticket;
            if(diff == 0) {
                if(writeTicket.value.compare_exchange_weak(ticket, ticket + 1, std::memory_order_acq_rel)) {
                    slot.data = std::forward<U>(data);
                    slot.seq.store(ticket+1,std::memory_order_release);
                    return true;
                }
            } else if(diff < 0) {
                // The slot still holds the item from one lap ago: full
                return false;
            } else {
                // Another producer took this ticket first
                ticket = writeTicket.value.load(std::memory_order_relaxed);
            }
        }
    }

    public:
    AtomicRingBuffer() {
        prepareBuffer();
    }
};

} // namespace Buffers
} // namespace AstraLib

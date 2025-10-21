#pragma once 
#include <linux/futex.h>
#include <sys/syscall.h>
#include <type_traits>
#include <unistd.h>
#include <atomic> 
#include <thread>

namespace AstraLib{
namespace Atomic {

 

// Primary template: default = false
template <typename T>
struct is_atomic_type : std::false_type {};

// Specialization for std::atomic<...>
template <typename U>
struct is_atomic_type<std::atomic<U>> : std::true_type {};

// Convenience variable
template <typename T>
inline constexpr bool is_atomic_type_v = is_atomic_type<T>::value;


template<typename T = std::atomic<bool>>
class alignas(64) AtomicFutex {
    private:
    std::atomic<int> futex_val;

    public:
    T customValue;
    private:
    T waitValue{};
    T wakeValue{};
    bool customValueUsed;
    public:
    void setCustomValueForWait(T a){
        if constexpr (is_atomic_type_v<T>) {
            waitValue.store(a.load(std::memory_order_acquire));
        } else {
            waitValue = a;
        }
        
    }

    void setCustomValueForWake(T a){
        if constexpr (is_atomic_type_v<T>) {
            waitValue.store(a.load(std::memory_order_acquire));
        } else {
            waitValue = a;
        }
    }
    
    // Will wait unless there has been a wake call
    void wait() {
        int expected = futex_val.load(std::memory_order_seq_cst);

        while (true) {
            int res = syscall(SYS_futex, &futex_val, FUTEX_WAIT, expected, nullptr, nullptr, 0);
            int current = futex_val.load(std::memory_order_acquire);
            if (current != expected) break; 
        }
    }

    // Will wake up 
    void wake() {
        futex_val.fetch_add(1, std::memory_order_acq_rel);
        std::atomic_thread_fence(std::memory_order_seq_cst);
        syscall(SYS_futex, &futex_val, FUTEX_WAKE, 1, nullptr, nullptr, 0);
    }
    
    AtomicFutex() {
        customValue.store(false,std::memory_order_release);
        customValueUsed = false;
        futex_val.store(0,std::memory_order_release);
    }
    AtomicFutex(T customValue_) : customValue(customValue_) {
        customValueUsed = true;
        futex_val.store(0,std::memory_order_release);
    }
};
   } // namespace Atomic
} // namespace AstraLib
#pragma once
#include <atomic>
#if defined(__x86_64__) || defined(__i386__)
#include <immintrin.h>
#endif

namespace AstraLib {
namespace Atomic {

// Spin-wait hint. _mm_pause on x86, yield on aarch64, a compiler barrier elsewhere.
inline void cpuRelax() noexcept {
#if defined(__x86_64__) || defined(__i386__)
    _mm_pause();
#elif defined(__aarch64__)
    asm volatile("yield" ::: "memory");
#else
    std::atomic_signal_fence(std::memory_order_seq_cst);
#endif
}

} // namespace Atomic
} // namespace AstraLib

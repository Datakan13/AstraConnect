#pragma once
// x86: TSC via rdtscp, fenced with cpuid. aarch64: the generic timer (CNTVCT_EL0),
// which counts at a fixed CNTFRQ_EL0 rather than the core clock, so "cycles" are
// timer ticks there. Other architectures have no Timer.
#if defined(__x86_64__) || defined(__i386__)
#define ASTRALIB_HAS_TIMER 1
#include <x86intrin.h>
#include <cpuid.h>
#elif defined(__aarch64__)
#define ASTRALIB_HAS_TIMER 1
#endif

#ifdef ASTRALIB_HAS_TIMER
#include <string>
#include <iostream>
#include <unistd.h>       // for sleep
#include <iostream>
#include <string>
#include <cstdint>

namespace AstraLib {
namespace Time {

class Timer {
private:
    unsigned aux;
    unsigned eax, ebx, ecx, edx;
    uint64_t startTime;
    double ghz;

#if defined(__aarch64__)
    static uint64_t readCounter() {
        uint64_t value;
        // isb keeps the read from being hoisted above earlier instructions
        asm volatile("isb; mrs %0, cntvct_el0" : "=r"(value) :: "memory");
        return value;
    }

    void setGHz() {
        uint64_t frequency;
        asm volatile("mrs %0, cntfrq_el0" : "=r"(frequency));
        ghz = frequency / 1e9; // ticks per ns
    }
#else
    void setGHz() {
        uint64_t t1 = __rdtscp(&aux);
        sleep(1); // 1 second sleep
        uint64_t t2 = __rdtscp(&aux);
        ghz = (t2 - t1) / 1e9; // cycles per ns = GHz
    }
#endif

public:
    Timer() {
        setGHz();  // auto-calibrate once at construction
    }

#if defined(__aarch64__)
    void start() {
        startTime = readCounter();
    }

    // Returns the number of timer ticks elapsed since start() was called
    inline uint64_t getTimeCycles(){
        return readCounter() - startTime;
    }
#else
    void start() {
        __cpuid(0, eax, ebx, ecx, edx);
        startTime = __rdtscp(&aux);
    }

    // Returns the number of cycles elapsed since start() was called
    inline uint64_t getTimeCycles(){
        uint64_t endTime = __rdtscp(&aux);
        __cpuid(0, eax, ebx, ecx, edx);
        uint64_t cycles = endTime - startTime;
        return cycles;
    }
#endif

    // Returns the number of nanoseconds elapsed since start() was called
    inline double getTimeNs(){
        uint64_t cycles = getTimeCycles();
        double ns = cycles / ghz;       // convert cycles → nanoseconds
        return ns;
    }

    void write(const std::string& label = "Timer") {
        uint64_t cycles = getTimeCycles();
        double ns = cycles / ghz;       // convert cycles → nanoseconds
        double us = ns / 1000.0;        // also microseconds

        std::cout << "[" << label << "] "
                  << cycles << " cycles, ~"
                  << ns << " ns (~"
                  << us << " µs)"
                  << std::endl;
    }

    
};

} // namespace Time
} // namespace AstraLib
#endif // ASTRALIB_HAS_TIMER

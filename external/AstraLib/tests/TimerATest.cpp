#include <AstraLib/Time/timer.hpp>
#include <chrono>
#include <thread>

// Tests concluded with a probably overhead from wait_cycles function around 100-500 cycles timer is working as inteded
void wait_cycles(uint64_t cycles) {
    uint64_t start  = __rdtsc();
    uint64_t target = start + cycles;
    while (__rdtsc() < target) {
        _mm_pause();  // friendly to CPU pipeline
    }
}


int main() {
    AstraLib::Time::Timer timer;
    timer.start();
    wait_cycles(6000);
    timer.write();
}
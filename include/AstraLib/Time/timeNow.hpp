#pragma once
#include <chrono>
#include <cstdint>

namespace AstraLib {
    namespace Time {
        inline uint64_t unixTimestampNS() {
            using namespace std::chrono;
            return duration_cast<nanoseconds>(
            system_clock::now().time_since_epoch()
            ).count();
        }
} // namespace Time
} // namespace AstraLib
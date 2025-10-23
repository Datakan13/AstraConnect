#pragma once
#include <cstdint>
class MarkPrice{
    public:
    int64_t timestamp;
    double indexPrice;
    double markPrice;
    double fundingRate;
    int64_t fundingTime;
};


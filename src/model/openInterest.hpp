#pragma once 
#include <cstdint>
class OpenInterest {
    public:
    uint64_t timestamp;
    double totalInterest;
    double totalInterestValue;
    double circulation;
};

class CurrentOpenInterest {
    public:
    uint64_t timestamp;
    double openInterest;
};
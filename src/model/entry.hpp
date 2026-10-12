#pragma once 
#include "utils/decimal/fixedPoint.hpp"
#include <cstdint>

// Price and volume in 1e-8 units, see utils/decimal/fixedPoint.hpp
struct Entry {
    AstraConnect::Utils::Fixed price;
    AstraConnect::Utils::Fixed volume;

    public:
    void assignValues(AstraConnect::Utils::Fixed price_, AstraConnect::Utils::Fixed volume_) {
        price = price_;
        volume = volume_;
    }

    Entry(AstraConnect::Utils::Fixed price_, AstraConnect::Utils::Fixed volume_) : price(price_), volume(volume_) {}
    Entry() : price(0), volume(0){}
    void operator+=(Entry& in) {price += in.price; volume += in.volume;} 
};
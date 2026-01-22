#pragma once 
#include <cstdint>

struct Entry {
    double price;
    double volume;

    public:
    void assignValues(double price_, double volume_) {
        price = price_;
        volume = volume_;
    }

    Entry(double price_, double volume_) : price(price_), volume(volume_) {}
    Entry() : price(0), volume(0){}
    void operator+=(Entry& in) {price += in.price; volume += in.volume;} 
};
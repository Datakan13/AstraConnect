#pragma once 
#include <cinttypes>
struct IndexedBidAsk {
    bool isBid;
    int64_t index;
    bool uninitilized;
    public:
    IndexedBidAsk() {uninitilized = true;}
    IndexedBidAsk(bool isBid_,int64_t index_) : isBid(isBid_), index(index_) {uninitilized = false;}
    void operator=(IndexedBidAsk& in) {isBid = in.isBid; index = in.index; uninitilized = false;}
    operator bool() {return uninitilized;}
};
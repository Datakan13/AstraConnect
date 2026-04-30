#pragma once
#include <array>
#include <AstraLib/AstraLib.hpp>
#include "model/entry.hpp"

class OrderbookArrayHolder {
    std::array<std::array<Entry,8192>,128> bidArrays;
    std::array<std::array<Entry,8192>,128> askArrays;
    AstraLib::Pools::ThreadSafeIndexPool<128>& indexPool;
    AstraLib::Buffers::AtomicRingBuffer<int,128> indexqueue;
    public:
    std::pair<std::array<Entry,8192>*,std::array<Entry,8192>*> getArrays(){
        int index = indexPool.getIndex();
        indexqueue.noMoveEnqueue(index);
        return {&bidArrays[index], &askArrays[index]};
    }

    void returnIndex() {
        indexPool.returnIndex(indexqueue.dequeue());
    }

    OrderbookArrayHolder(AstraLib::Pools::ThreadSafeIndexPool<128>& indexPool_) : indexPool(indexPool_){

    }

};

class OrderbookArrays{
    public:
    std::array<Entry,8192>* bidArray = nullptr;
    std::array<Entry,8192>* askArray = nullptr;
    int64_t bidCount;
    int64_t askCount;
    int64_t u;
    int64_t U;
    int64_t pu;
    OrderbookArrays(std::array<Entry,8192>* bidArray_,std::array<Entry,8192>* askArray_,int64_t bidCount_,int64_t askCount_,int64_t u_,int64_t U_, int64_t pu_) :
     bidArray(bidArray_),askArray(askArray_),bidCount(bidCount_),askCount(askCount_),u(u_), U(U_), pu(pu_) {
        //std::cout << "[OrderbookArrays ctor] this=" << this << std::endl;
        //std::cout << bidArray << std::endl;
    }
    OrderbookArrays() {

    }
};
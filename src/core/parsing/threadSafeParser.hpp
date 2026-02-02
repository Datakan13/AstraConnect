#pragma once 
#include <simdjson/simdjson.h>
#include <AstraLib/AstraLib.hpp>
class ThreadSafeParser {
    simdjson::ondemand::parser parser;
    AstraLib::Atomic::Spinlock lock;
    public:
    simdjson::ondemand::parser& parserAccess() {
        lock.lock();
        return parser;
    }

    void returnParser() {
        lock.unlock();
    }
};

class ThreadSafeParserRAII {
    public:
    simdjson::ondemand::parser& parser;
    ThreadSafeParser& TSP;
    ThreadSafeParserRAII(ThreadSafeParser& TSP_) :TSP(TSP_), parser(TSP_.parserAccess()) {}

    ~ThreadSafeParserRAII() {
        TSP.returnParser();
    }

};


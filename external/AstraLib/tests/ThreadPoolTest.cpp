#include <AstraLib/Threading/ThreadPool.hpp>
#include <AstraLib/Debug/debugFunc.hpp>
#include <atomic>

void atomicIncremented(std::atomic<int>& e) {
    int r = e.fetch_add(1,std::memory_order_acq_rel);
    AstraLib::Debug::debugMessage("Count: " , r);
}

int main() {
    AstraLib::Threading::ThreadPool pool;
    std::atomic<int> count{0};
    for(int i=0; i<1000; i++) {
        pool.assignTask( [&count] {
            atomicIncremented(count);
        });
    }
    while(count.load(std::memory_order_acq_rel) <1000) {
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
}
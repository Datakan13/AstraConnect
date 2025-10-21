#include <AstraLib/Buffers/atomicRingBuffer.hpp>
#include <AstraLib/Debug/debugFunc.hpp>
int main() {
    AstraLib::Buffers::AtomicRingBuffer<int,1024> testQueue;

    for( int i=0;i<1000;i++) {
        testQueue.enqueue(std::move(i));

    }

    for(int i = 0; i<1000; i++){
        int a = testQueue.dequeue();
        AstraLib::Debug::debugMessage("Dequeued: ", a);
    }

}
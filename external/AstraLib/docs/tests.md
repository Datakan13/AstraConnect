# Testing 
* Library tests are included in the tests folder
* You will see test codes such as P, S, A
    - P stands for performance in these tests you can expect to see cycle + microsecond + nanosecond latencies.
    - S stands for stress in these tests you can expect to see cycle + microsecond + nanosecond latencies but rather under extreme conditions.
    - A stands for accuracy in these tests you can see the expected result and the actual result i.e. For the TimerATest you see the expected 6000 cycle value and the actual value in cycles and how much jitter it results in
# How to run tests
* There are 4 main Cmake flags 
    - ENABLE_ALL_TESTS which enables all tests 
    - ENABLE_ALL_PTESTS which enables all performance tests 
    - ENABLE_ALL_ATESTS which enables all accuracy tests 
    - ENABLE_ALL_STESTS which enables all stress tests 
* There are other several Cmake flags for each of the tests. You can find these in the top of the files you want to run for tests 

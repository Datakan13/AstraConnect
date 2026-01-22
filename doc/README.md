AstraConnect 

AstraConnect is a low latency exchange SDK for binance 
Including modules FuturesAPI SpotAPI WebsocketStreams UserDataStreams and Orderbook

all of the modules are written with async processing in mind and utilizes boost/asio for managing async processing 
the current version is optimized for multithreading with numa optimizations in mind though not currently implemented 
the project utilizes AstraLib simdjson and boost(system/asio) 
AstraLib is my own library that includes backbone pieces like AtomicRingBuffer which is an indepentent derivation of Vyukov's ring buffer design. the library also includes utilities like padded atomics a futex utility system threadpool and a logger.
testing is done via tests in tests folder currently there is only one test file which runs all tests back to back however this system will evolve into unit and integration tests as the project evolves
currently orderbook design is done and about to be documented though the implementation is incomplete 

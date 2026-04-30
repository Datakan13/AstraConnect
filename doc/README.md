# AstaConnect
    AstraConnect is a low latency exchange SDK for binance.

## Features

- Asynchronous Binance API client
- Futures API support
- Spot API support
- WebSocket market data streams
- User data stream support
- Low-latency orderbook implementation
- Designed for NUMA-aware multithreaded systems

## Dependencies 

- **Boost (systems, Asio)** – asynchronous networking
- **simdjson** - fast json parsing
- **AstraLib** - internal utility/infrastructure library

### AstraLib

AstraLib is a companion library providing core infrastructure components:

- AtomicRingBuffer (derived from Vyukov's ring buffer design)
- Padded atomics
- Futex utilities
- Thread pool
- Logger


# AstraConnect

AstraConnect came out of wanting a Binance client that didn't sit between the strategy and
the exchange. General-purpose exchange SDKs abstract across venues, which means a
lowest-common-denominator model, a DOM parse of every message, and a wire format you can
only see through someone else's types. Each of those is a reasonable default and each one
costs latency and control. AstraConnect takes the opposite bet: one venue, the wire format
kept in plain sight, and parsing straight into the structs the caller actually wanted.

It's a C++20 header-only SDK for Binance USDⓈ-M futures: REST market data, WebSocket market
and user data streams, WebSocket API order entry, and a locally maintained order book. It is
built on [AstraLib](https://github.com/Datakan13/AstraLib) for its concurrency primitives.

## Status

Pre-release, and not yet suitable for trading real money.

**Works** — REST market data (open interest and its history, exchange info, klines, depth
snapshot), trade and mark price streams, user data streams, and WebSocket API order entry
(place, modify, cancel, query, account balance). The order book
applies a snapshot, tracks the diff stream, and re-syncs after a dropped update; on an
unrecoverable failure it stops and reports the reason through `error()` rather than taking
the process down with it. Each side is kept sorted as bits over price ticks, so the best
levels are read directly (`getTopBids`, `getBestAsk`, `getEntryWithPrice`).

**Known broken** — the user data stream has no listen key keepalive, so Binance expires the
key after 60 minutes. The spot surface is one endpoint deep.

CI builds with GCC and Clang and runs the offline tests on every push, plus ASan/UBSan and
TSan builds and a nightly stress soak; see [Testing](#testing). The live tests are run by hand,
and one of them places a real order.

## Requirements

- Linux on x86-64 or aarch64 — inherits AstraLib's futex syscalls; spin waits use `AstraLib::Atomic::cpuRelax()`
- A C++20-capable compiler (developed and tested with GCC 11)
- CMake 3.22+
- Boost 1.74+ (`system`, `thread`) and OpenSSL 3.x

simdjson and AstraLib are vendored under `include/`; Boost and OpenSSL are not. The vendored
AstraLib is a modified version (see [NOTICE](NOTICE)).

## Build

```
cmake -S . -B build
cmake --build build
```

AstraConnect is header-only apart from simdjson. Add `src/` and `include/` to your include
path, compile `include/simdjson/simdjson.cpp` into your target, link Boost and OpenSSL, and
`#include <api/AstraConnect.hpp>`.

Credentials are passed to `APIManager`; the library never reads them from the
environment, so load them from wherever your application keeps secrets:

```cpp
APIManager market;                                  // market data only
APIManager trading(Credentials{apiKey, privateKey}); // order entry and user data
```

REST market data and market streams need no keys. `Futures::OrderStream` needs both;
`User::FuturesStream` needs the API key. Constructing either without them throws
`std::invalid_argument` before any connection is opened.

## Components

Everything hangs off a single `APIManager`, which owns the io_context, the TLS context and
the shared parser.

- **REST** — `Futures::API` (open interest and history, exchange info, klines, depth snapshot,
  and `fetchRaw` for unwrapped endpoints), `Spot::API` (klines)
- **Market streams** — `Futures::Streams::TradeEventStream`, `MarkPriceStream`
- **User data** — `User::FuturesStream`
- **Order entry** — `Futures::OrderStream`, over the WebSocket API with HMAC-SHA256 signing:
  place, modify, cancel and query orders, and fetch the account balance
- **Order book** — `Orderbook`, maintained locally from a snapshot plus the diff stream
- **Raw stream** — `AstraConnect::RawWebsocket`, a bare WebSocket connection with no parsing
  and no reconnect, for streams the SDK doesn't wrap yet

`AggregatedTradeEventStream` is declared but empty, and `User::SpotStream` only fetches a
listen key.

## Design Philosophy

Each of these is a deliberate tradeoff rather than an inherited default:

- **Errors as return values, not exceptions** — the parsers and reconnect loops run on
  background stream threads, where an escaping exception does not propagate to the caller,
  it calls `std::terminate`. Transport results come back as `StreamData` and API results as
  `FetchError`, so a failure is something the caller can ignore but not something that can
  take the process down. Connection failures carry the reason up from the layer that saw it.
- **simdjson on-demand instead of a DOM** — exchange messages are read once and thrown away,
  so materialising a document tree is work nobody asked for. On-demand walks the bytes once
  and writes directly into the destination struct. The cost is that the cursor is forward
  only, which is why the wire formats are documented rather than inferred.
- **Fixed-capacity ring buffers between threads** — the socket thread hands off
  without taking a lock, and the buffers are sized up front so the steady state does not
  allocate. This is a real tradeoff and not a free win: the buffer blocks when full, so a
  consumer that stalls will eventually stall the reader behind it.
- **One venue, no abstraction layer** — there is no generic `Exchange` interface, because
  the useful parts of Binance's API are the parts a generic interface would have to discard.
  Binance's field names appear as Binance's field names.
- **Fixed-point prices** — prices and quantities are parsed straight from Binance's decimal
  strings into `int64` at 1e-8 scale, so values are exact and the order book compares
  integers rather than doubles.
- **Header-only** — no build artefact to version against, at the cost of compile time.

These are design intentions, not benchmark results. Nothing here has been measured against
an alternative; take them as the reasoning behind the choices rather than performance claims.

## Wire formats

Every Binance payload the SDK parses is documented in [doc/payloads/](doc/payloads/), one
file per payload, each recording its source URL, the date it was last checked, and whether
it has actually been verified against a live response. The parsers carry a one-line pointer
to their doc:

```cpp
// Wire format: doc/payloads/rest/futures-open-interest.md
```

This exists because the wire format is the part most likely to be quietly wrong. Binance
changes payloads without notice, and fields documented as "only pushed with ..." really are
absent otherwise. Every parsing bug found so far was in a payload nobody had re-checked.

## Testing

Each test is its own executable. The offline ones are registered with CTest and labelled
`unit` (fast, deterministic) or `stress` (multi-threaded or long randomised runs):

```
cmake --build build
ctest --test-dir build                 # all offline tests
ctest --test-dir build -L unit         # just the fast ones
```

Offline, `unit`:

- `topBookTest` — checks the sorted book against `std::map` under random updates
- `fixedPointTest` — decimal string ↔ fixed-point conversion, including on real simdjson fields
- `payloadParseTest` — every payload in [doc/payloads](doc/payloads/README.md) through its parser,
  the error helpers and enum mapping, then ~56k truncated and corrupted copies of those payloads,
  each run in a child process so a crash is pinned to the input that caused it
- `requestSigningTest` — request building and HMAC signing, including Binance's published
  signature example
- `backoffTest` — the reconnect schedule and its interruptible wait

Offline, `stress`:

- `ringBufferTryTest` — the non-blocking paths of the vendored AstraLib ring buffer
- `depthDecoderTest` — the depth diff decoder and its slot holder, including a producer/consumer
  run that checks no slot is reused while its event is still queued
- `sortedBookStressTest` — the sorted book at its level and block limits, with price jumps of
  millions of ticks and concurrent readers
- `concurrencyStressTest` — Spinlock, the shared parser, the index pool, `OrderTracker` and
  backoff shutdown, from many threads at once

`ASTRA_STRESS_SCALE=N` divides the stress iteration counts by N. Sanitizer builds:

```
cmake -S . -B build-asan -DCMAKE_CXX_COMPILER=clang++ -DCMAKE_BUILD_TYPE=RelWithDebInfo -DASTRA_SANITIZE=address,undefined
cmake -S . -B build-tsan -DCMAKE_CXX_COMPILER=clang++ -DCMAKE_BUILD_TYPE=RelWithDebInfo -DASTRA_SANITIZE=thread
cmake --build build-tsan --target offline_tests
ASTRA_STRESS_SCALE=10 ctest --test-dir build-tsan
```

Clang is recommended for these: GCC 11 takes several minutes per parser-heavy file with
sanitizers on. If the sanitizer runtime fails with "unexpected memory mapping", lower the ASLR
entropy (`sudo sysctl vm.mmap_rnd_bits=28`) or run under `setarch -R`.

Confirmed library bugs that are not fixed yet are written as `KNOWN_BUG` checks: they print
`[KNOWN BUG]` and pass while the bug is present, and fail once it is fixed, so the check is turned
into an ordinary one.

Live (needs network access to Binance, never run by CTest or CI):

- `rawWebsocketTest` — `RawWebsocket` and `fetchRaw`; no API key needed. Configure with
  `-DASTRA_LIVE_TESTS=ON` to register it with CTest under the `live` label
- `apiTest` — **places a real order**, so read it before running it

## Notes

Parts of this codebase were written with AI assistance, including documentation, wire format
verification, and fixes across the transport, parsing and memory-ownership layers. The
architecture and the design decisions are the creator's.

## License

Licensed under the Apache License, Version 2.0. See [LICENSE](LICENSE).

simdjson and AstraLib are vendored under `include/` and are themselves Apache-2.0; the
vendored AstraLib is a modified version of upstream (AI-assisted changes, listed in NOTICE);
Boost and OpenSSL are linked rather than redistributed. See [NOTICE](NOTICE) for the
full attribution list.

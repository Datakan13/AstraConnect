# Repository Structure

## Top-level

| Path          | Purpose                                          |
|---------------|--------------------------------------------------|
| `src/`        | All library source (header-only)                 |
| `tests/`      | Unit and integration tests                       |
| `doc/`        | Additional documentation                         |

## src/

| Path                | Purpose                                                          |
|---------------------|------------------------------------------------------------------|
| `api/`              | Public-facing API layer — entry point for all SDK users          |
| `api/AstraConnect.hpp` | Top-level include — include this to use the SDK               |
| `api/futures/`      | Futures-specific streams, orders, and REST bindings              |
| `api/futures/order/`| Order lifecycle: placement, tracking, response and sent events   |
| `api/futures/streams/` | Futures market data streams (trades, mark price, aggregated) |
| `api/spot/`         | Spot market API bindings                                         |
| `api/orderbook/`    | Lock-free orderbook with top-of-book and snapshot support        |
| `api/user/`         | User data streams (account updates, order fills)                 |
| `api/user/streams/` | Futures and spot user stream implementations                     |
| `api/common/`       | Shared enums, error types, factories, and user data models       |
| `api/common/enums/` | Market and order type enumerations and conversion utilities      |
| `api/common/error/` | API and fetch error types                                        |
| `api/common/factory/` | Factories for constructing stream event objects               |
| `api/common/userData/` | User data stream events, parsers, and cast utilities         |
| `core/`             | Low-level infrastructure: stream management, parsing, types      |
| `core/parsing/`     | Thread-safe JSON parsing utilities                               |
| `core/protocol/`    | Request parameter construction for REST calls                    |
| `core/streams/`     | Stream and WebSocket connection holders                          |
| `core/types/`       | Shared primitive types (status codes, etc.)                      |
| `model/`            | Plain data structs (entries, candles, mark price, trade events)  |
| `net/`              | Boost.Beast/Asio glue and WebSocket networking primitives        |
| `manager/`          | High-level API manager coordinating streams and lifecycle        |
| `utils/crypto/`     | HMAC-SHA256 request signing                                      |

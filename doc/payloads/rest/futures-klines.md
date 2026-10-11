# Kline / Candlestick Data (Futures)

| | |
|---|---|
| **Endpoint** | `GET /fapi/v1/klines` (fapi.binance.com) |
| **Source** | <https://developers.binance.com/docs/products/derivatives-trading-usds-futures/market-data> |
| **Accessed** | 2026-10-11 |
| **Verification** | verified |
| **Parsed by** | `src/api/futures/api.hpp` |

## Payload

```jsonc
[
    [
        1499040000000,      // Kline open time [index: 0]
        "0.01634790",       // Open price [index: 1]
        "0.80000000",       // High price [index: 2]
        "0.01575800",       // Low price [index: 3]
        "0.01577100",       // Close price [index: 4]
        "148976.11427815",  // Volume [index: 5]
        1499644799999,      // Kline Close time [index: 6]
        "2434.19055334",    // Quote asset volume [index: 7]
        308,                // Number of trades [index: 8]
        "1756.87402397",    // Taker buy base asset volume [index: 9]
        "28.46694368",      // Taker buy quote asset volume [index: 10]
        "0"                 // Unused field, ignore. [index: 11]
    ]
]
```

## Notes

Array of arrays. Index 0, 6 and 8 are numbers; all other elements are strings.
The parser reads indices 0-7 and ignores 8-11.
Same layout as the spot endpoint ([spot-klines.md](spot-klines.md)).

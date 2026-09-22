# Open Interest

| | |
|---|---|
| **Endpoint** | `GET /fapi/v1/openInterest` |
| **Source** | <https://developers.binance.com/docs/products/derivatives-trading-usds-futures/market-data> |
| **Accessed** | 2026-09-23 |
| **Verification** | verified |
| **Parsed by** | `src/api/futures/api.hpp` |

## Payload

```jsonc
{
    "openInterest": "10659.509",
    "symbol": "BTCUSDT",
    "time": 1589437530011   // Transaction time
}
```

## Notes

`openInterest` is a string, `time` is a number.

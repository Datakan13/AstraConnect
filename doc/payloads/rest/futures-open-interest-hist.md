# Open Interest Statistics

| | |
|---|---|
| **Endpoint** | `GET /futures/data/openInterestHist` |
| **Source** | <https://developers.binance.com/docs/products/derivatives-trading-usds-futures/market-data> |
| **Accessed** | 2026-09-23 |
| **Verification** | verified |
| **Parsed by** | `src/api/futures/api.hpp` |

## Payload

```jsonc
[
    {
        "symbol":"BTCUSDT",
        "sumOpenInterest":"20403.63700000",  // total open interest
        "sumOpenInterestValue": "150570784.07809979",   // total open interest value
        "CMCCirculatingSupply": "165880.538", // circulating supply provided by CMC
        "timestamp":"1583127900000" // WARNING: document claims this is a string however the actual payload is : "timestamp":1761051300000
    },
    {
        "symbol":"BTCUSDT",
        "sumOpenInterest":"20401.36700000",
        "sumOpenInterestValue":"149940752.14464448",
        "CMCCirculatingSupply": "165900.14853",
        "timestamp":"1583128200000"   // WARNING: document claims this is a string however the actual payload is : "timestamp":1761051300000
    },
]
```

## Notes

The docs describe `timestamp` as a string. **It is a JSON number.** Confirmed against
the live endpoint; the parser correctly uses `get_int64()`.

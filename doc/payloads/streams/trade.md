# Trade Stream

| | |
|---|---|
| **Endpoint** | `<symbol>@trade` (fstream.binance.com) |
| **Source** | <https://developers.binance.com/docs/products/derivatives-trading-usds-futures/websocket-market-streams> |
| **Accessed** | 2026-09-23 |
| **Verification** | unverified |
| **Parsed by** | `src/api/common/factory/tradeEventFactory.hpp` |

## Payload

```jsonc
Update rate: Event based
{
    "e": "trade",       // Event type
    "E": 1672515782136, // Event time
    "s": "BNBBTC",      // Symbol
    "t": 12345,         // Trade ID
    "p": "0.001",       // Price
    "q": "100",         // Quantity
    "T": 1672515782136, // Trade time
    "m": true,          // Is the buyer the market maker?
    "M": true           // Ignore
}
```

## Notes

Event based, no fixed update rate.
`t` (trade id), `s` (symbol) and `M` are present on the wire but not parsed.

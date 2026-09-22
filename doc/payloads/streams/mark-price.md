# Mark Price Stream

| | |
|---|---|
| **Endpoint** | `<symbol>@markPrice@1s` (fstream.binance.com) |
| **Source** | <https://developers.binance.com/docs/products/derivatives-trading-usds-futures/websocket-market-streams> |
| **Accessed** | 2026-09-23 |
| **Verification** | unverified |
| **Parsed by** | `src/api/common/factory/markPriceFactory.hpp` |

## Payload

```jsonc
  Update rate: 1000ms
{
  "e": "markPriceUpdate",  	// Event type
  "E": 1562305380000,      	// Event time
  "s": "BTCUSDT",          	// Symbol
  "p": "11794.15000000",   	// Mark price
  "i": "11784.62659091",		// Index price
  "P": "11784.25641265",		// Estimated Settle Price, only useful in the last hour before the settlement starts
  "r": "0.00038167",       	// Funding rate
  "T": 1562306400000       	// Next funding time
}
```

## Notes

Update rate 1000ms (3000ms without the `@1s` suffix).
`P` (estimated settle price) is present on the wire but not parsed.

# TRADE_LITE

| | |
|---|---|
| **Endpoint** | User data stream |
| **Source** | <https://developers.binance.com/docs/products/derivatives-trading-usds-futures/user-data-streams> |
| **Accessed** | 2026-09-23 |
| **Verification** | verified |
| **Parsed by** | `src/api/common/userData/userDataStreamEvents.hpp` |

## Payload

```jsonc
{
  "e":"TRADE_LITE",             // Event Type
  "E":1721895408092,            // Event Time
  "T":1721895408214,            // Transaction Time
  "s":"BTCUSDT",                // Symbol
  "q":"0.001",                  // Original Quantity
  "p":"0",                      // Original Price
  "m":false,                    // Is this trade the maker side?
  "c":"z8hcUoOsqEdKMeKPSABslD", // Client Order Id
      // special client order id:
      // starts with "autoclose-": liquidation order
      // "adl_autoclose": ADL auto close order
      // "settlement_autoclose-": settlement order for delisting or delivery
  "S":"BUY",                   // Side
  "L":"64089.20",              // Last Filled Price
  "l":"0.040",                 // Order Last Filled Quantity
  "t":109100866,               // Trade Id
  "i":8886774,                // Order Id
}
```

## Notes

Lower latency than ORDER_TRADE_UPDATE but only carries TRADE execution type
and fewer fields.

Special client order ids: `autoclose-` prefix means liquidation, `adl_autoclose`
means ADL auto close, `settlement_autoclose-` means delisting or delivery settlement.

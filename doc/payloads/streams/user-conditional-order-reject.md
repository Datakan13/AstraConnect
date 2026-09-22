# CONDITIONAL_ORDER_TRIGGER_REJECT

| | |
|---|---|
| **Endpoint** | User data stream |
| **Source** | <https://developers.binance.com/docs/products/derivatives-trading-usds-futures/user-data-streams> |
| **Accessed** | 2026-09-23 |
| **Verification** | unverified |
| **Parsed by** | `src/api/common/userData/userDataStreamEvents.hpp` |

## Payload

```jsonc
{
    "e":"CONDITIONAL_ORDER_TRIGGER_REJECT",      // Event Type
    "E":1685517224945,      // Event Time
    "T":1685517224955,      // me message send Time
    "or":{
      "s":"ETHUSDT",      // Symbol
      "i":155618472834,      // orderId
      "r":"Due to the order could not be filled immediately, the FOK order has been rejected. The order will not be recorded in the order history",      // reject reason
     }
}
```

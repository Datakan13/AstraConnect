# MARGIN_CALL

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
    "e":"MARGIN_CALL",    	// Event Type
    "E":1587727187525,		// Event Time
    "cw":"3.16812045",		// Cross Wallet Balance. Only pushed with crossed position margin call
    "p":[					// Position(s) of Margin Call
      {
        "s":"ETHUSDT",		// Symbol
        "ps":"LONG",		// Position Side
        "pa":"1.327",		// Position Amount
        "mt":"CROSSED",		// Margin Type
        "iw":"0",			// Isolated Wallet (if isolated position)
        "mp":"187.17127",	// Mark Price
        "up":"-1.166074",	// Unrealized PnL
        "mm":"1.614445"		// Maintenance Margin Required
      }
    ]
}
```

## Notes

Risk guidance only. Binance explicitly warns it is not suitable to drive strategy:
in a volatile market the position may already be liquidated when this arrives.

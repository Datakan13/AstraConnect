# ACCOUNT_CONFIG_UPDATE

| | |
|---|---|
| **Endpoint** | User data stream |
| **Source** | <https://developers.binance.com/docs/products/derivatives-trading-usds-futures/user-data-streams> |
| **Accessed** | 2026-09-23 |
| **Verification** | unverified |
| **Parsed by** | `src/api/common/userData/userDataStreamEvents.hpp` |

## Payload

```jsonc
first:
{
    "e":"ACCOUNT_CONFIG_UPDATE",       // Event Type
    "E":1611646737479,		           // Event Time
    "T":1611646737476,		           // Transaction Time
    "ac":{
    "s":"BTCUSDT",					   // symbol
    "l":25						       // leverage

    }
}

second:
{
    "e":"ACCOUNT_CONFIG_UPDATE",       // Event Type
    "E":1611646737479,		           // Event Time
    "T":1611646737476,		           // Transaction Time
    "ai":{							   // User's Account Configuration
    "j":true						   // Multi-Assets Mode
    }
}
```

## Notes

Carries **either** `ac` (trade pair leverage change: `s` symbol, `l` leverage)
**or** `ai` (multi-assets mode change: `j` mode) — not both.

# STRATEGY_UPDATE

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
	"e": "STRATEGY_UPDATE", // Event Type
	"T": 1669261797627, // Transaction Time
	"E": 1669261797628, // Event Time
	"su": {
			"si": 176054594, // Strategy ID
			"st": "GRID", // Strategy Type
			"ss": "NEW", // Strategy Status
			"s": "BTCUSDT", // Symbol
			"ut": 1669261797627, // Update Time
			"c": 8007 // opCode
		}
}
```

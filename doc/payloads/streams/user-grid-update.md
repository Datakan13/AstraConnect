# GRID_UPDATE

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
	"e": "GRID_UPDATE", // Event Type
	"T": 1669262908216, // Transaction Time
	"E": 1669262908218, // Event Time
	"gu": {
			"si": 176057039, // Strategy ID
			"st": "GRID", // Strategy Type
			"ss": "WORKING", // Strategy Status
			"s": "BTCUSDT", // Symbol
			"r": "-0.00300716", // Realized PNL
			"up": "16720", // Unmatched Average Price
			"uq": "-0.001", // Unmatched Qty
			"uf": "-0.00300716", // Unmatched Fee
			"mp": "0.0", // Matched PNL
			"ut": 1669262908197 // Update Time
		   }
}
```

## Notes

**Deprecated by Binance.**

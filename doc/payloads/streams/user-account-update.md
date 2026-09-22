# ACCOUNT_UPDATE

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
  "e": "ACCOUNT_UPDATE",				// Event Type
  "E": 1564745798939,            		// Event Time
  "T": 1564745798938 ,           		// Transaction
  "a":                          		// Update Data
    {
      "m":"ORDER",						// Event reason type
      "B":[                     		// Balances
        {
          "a":"USDT",           		// Asset
          "wb":"122624.12345678",    	// Wallet Balance
          "cw":"100.12345678",			// Cross Wallet Balance
          "bc":"50.12345678"			// Balance Change except PnL and Commission
        },
        {
          "a":"BUSD",
          "wb":"1.00000000",
          "cw":"0.00000000",
          "bc":"-49.12345678"
        }
      ],
      "P":[
        {
          "s":"BTCUSDT",          	// Symbol
          "pa":"0",               	// Position Amount
          "ep":"0.00000",            // Entry Price
          "bep":"0",                // breakeven price
		  "cr":"200",             	// (Pre-fee) Accumulated Realized
          "up":"0",						// Unrealized PnL
          "mt":"isolated",				// Margin Type
          "iw":"0.00000000",			// Isolated Wallet (if isolated position)
          "ps":"BOTH"					// Position Side
        }，
        {
        	"s":"BTCUSDT",
        	"pa":"20",
        	"ep":"6563.66500",
        	"bep":"0",                // breakeven price
        	"cr":"0",
        	"up":"2850.21200",
        	"mt":"isolated",
        	"iw":"13200.70726908",
        	"ps":"LONG"
      	 },
        {
        	"s":"BTCUSDT",
        	"pa":"-10",
        	"ep":"6563.86000",
        	"bep":"6563.6",          // breakeven price
        	"cr":"-45.04000000",
        	"up":"-1423.15600",
        	"mt":"isolated",
        	"iw":"6570.42511771",
        	"ps":"SHORT"
        }
      ]
    }
}
```

## Notes

**`P` is optional.** When a FUNDING FEE lands on a crossed position Binance pushes
`B` only, with no `P` array at all. Parsing it unconditionally crashed the stream thread
every funding settlement.

Both array keys are **uppercase**: `B` for balances, `P` for positions.

`m` (reason type) has 17 documented values: DEPOSIT, WITHDRAW, ORDER, FUNDING_FEE,
WITHDRAW_REJECT, ADJUSTMENT, INSURANCE_CLEAR, ADMIN_DEPOSIT, ADMIN_WITHDRAW,
MARGIN_TRANSFER, MARGIN_TYPE_CHANGE, ASSET_TRANSFER, OPTIONS_PREMIUM_FEE,
OPTIONS_SETTLE_PROFIT, AUTO_EXCHANGE, COIN_SWAP_DEPOSIT, COIN_SWAP_WITHDRAW.

`bc` is the balance change excluding PnL and commission.
Only symbols whose positions changed are pushed.

# ORDER_TRADE_UPDATE

| | |
|---|---|
| **Endpoint** | User data stream |
| **Source** | <https://developers.binance.com/docs/products/derivatives-trading-usds-futures/user-data-streams> |
| **Accessed** | 2026-09-23 |
| **Verification** | partial |
| **Parsed by** | `src/api/common/userData/userDataStreamEvents.hpp` |

## Payload

```jsonc
{
  "e":"ORDER_TRADE_UPDATE",		   // Event Type
  "E":1568879465651,			       // Event Time
  "T":1568879465650,			       // Transaction Time
  "o":{
    "s":"BTCUSDT",			         // Symbol
    "c":"TEST",				           // Client Order Id
      // special client order id:
      // starts with "autoclose-": liquidation order
      // "adl_autoclose": ADL auto close order
      // "settlement_autoclose-": settlement order for delisting or delivery
    "S":"SELL",					         // Side
    "o":"TRAILING_STOP_MARKET",	 // Order Type
    "f":"GTC",					         // Time in Force
    "q":"0.001",				         // Original Quantity
    "p":"0",					           // Original Price
    "ap":"0",					           // Average Price
    "sp":"7103.04",				       // Stop Price. Please ignore with TRAILING_STOP_MARKET order
    "x":"NEW",					         // Execution Type
    "X":"NEW",					         // Order Status
    "i":8886774,				         // Order Id
    "l":"0",					           // Order Last Filled Quantity
    "z":"0",					           // Order Filled Accumulated Quantity
    "L":"0",					           // Last Filled Price
    "N":"USDT",            	     // Commission Asset
    "n":"0",               	     // Commission
    "T":1568879465650,			     // Order Trade Time
    "t":0,			        	       // Trade Id
    "b":"0",			    	         // Bids Notional
    "a":"9.91",					         // Ask Notional
    "m":false,					         // Is this trade the maker side?
    "R":false,					         // Is this reduce only
    "wt":"CONTRACT_PRICE", 		   // Stop Price Working Type
    "ot":"TRAILING_STOP_MARKET", // Original Order Type
    "ps":"LONG",					       // Position Side
    "cp":false,						       // If Close-All, pushed with conditional order
    "AP":"7476.89",				       // Activation Price, only puhed with TRAILING_STOP_MARKET order
    "cr":"5.0",					         // Callback Rate, only puhed with TRAILING_STOP_MARKET order
    "pP": false,                 // If price protection is turned on
    "si": 0,                     // ignore
    "ss": 0,                     // ignore
    "rp":"0",	   					       // Realized Profit of the trade
    "V":"EXPIRE_TAKER",          // STP mode
    "pm":"OPPONENT",             // Price match mode
    "gtd":0,                     // TIF GTD order auto cancel time
    "er":"0"                     // Expire Reason
  }
}
```

## Notes

`AP` (activation price) and `cr` (callback rate) are **only pushed for
TRAILING_STOP_MARKET** orders. The parser guards on order type before reading them.

`er` (expire reason) values: 0 none, 1 self-trade prevention, 2 IOC partial fill,
3 IOC + STP, 4 knocked out by higher priority RO order, 5 account liquidated,
6 GTE condition unsatisfied, 7 symbol delisted, 8 initial order expired after stop
trigger, 9 market order partial fill.

Liquidation shows `c` as `autoclose-XXX` with `X` = NEW; ADL shows `c` as
`adl_autoclose` with `X` = NEW.

`si` and `ss` are documented as ignore and are not parsed.

Execution types: NEW, CANCELED, CALCULATED, EXPIRED, TRADE, AMENDMENT.
Order statuses: NEW, PARTIALLY_FILLED, FILLED, CANCELED, EXPIRED, EXPIRED_IN_MATCH.

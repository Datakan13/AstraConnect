# Binance Wire Formats

Every Binance payload this SDK parses, one file each. These used to live as block
comments inside the headers; they were moved here so the parsers stay readable and so
the wire format can be re-checked against Binance without reading C++.

Each source file that parses a payload carries a one-line pointer back to its doc:

```cpp
// Wire format: doc/payloads/rest/futures-open-interest.md
```

## Layout

| Directory | Contents |
|---|---|
| `rest/` | HTTP endpoints on `fapi.binance.com` and `api.binance.com` |
| `streams/` | WebSocket market and user data streams on `fstream.binance.com` |
| `ws-api/` | WebSocket API request/response on `ws-fapi.binance.com` |

## Verification status

Every doc carries an **Accessed** date and a **Verification** field. The three values:

| Value | Meaning |
|---|---|
| `verified` | Field names and JSON types checked against a live response or official docs, and confirmed to match the parser |
| `partial` | Semantics confirmed from official docs, but the full field list was not checked |
| `unverified` | Reproduced from the original source comment; never independently checked |

`unverified` does **not** mean wrong. It means nobody has confirmed it recently, and
every bug found so far turned up in payloads that were in exactly that state.

### REST

| Payload | Verification | Method |
|---|---|---|
| [Open Interest](rest/futures-open-interest.md) | verified | live `GET /fapi/v1/openInterest` |
| [Open Interest Statistics](rest/futures-open-interest-hist.md) | verified | live `GET /futures/data/openInterestHist` |
| [Exchange Information](rest/futures-exchange-info.md) | verified | live `GET /fapi/v1/exchangeInfo` |
| [Kline / Candlestick](rest/spot-klines.md) | verified | live `GET /api/v3/klines` |
| [Kline / Candlestick (Futures)](rest/futures-klines.md) | verified | live `GET /fapi/v1/klines` |
| [Listen Key](rest/futures-listen-key.md) | verified | official docs |
| [Listen Key (Spot)](rest/spot-listen-key.md) | unverified | — |

The order book snapshot (`GET /fapi/v1/depth`) had no source comment to migrate. It was
validated against the live endpoint on 2026-09-23: `lastUpdateId` is a number, `bids` and
`asks` are arrays of string pairs. The response also carries `E` and `T`, which the parser
ignores.

### Streams

| Payload | Verification |
|---|---|
| [ACCOUNT_UPDATE](streams/user-account-update.md) | verified |
| [ORDER_TRADE_UPDATE](streams/user-order-update.md) | partial |
| [TRADE_LITE](streams/user-trade-lite.md) | verified |
| [MARGIN_CALL](streams/user-margin-call.md) | unverified |
| [ACCOUNT_CONFIG_UPDATE](streams/user-account-config-update.md) | unverified |
| [STRATEGY_UPDATE](streams/user-strategy-update.md) | unverified |
| [GRID_UPDATE](streams/user-grid-update.md) | unverified (deprecated by Binance) |
| [CONDITIONAL_ORDER_TRIGGER_REJECT](streams/user-conditional-order-reject.md) | unverified |
| [Trade](streams/trade.md) | unverified |
| [Mark Price](streams/mark-price.md) | unverified |

Two further user data events exist that this SDK does not model. Both are recognised so
they cannot crash the stream thread, but neither carries a payload object:

- `ALGO_UPDATE` — algo order lifecycle
- `listenKeyExpired` — emitted when the listen key expires, 60 minutes after creation

### WebSocket API

| Payload | Verification |
|---|---|
| [New Order](ws-api/order-new.md) | unverified |
| [Modify Order](ws-api/order-modify.md) | unverified |
| [Cancel Order](ws-api/order-cancel.md) | unverified |

`order.status` (`QueryOrderResponse`) and `v2/account.balance` (`AccountInfoResponse`)
had no source comments to migrate. `v2/account.balance` was checked against official docs
on 2026-09-23 and matches the parser exactly: envelope `{id, status, result, rateLimits}`,
nine balance fields, five rate limit fields.

The envelope is shared by every WebSocket API method:

```jsonc
{
  "id": "605a6d20-6588-4cb9-afa0-b0ab087507ba",
  "status": 200,
  "result": { },          // singular; an object or an array depending on method
  "rateLimits": [ { "rateLimitType": "REQUEST_WEIGHT", "interval": "MINUTE",
                    "intervalNum": 1, "limit": 2400, "count": 20 } ]
}
```

## Re-checking a payload

REST responses can be read straight off the live API, which beats the docs because it is
the actual wire format:

```sh
curl -s 'https://fapi.binance.com/fapi/v1/depth?symbol=BTCUSDT&limit=5'
curl -s 'https://fapi.binance.com/fapi/v1/exchangeInfo' | jq '.symbols[0].filters'
```

Stream and WebSocket API payloads cannot be sampled this way. Those come from the docs at
<https://developers.binance.com/docs>, which is a client-rendered app — plain HTTP fetches
return an empty shell. The full text is published as `llms.txt` / `llms-full.txt`, linked
from the site footer; that dump carries the prose and semantics for every event, though
the field tables themselves are embedded schema components and are not included in it.

When you re-check something, update both the **Accessed** date and the **Verification**
field in that file, and the table above.

## Conventions

- Prices, quantities and balances are **strings**, not numbers. Timestamps are numbers.
  `openInterestHist.timestamp` is documented as a string but is a number on the wire.
- JSON keys are case-sensitive and Binance mixes cases meaningfully: in `ACCOUNT_UPDATE`,
  `B` is balances and `P` is positions, while `b` and `a` in the depth diff stream are
  bids and asks.
- Fields documented as "only pushed with …" are genuinely absent otherwise, and reading
  them unconditionally throws.
- Binance adds event types and fields without notice. An unrecognised event type resolves
  to `EventType::UNKNOWN` rather than throwing, because these parsers run on a stream
  thread where an escaping exception terminates the process.

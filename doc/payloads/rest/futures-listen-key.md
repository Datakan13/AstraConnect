# Listen Key

| | |
|---|---|
| **Endpoint** | `POST` / `PUT` / `DELETE /fapi/v1/listenKey` |
| **Source** | <https://developers.binance.com/docs/products/derivatives-trading-usds-futures/user-data-streams> |
| **Accessed** | 2026-09-23 |
| **Verification** | verified |
| **Parsed by** | `src/api/user/streams/userFuturesStream.hpp` |

## Payload

```jsonc
{"listenKey":"gwKzdioWPho490C2wogHUt9EF8rfkSxO5EVILWXV7gD0k94n7wP97EEfzA3DURPH"}
```

## Notes

Three operations: `POST` start, `PUT` keepalive, `DELETE` close.

**Keys expire after 60 minutes.** Binance then emits a `listenKeyExpired` user data
event. The `PUT` keepalive is not currently implemented, so the stream dies after
an hour.

# Listen Key (Spot)

| | |
|---|---|
| **Endpoint** | `POST /api/v3/userDataStream` |
| **Source** | <https://developers.binance.com/docs/products/spot/user-data-stream> |
| **Accessed** | 2026-09-23 |
| **Verification** | unverified |
| **Parsed by** | `src/api/user/streams/userSpotStream.hpp` |

## Payload

```jsonc
{"listenKey":"CFhC7fBYdyq2PoFnsk9U5lvpCEb5fEyQkjCR0DdvPCCzCZMEt9DCGrKzb"}
```

## Notes

The spot user stream class is incomplete and currently targets the **futures**
host and path. Left as-is.

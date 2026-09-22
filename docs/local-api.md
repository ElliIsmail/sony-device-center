# Local API

The desktop app (`sony-device-center`) can serve a small HTTP API so other
programs on the same computer can read the headphone state and switch modes
through it. The app owns the only Bluetooth control channel to the headset, so
this is how tools such as the [Stream Deck plugin](../integrations/streamdeck)
reach it while the app is open (or sitting in the tray).

- Address: `http://127.0.0.1:47821`, loopback only, never exposed on the network.
- Toggle: **Settings → Local API for Stream Deck** (on by default).
- One request per connection, JSON responses, no request bodies.

## Security

- The server binds to `127.0.0.1` only.
- Requests whose `Host` header is not `127.0.0.1`, `localhost` or `::1` get
  `403`. This blocks DNS-rebinding attacks from web pages.
- Every state-changing request must be a `POST` carrying
  `X-Sony-Device-Center: 1`. A web page cannot add that header to a
  cross-origin request without a CORS preflight, and the server never approves
  one.

## `GET /status`

```json
{
  "ok": true,
  "connected": true,
  "device": "WH-1000XM4",
  "battery": 100,
  "charging": false,
  "noiseControl": "cancelling",
  "ambientLevel": 10,
  "speakToChat": false
}
```

| Field | Meaning |
| --- | --- |
| `battery` | 0–100, or `null` while disconnected or unknown |
| `noiseControl` | `cancelling`, `ambient`, `off` or `unknown` |
| `ambientLevel` | 1–20, the level Ambient Sound uses |
| `speakToChat` | `true`/`false`, or `null` when the model has no Speak-to-Chat |

## Commands

All return `{"ok":true}` on success, or `{"ok":false,"error":"..."}` with a
4xx status.

| Request | Effect |
| --- | --- |
| `POST /noise-control/cancelling` | Noise Cancelling |
| `POST /noise-control/ambient` | Ambient Sound at `ambientLevel` |
| `POST /noise-control/off` | Noise control off |
| `POST /noise-control/next` | Cycle Noise Cancelling → Ambient → Off |
| `POST /speak-to-chat/on`, `/off`, `/toggle` | Speak-to-Chat |

`409` means the headphones are disconnected, the feature is unsupported, or the
app is still applying the previous command (`"busy, retry shortly"`).

```sh
curl http://127.0.0.1:47821/status
curl -X POST -H "X-Sony-Device-Center: 1" http://127.0.0.1:47821/noise-control/next
```

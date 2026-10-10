# Stream Deck plugin

Three Stream Deck keys for Sony headphones, powered by the Sony Device Center
desktop app:

| Key | Shows | Press |
| --- | --- | --- |
| **Battery** | Ring gauge with the percentage: green, amber at 50 % or less, red at 20 % or less, blue while charging | Refresh now |
| **Noise Control** | Current mode (Noise Cancelling, Ambient + level, Off) | Cycle Noise Cancelling → Ambient → Off |
| **Speak-to-Chat** | On / Off | Toggle |
| **HyperX Battery** | Same ring gauge, for a HyperX Cloud III S Wireless | Refresh now |

The plugin never touches Bluetooth. Only one program can hold the headset's
control channel, and that is the desktop app, so the plugin talks to the app's
[local API](../../docs/local-api.md) on `127.0.0.1:47821`. Keep the app running
(it can sit in the tray) with **Settings → Local API for Stream Deck** switched
on. Keys show *App off* when the app is not reachable and *Offline* when the
headphones are disconnected. It polls every 3 seconds, only while a key is
visible.

The **HyperX Battery** key is the exception: it reads the headset's 2.4 GHz
USB dongle (`03F0:02CC`) directly over HID with
[node-hid](https://github.com/node-hid/node-hid), so it works without the
desktop app and alongside NGENUITY. It shows *No dongle* when the dongle is
unplugged and *Offline* when the headset is off. It polls every 10 seconds;
over Bluetooth alone the headset reports nothing to the plugin.

Requires Stream Deck 7.1 or later, which runs the plugin on its bundled
Node.js 24.

## Build and install

```sh
cd integrations/streamdeck
npm install
npm run build           # bundles src/ into com.sonydevicecenter.sdPlugin/bin/plugin.js
npx streamdeck link com.sonydevicecenter.sdPlugin   # once: registers the folder with Stream Deck
```

Then restart Stream Deck. The keys appear under **Sony Device Center** in the
action list.

- `npm run watch` rebuilds on change. With developer mode on
  (`npx streamdeck dev`), `npm run restart` reloads the plugin.
- `npm run validate` checks the manifest and images.
- `npm run pack` produces a `.streamDeckPlugin` file you can double-click to
  install on another machine.

## Layout

```
src/api.ts            local API client
src/hyperx.ts         HyperX Cloud III S dongle reader (HID)
src/poller.ts         one shared poll loop per source, for every visible key
src/render.ts         key images (SVG, same colours as the app)
src/actions/*.ts      the four actions
com.sonydevicecenter.sdPlugin/   manifest, icons, and the bundled bin/
                                 (bin/node_modules holds node-hid's native binary)
```

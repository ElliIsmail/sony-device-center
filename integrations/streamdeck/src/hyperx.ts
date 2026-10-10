// HyperX Cloud III S Wireless, read straight from its 2.4 GHz USB dongle over
// HID. Unlike the Sony headphones there is no exclusive control channel to
// share: NGENUITY and this plugin can both talk to the dongle at once.
//
// Protocol (from HaloBattery's Cloud III S provider, confirmed on a 03F0:02CC
// dongle): a 64-byte output report 0x0C on the 0x01C0 vendor collection,
//   0c 02 03 01 00 <cmd> 00 ...      (byte 3 = 01: "get")
// answered by input report 0x0C carrying <cmd> in byte 5 and the value in
// byte 6, 0xFF when the headset is off. The dongle also sends other input
// reports (0x0D notifications, 0x0F buttons, 0x05 mic), which are skipped.

import { devicesAsync, HIDAsync, type Device } from "node-hid";

const VENDOR_HP = 0x03f0;
const PRODUCT_CLOUD_III_S = 0x02cc;
const CONTROL_USAGE_PAGE = 0x01c0;

const REPORT_ID = 0x0c;
const CMD_BATTERY = 0x06;
const CMD_CHARGING = 0x48;
const NO_VALUE = 0xff;
const TIMEOUT_MS = 1000;

export type HyperXSnapshot =
	| { state: "no-dongle" }
	/** Dongle plugged in, headset off or out of range. */
	| { state: "offline" }
	| { state: "online"; battery: number; charging: boolean };

/** The collection that takes report 0x0C. macOS lists one entry per interface, so fall back to the first. */
function controlInterface(devices: Device[]): Device | undefined {
	return devices.find((d) => d.usagePage === CONTROL_USAGE_PAGE) ?? devices[0];
}

/** Sends a "get" for one setting; resolves to its value, or null when the headset doesn't answer. */
async function query(device: HIDAsync, cmd: number): Promise<number | null> {
	const request = Buffer.alloc(64);
	request.set([REPORT_ID, 0x02, 0x03, 0x01, 0x00, cmd]);
	await device.write(request);

	const deadline = Date.now() + TIMEOUT_MS;
	while (Date.now() < deadline) {
		const reply = await device.read(Math.max(1, deadline - Date.now()));
		if (reply && reply[0] === REPORT_ID && reply[5] === cmd) {
			return reply[6] === NO_VALUE ? null : reply[6];
		}
	}
	return null;
}

export async function readHyperX(): Promise<HyperXSnapshot> {
	try {
		const info = controlInterface(await devicesAsync(VENDOR_HP, PRODUCT_CLOUD_III_S));
		if (!info?.path) return { state: "no-dongle" };

		// Opened per poll so unplugging and replugging the dongle needs no special handling.
		const device = await HIDAsync.open(info.path, { nonExclusive: true });
		try {
			const battery = await query(device, CMD_BATTERY);
			if (battery === null || battery > 100) return { state: "offline" };
			// 0 = on battery, 1 = charging, 2 = full and still on the cable.
			const charging = (await query(device, CMD_CHARGING)) === 1;
			return { state: "online", battery, charging };
		} finally {
			await device.close();
		}
	} catch {
		// Unplugged mid-poll, or the collection could not be opened.
		return { state: "no-dongle" };
	}
}

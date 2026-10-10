import { action, type KeyDownEvent } from "@elgato/streamdeck";

import type { HyperXSnapshot } from "../hyperx";
import { hyperxPoller } from "../poller";
import { batteryKey, glyphs, idleKey } from "../render";
import { PolledAction } from "./base";

const NAME = "Cloud III S";

/** Battery gauge for a HyperX Cloud III S Wireless, read from its USB dongle. Pressing it refreshes immediately. */
@action({ UUID: "com.sonydevicecenter.hyperx-battery" })
export class HyperXBatteryAction extends PolledAction<HyperXSnapshot> {
	constructor() {
		super(hyperxPoller);
	}

	protected image(s: HyperXSnapshot): string {
		if (s.state === "no-dongle") return idleKey(glyphs.power, "No dongle", NAME);
		if (s.state === "offline") return idleKey(glyphs.power, "Offline", NAME);
		return batteryKey(s.battery, s.charging, null, NAME);
	}

	override async onKeyDown(_ev: KeyDownEvent): Promise<void> {
		await hyperxPoller.refresh();
	}
}

import { action, type KeyDownEvent } from "@elgato/streamdeck";

import type { Snapshot } from "../api";
import { poller } from "../poller";
import { batteryKey, glyphs, idleKey } from "../render";
import { StatusAction } from "./base";

/** Live battery gauge. Pressing it refreshes immediately. */
@action({ UUID: "com.sonydevicecenter.battery" })
export class BatteryAction extends StatusAction {
	protected image(s: Snapshot): string {
		if (!s.reachable) return idleKey(glyphs.power, "App off", "Start the app");
		if (!s.connected || s.battery === null) return idleKey(glyphs.power, "Offline", s.device || "Headphones");
		return batteryKey(s.battery, s.charging, s.device.replace(/^WH-|^WF-/, ""));
	}

	override async onKeyDown(_ev: KeyDownEvent): Promise<void> {
		await poller.refresh();
	}
}

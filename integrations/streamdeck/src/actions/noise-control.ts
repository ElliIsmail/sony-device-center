import { action, type KeyDownEvent } from "@elgato/streamdeck";

import type { Snapshot } from "../api";
import { glyphs, idleKey, noiseControlKey } from "../render";
import { StatusAction } from "./base";

/** Shows the noise control mode; pressing cycles Noise Cancelling -> Ambient -> Off. */
@action({ UUID: "com.sonydevicecenter.noise-control" })
export class NoiseControlAction extends StatusAction {
	protected image(s: Snapshot): string {
		if (!s.reachable) return idleKey(glyphs.shield, "App off", "Start the app");
		if (!s.connected || s.noiseControl === "unknown") return idleKey(glyphs.shield, "Offline", "Noise control");
		return noiseControlKey(s.noiseControl, s.ambientLevel);
	}

	override async onKeyDown(ev: KeyDownEvent): Promise<void> {
		await this.run(ev.action, "/noise-control/next");
	}
}

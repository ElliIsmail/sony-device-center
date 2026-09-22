import { action, type KeyDownEvent } from "@elgato/streamdeck";

import type { Snapshot } from "../api";
import { glyphs, idleKey, speakToChatKey } from "../render";
import { StatusAction } from "./base";

/** Shows whether Speak-to-Chat is on; pressing toggles it. */
@action({ UUID: "com.sonydevicecenter.speak-to-chat" })
export class SpeakToChatAction extends StatusAction {
	protected image(s: Snapshot): string {
		if (!s.reachable) return idleKey(glyphs.chat, "App off", "Start the app");
		if (!s.connected) return idleKey(glyphs.chat, "Offline", "Speak-to-Chat");
		if (s.speakToChat === null) return idleKey(glyphs.chat, "N/A", "Not on this model");
		return speakToChatKey(s.speakToChat);
	}

	override async onKeyDown(ev: KeyDownEvent): Promise<void> {
		await this.run(ev.action, "/speak-to-chat/toggle");
	}
}

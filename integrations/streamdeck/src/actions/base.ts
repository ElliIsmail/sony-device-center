import streamDeck, { SingletonAction, type KeyDownEvent, type WillAppearEvent, type WillDisappearEvent } from "@elgato/streamdeck";

import { command, type Snapshot } from "../api";
import { poller } from "../poller";

// Visible keys across every action; polling runs only while this is non-zero.
let visibleKeys = 0;

/** Shared plumbing: redraw every visible key of this action on each poll. */
export abstract class StatusAction extends SingletonAction {
	constructor() {
		super();
		poller.subscribe((snapshot) => void this.#renderAll(snapshot));
	}

	/** The key image (SVG data URL) for a snapshot. */
	protected abstract image(snapshot: Snapshot): string;

	override async onWillAppear(ev: WillAppearEvent): Promise<void> {
		visibleKeys++;
		poller.setActive(true);
		if (ev.action.isKey()) await ev.action.setImage(this.image(poller.latest));
	}

	override onWillDisappear(_ev: WillDisappearEvent): void {
		visibleKeys = Math.max(0, visibleKeys - 1);
		poller.setActive(visibleKeys > 0);
	}

	/** Runs a command for a key press; flashes the key's alert on failure. */
	protected async run(action: KeyDownEvent["action"], path: string): Promise<void> {
		const error = await command(path);
		if (error) {
			streamDeck.logger.warn(`${path}: ${error}`);
			await action.showAlert();
		}
		await poller.refresh();
	}

	async #renderAll(snapshot: Snapshot): Promise<void> {
		const image = this.image(snapshot);
		await Promise.all([...this.actions].map((action) => (action.isKey() ? action.setImage(image) : undefined)));
	}
}

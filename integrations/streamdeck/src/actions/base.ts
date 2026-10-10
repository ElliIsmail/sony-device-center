import streamDeck, { SingletonAction, type KeyDownEvent, type WillAppearEvent, type WillDisappearEvent } from "@elgato/streamdeck";

import { command, type Snapshot } from "../api";
import { poller, type Poller } from "../poller";

/** Shared plumbing: redraw every visible key of this action on each poll of its source. */
export abstract class PolledAction<T> extends SingletonAction {
	readonly #source: Poller<T>;

	constructor(source: Poller<T>) {
		super();
		this.#source = source;
		source.subscribe((snapshot) => void this.#renderAll(snapshot));
	}

	/** The key image (SVG data URL) for a snapshot. */
	protected abstract image(snapshot: T): string;

	override async onWillAppear(ev: WillAppearEvent): Promise<void> {
		this.#source.retain();
		if (ev.action.isKey()) await ev.action.setImage(this.image(this.#source.latest));
	}

	override onWillDisappear(_ev: WillDisappearEvent): void {
		this.#source.release();
	}

	async #renderAll(snapshot: T): Promise<void> {
		const image = this.image(snapshot);
		await Promise.all([...this.actions].map((action) => (action.isKey() ? action.setImage(image) : undefined)));
	}
}

/** A key backed by the Sony Device Center app's local API. */
export abstract class StatusAction extends PolledAction<Snapshot> {
	constructor() {
		super(poller);
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
}

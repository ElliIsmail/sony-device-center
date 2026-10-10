import { fetchStatus, type Snapshot } from "./api";
import { readHyperX, type HyperXSnapshot } from "./hyperx";

type Listener<T> = (snapshot: T) => void;

/**
 * Polls a source while at least one key that uses it is visible and fans the
 * result out to every subscribed action, so N keys cost one request per interval.
 */
export class Poller<T> {
	#listeners = new Set<Listener<T>>();
	#timer: NodeJS.Timeout | undefined;
	#latest: T;
	#inFlight = false;
	#visibleKeys = 0;
	readonly #fetch: () => Promise<T>;
	readonly #intervalMs: number;

	constructor(fetch: () => Promise<T>, intervalMs: number, initial: T) {
		this.#fetch = fetch;
		this.#intervalMs = intervalMs;
		this.#latest = initial;
	}

	get latest(): T {
		return this.#latest;
	}

	subscribe(listener: Listener<T>): () => void {
		this.#listeners.add(listener);
		return () => this.#listeners.delete(listener);
	}

	/** Called when a key using this source appears; starts polling with the first one. */
	retain(): void {
		if (this.#visibleKeys++ === 0) {
			this.#timer = setInterval(() => void this.refresh(), this.#intervalMs);
			void this.refresh();
		}
	}

	/** Called when such a key disappears; stops polling with the last one. */
	release(): void {
		this.#visibleKeys = Math.max(0, this.#visibleKeys - 1);
		if (this.#visibleKeys === 0 && this.#timer) {
			clearInterval(this.#timer);
			this.#timer = undefined;
		}
	}

	/** Polls now, e.g. right after a key press so the key reflects it quickly. */
	async refresh(): Promise<void> {
		if (this.#inFlight) return;
		this.#inFlight = true;
		try {
			this.#latest = await this.#fetch();
			for (const listener of this.#listeners) listener(this.#latest);
		} finally {
			this.#inFlight = false;
		}
	}
}

/** The Sony Device Center app's local API. */
export const poller = new Poller<Snapshot>(fetchStatus, 3000, { reachable: false });

/** The HyperX dongle. Battery moves slowly, so a longer interval is plenty. */
export const hyperxPoller = new Poller<HyperXSnapshot>(readHyperX, 10000, { state: "no-dongle" });

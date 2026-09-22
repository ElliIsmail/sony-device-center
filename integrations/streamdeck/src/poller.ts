import { fetchStatus, type Snapshot } from "./api";

type Listener = (snapshot: Snapshot) => void;

const INTERVAL_MS = 3000;

/**
 * Polls the app while at least one key is visible and fans the result out to
 * every action, so N keys cost one request per interval.
 */
class Poller {
	#listeners = new Set<Listener>();
	#timer: NodeJS.Timeout | undefined;
	#latest: Snapshot = { reachable: false };
	#inFlight = false;

	get latest(): Snapshot {
		return this.#latest;
	}

	subscribe(listener: Listener): () => void {
		this.#listeners.add(listener);
		return () => this.#listeners.delete(listener);
	}

	/** Called on every key appear/disappear with the number of visible keys. */
	setActive(active: boolean): void {
		if (active && !this.#timer) {
			this.#timer = setInterval(() => void this.refresh(), INTERVAL_MS);
			void this.refresh();
		} else if (!active && this.#timer) {
			clearInterval(this.#timer);
			this.#timer = undefined;
		}
	}

	/** Polls now, e.g. right after a key press so the key reflects it quickly. */
	async refresh(): Promise<void> {
		if (this.#inFlight) return;
		this.#inFlight = true;
		try {
			this.#latest = await fetchStatus();
			for (const listener of this.#listeners) listener(this.#latest);
		} finally {
			this.#inFlight = false;
		}
	}
}

export const poller = new Poller();

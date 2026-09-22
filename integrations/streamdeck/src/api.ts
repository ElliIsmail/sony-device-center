// Client for the Sony Device Center local API (docs/local-api.md). The app
// owns the only Bluetooth control channel, so the plugin never talks to the
// headphones directly.

const BASE_URL = "http://127.0.0.1:47821";
const TIMEOUT_MS = 2000;

export type NoiseControl = "cancelling" | "ambient" | "off" | "unknown";

export type Status = {
	connected: boolean;
	device: string;
	battery: number | null;
	charging: boolean;
	noiseControl: NoiseControl;
	ambientLevel: number;
	/** null when the model has no Speak-to-Chat. */
	speakToChat: boolean | null;
};

/** What the plugin knows: the app is unreachable, or its latest status. */
export type Snapshot = { reachable: false } | ({ reachable: true } & Status);

export async function fetchStatus(): Promise<Snapshot> {
	try {
		const res = await fetch(`${BASE_URL}/status`, { signal: AbortSignal.timeout(TIMEOUT_MS) });
		if (!res.ok) return { reachable: false };
		return { reachable: true, ...((await res.json()) as Status) };
	} catch {
		return { reachable: false };
	}
}

/** Sends a command; resolves to an error message, or null on success. */
export async function command(path: string): Promise<string | null> {
	try {
		const res = await fetch(`${BASE_URL}${path}`, {
			method: "POST",
			// Required by the app for every state change (blocks cross-site requests).
			headers: { "X-Sony-Device-Center": "1" },
			signal: AbortSignal.timeout(TIMEOUT_MS),
		});
		if (res.ok) return null;
		const body = (await res.json().catch(() => ({}))) as { error?: string };
		return body.error ?? `HTTP ${res.status}`;
	} catch {
		return "Sony Device Center is not running";
	}
}

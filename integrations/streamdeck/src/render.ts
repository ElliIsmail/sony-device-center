// Key images as SVG data URLs, 144x144 (the @2x key size). Colours match the
// desktop app's design tokens and its tray badge.

const C = {
	bg: "#0A0B0F",
	track: "#22252F",
	text: "#F4F6FA",
	dim: "#98A1B2",
	faint: "#5C6473",
	accent: "#7C5CFF",
	warm: "#F2A73B",
	success: "#2DD4A7",
	danger: "#FF5A5F",
	charging: "#3B82F6",
};

const FONT = "Segoe UI, Helvetica Neue, Arial, sans-serif";

// 24x24 stroke glyphs, same shapes as the app's icon set.
const GLYPH = {
	shield: "M12 22s8-4 8-10V5l-8-3-8 3v7c0 6 8 10 8 10z",
	mic: "M12 2a3 3 0 0 0-3 3v7a3 3 0 0 0 6 0V5a3 3 0 0 0-3-3z M19 10v2a7 7 0 0 1-14 0v-2 M12 19v3",
	power: "M18.36 6.64a9 9 0 1 1-12.73 0 M12 2v10",
	bolt: "M13 2L3 14h9l-1 8 10-12h-9l1-8z",
	chat: "M21 15a2 2 0 0 1-2 2H7l-4 4V5a2 2 0 0 1 2-2h14a2 2 0 0 1 2 2z",
};

function escape(text: string): string {
	return text.replace(/[&<>"']/g, (c) => `&#${c.charCodeAt(0)};`);
}

function toDataUrl(body: string): string {
	const svg = `<svg xmlns="http://www.w3.org/2000/svg" width="144" height="144" viewBox="0 0 144 144">`
		+ `<rect width="144" height="144" fill="${C.bg}"/>${body}</svg>`;
	return `data:image/svg+xml;charset=utf8,${encodeURIComponent(svg)}`;
}

function glyph(path: string, color: string, x: number, y: number, size: number, filled = false): string {
	const scale = size / 24;
	const paint = filled ? `fill="${color}" stroke="none"` : `fill="none" stroke="${color}" stroke-width="2" stroke-linecap="round" stroke-linejoin="round"`;
	return `<g transform="translate(${x} ${y}) scale(${scale})"><path d="${path}" ${paint}/></g>`;
}

function label(text: string, y: number, size: number, color: string, weight = 600): string {
	return `<text x="72" y="${y}" text-anchor="middle" font-family="${FONT}" font-size="${size}" font-weight="${weight}" fill="${color}">${escape(text)}</text>`;
}

export function batteryColor(level: number, charging: boolean): string {
	if (charging) return C.charging;
	if (level <= 20) return C.danger;
	if (level <= 50) return C.warm;
	return C.success;
}

/** Ring gauge filled to the battery level, with the percentage in the middle. */
export function batteryKey(level: number, charging: boolean, caption: string): string {
	const r = 50;
	const circumference = 2 * Math.PI * r;
	const filled = (Math.max(0, Math.min(100, level)) / 100) * circumference;
	const color = batteryColor(level, charging);
	const ring = `<circle cx="72" cy="62" r="${r}" fill="none" stroke="${C.track}" stroke-width="9"/>`
		+ `<circle cx="72" cy="62" r="${r}" fill="none" stroke="${color}" stroke-width="9" stroke-linecap="round"`
		+ ` stroke-dasharray="${filled} ${circumference}" transform="rotate(-90 72 62)"/>`;
	const number = label(String(level), 76, level >= 100 ? 40 : 46, C.text, 700);
	const unit = charging ? glyph(GLYPH.bolt, color, 63, 84, 18, true) : label("%", 98, 16, C.dim);
	return toDataUrl(ring + number + unit + label(caption, 138, 15, C.dim));
}

/** Grey key for "app not running" / "headphones disconnected" / unsupported. */
export function idleKey(path: string, title: string, subtitle: string): string {
	return toDataUrl(glyph(path, C.faint, 44, 22, 56) + label(title, 108, 20, C.dim) + label(subtitle, 130, 14, C.faint, 400));
}

export function noiseControlKey(mode: "cancelling" | "ambient" | "off", ambientLevel: number): string {
	const spec = {
		cancelling: { path: GLYPH.shield, color: C.accent, title: "Noise Cancel", sub: "Press: Ambient" },
		ambient: { path: GLYPH.mic, color: C.warm, title: `Ambient ${ambientLevel}`, sub: "Press: Off" },
		off: { path: GLYPH.power, color: C.dim, title: "Off", sub: "Press: Noise Cancel" },
	}[mode];
	const halo = `<circle cx="72" cy="50" r="36" fill="${spec.color}" fill-opacity="0.14" stroke="${spec.color}" stroke-opacity="0.45" stroke-width="2"/>`;
	return toDataUrl(halo + glyph(spec.path, spec.color, 50, 28, 44) + label(spec.title, 112, 20, C.text) + label(spec.sub, 134, 13, C.faint, 400));
}

export function speakToChatKey(enabled: boolean): string {
	const color = enabled ? C.success : C.faint;
	const halo = `<circle cx="72" cy="50" r="36" fill="${color}" fill-opacity="${enabled ? 0.16 : 0.06}" stroke="${color}" stroke-opacity="0.5" stroke-width="2"/>`;
	return toDataUrl(halo + glyph(GLYPH.chat, color, 50, 28, 44) + label("Speak-to-Chat", 112, 17, C.text) + label(enabled ? "ON" : "OFF", 134, 16, color, 700));
}

export const glyphs = GLYPH;

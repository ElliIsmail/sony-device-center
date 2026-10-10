// Bundles src/plugin.ts into the .sdPlugin folder that Stream Deck loads.
import * as esbuild from "esbuild";
import { cpSync, rmSync } from "node:fs";

const BIN = "com.sonydevicecenter.sdPlugin/bin";

const options = {
	entryPoints: ["src/plugin.ts"],
	outfile: `${BIN}/plugin.js`,
	bundle: true,
	platform: "node",
	format: "esm",
	target: "node20",
	sourcemap: true,
	logLevel: "info",
	// node-hid finds its native binary relative to its own folder, so it cannot
	// be bundled; it ships as-is in bin/node_modules (see copyNativeModules).
	external: ["node-hid"],
	// The SDK's ESM output still reaches for require() in a few places.
	banner: { js: "import { createRequire } from 'node:module'; const require = createRequire(import.meta.url);" },
};

/** Copies node-hid's runtime files, with only the Stream Deck platforms' prebuilds. */
function copyNativeModules() {
	const out = `${BIN}/node_modules`;
	rmSync(out, { recursive: true, force: true });
	const copy = (from) => cpSync(`node_modules/${from}`, `${out}/${from}`, { recursive: true });

	for (const file of ["package.json", "nodehid.js", "binding-options.js", "LICENSE-bsd.txt"]) copy(`node-hid/${file}`);
	for (const target of ["win32-x64", "win32-arm64", "darwin-x64", "darwin-arm64"]) copy(`node-hid/prebuilds/HID-${target}`);
	for (const file of ["package.json", "bindings.js", "lib", "LICENSE"]) copy(`pkg-prebuilds/${file}`);
}

copyNativeModules();

if (process.argv.includes("--watch")) {
	await (await esbuild.context(options)).watch();
} else {
	await esbuild.build(options);
}

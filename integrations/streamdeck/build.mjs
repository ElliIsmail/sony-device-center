// Bundles src/plugin.ts into the .sdPlugin folder that Stream Deck loads.
import * as esbuild from "esbuild";

const options = {
	entryPoints: ["src/plugin.ts"],
	outfile: "com.sonydevicecenter.sdPlugin/bin/plugin.js",
	bundle: true,
	platform: "node",
	format: "esm",
	target: "node20",
	sourcemap: true,
	logLevel: "info",
	// The SDK's ESM output still reaches for require() in a few places.
	banner: { js: "import { createRequire } from 'node:module'; const require = createRequire(import.meta.url);" },
};

if (process.argv.includes("--watch")) {
	await (await esbuild.context(options)).watch();
} else {
	await esbuild.build(options);
}

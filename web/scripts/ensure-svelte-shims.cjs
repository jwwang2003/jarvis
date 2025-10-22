#!/usr/bin/env node

const fs = require('fs');
const path = require('path');

const SHIM_FILENAMES = ['svelte-native-jsx.d.ts', 'svelte-shims-v4.d.ts'];
const POSSIBLE_MODULE_PATHS = ['svelte-check/dist/src', 'svelte2tsx'];

function resolveShim(filename) {
	for (const base of POSSIBLE_MODULE_PATHS) {
		try {
			return require.resolve(path.posix.join(base, filename));
		} catch (_) {
			// Try next base path
		}
	}

	return undefined;
}

function ensureShims() {
	const targetDir = path.resolve(__dirname, '..', 'node_modules', '.svelte2tsx-language-server-files');
	fs.mkdirSync(targetDir, { recursive: true });

	const missing = [];
	for (const filename of SHIM_FILENAMES) {
		const sourcePath = resolveShim(filename);
		if (!sourcePath) {
			missing.push(filename);
			continue;
		}

		const targetPath = path.join(targetDir, filename);
		const sourceContent = fs.readFileSync(sourcePath, 'utf8');
		let needsWrite = true;
		if (fs.existsSync(targetPath)) {
			const currentContent = fs.readFileSync(targetPath, 'utf8');
			needsWrite = currentContent !== sourceContent;
		}

		if (needsWrite) {
			fs.writeFileSync(targetPath, sourceContent, 'utf8');
		}
	}

	if (missing.length > 0) {
		console.warn(
			`[ensure-svelte-shims] Unable to locate shim${missing.length > 1 ? 's' : ''}: ${missing.join(', ')}`
		);
	}
}

ensureShims();

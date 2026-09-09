import { build } from 'esbuild';
import { copyFile } from 'node:fs/promises';
import { dirname, join, resolve } from 'node:path';
import { fileURLToPath } from 'node:url';

const source = dirname(fileURLToPath(import.meta.url));
if (!process.argv[2]) throw new Error('Usage: node build.mjs <output-directory>');
const output = resolve(process.argv[2]);
await build({
  entryPoints: [join(source, 'index.mjs')],
  outfile: join(output, 'codex-api-usage.mjs'),
  bundle: true,
  platform: 'node',
  format: 'esm',
  target: 'node22',
  legalComments: 'eof',
});
await copyFile(join(source, 'node_modules/smol-toml/LICENSE'), join(output, 'codex-api-usage.LICENSE.txt'));

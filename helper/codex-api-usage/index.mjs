import { createHash, randomUUID } from 'node:crypto';
import { readFile, mkdir, writeFile, rename, rm } from 'node:fs/promises';
import { homedir } from 'node:os';
import { dirname, join, resolve } from 'node:path';
import { pathToFileURL } from 'node:url';
import { parse } from 'smol-toml';

export class UsageError extends Error {}

export async function readState(root) {
  const [auth, config] = await Promise.all([
    readFile(join(root, 'auth.json')), readFile(join(root, 'config.toml')),
  ]);
  if (auth.length > 1024 * 1024 || config.length > 1024 * 1024) {
    throw new UsageError('Codex configuration is too large');
  }
  const fingerprint = createHash('sha256').update(auth).update(config).digest('hex');
  return { auth, config, fingerprint };
}

export function resolveAccount(authBytes, configBytes) {
  const auth = JSON.parse(authBytes.toString('utf8').replace(/^\uFEFF/, ''));
  const config = parse(configBytes.toString('utf8').replace(/^\uFEFF/, ''));
  // OAuth may coexist with a custom provider's API key (for example, for image generation).
  const provider = config.model_providers?.[config.model_provider] ?? {};
  if (config.profile) throw new UsageError('Codex profile overrides are not supported');
  if (provider.env_key) {
    throw new UsageError('Provider key overrides are not supported');
  }
  const key = provider.experimental_bearer_token ?? auth.OPENAI_API_KEY;
  if (typeof key !== 'string' || !key.trim()) throw new UsageError('No file-based Codex API key');
  if (typeof provider.base_url !== 'string' || !provider.base_url.trim()) {
    throw new UsageError('A custom HTTPS provider base_url is required');
  }
  const url = new URL(provider.base_url);
  if (url.protocol !== 'https:' || !url.hostname || url.username || url.password) {
    throw new UsageError('A custom HTTPS provider base_url is required');
  }
  if (url.search || url.hash) throw new UsageError('Provider base_url must not contain query or fragment');
  const path = url.pathname.replace(/\/+$/, '');
  url.pathname = `${path}/usage`;
  const urls = [url.href];
  if (!path) {
    url.pathname = '/v1/usage';
    urls.push(url.href);
  }
  return { key: key.trim(), urls };
}

function number(value) {
  if (typeof value !== 'number' && (typeof value !== 'string' || !value.trim())) return null;
  const result = Number(value);
  return Number.isFinite(result) ? result : null;
}

export function summarize(body) {
  if (!body || typeof body !== 'object' || Array.isArray(body)) throw new UsageError('Unsupported usage response');
  const metrics = {};
  const add = (prefix, limitValue, usedValue, reset, source, unit = 'USD') => {
    const limit = number(limitValue);
    const used = number(usedValue);
    if (limit === null || limit <= 0 || used === null || used < 0) return;
    if (typeof unit !== 'string' || !/^[A-Za-z]{1,8}$/.test(unit)) return;
    const percentage = used >= limit ? 100 : used / limit * 100;
    metrics[`${prefix}_percentage`] = percentage.toFixed(1);
    let detail = `${source}: ${used.toFixed(2)} / ${limit.toFixed(2)} ${unit}`;
    if (typeof reset === 'string' && Number.isFinite(Date.parse(reset))) {
      detail += `; resets ${new Date(reset).toISOString()}`;
    }
    metrics[`${prefix}_detail`] = detail;
  };
  const windows = { '5h': 'five_hour', '1d': 'one_day', '7d': 'seven_day' };
  if (Array.isArray(body.rate_limits)) {
    for (const entry of body.rate_limits) {
      const prefix = windows[entry?.window];
      if (prefix) add(prefix, entry.limit, entry.used, entry.reset_at, 'API key');
    }
  }
  // Key limits and subscription limits are different scopes. Do not mix them in quota_limited mode.
  if (body.mode !== 'quota_limited' && body.subscription) {
    const sub = body.subscription;
    if (!('one_day_percentage' in metrics)) add('one_day', sub.daily_limit_usd, sub.daily_usage_usd, null, 'Subscription');
    if (!('seven_day_percentage' in metrics)) add('seven_day', sub.weekly_limit_usd, sub.weekly_usage_usd, null, 'Subscription');
  }
  if (body.quota && body.quota.unlimited !== true) {
    add('total', body.quota.limit, body.quota.used, null, 'API key total', body.quota.unit ?? 'USD');
  }
  return metrics;
}

export async function query(key, urls, fetchUsage = fetch) {
  for (const [index, url] of urls.entries()) {
    const response = await fetchUsage(url, {
      headers: { Authorization: `Bearer ${key}`, Accept: 'application/json' },
      redirect: 'manual', signal: AbortSignal.timeout(10000),
    });
    if (!response.ok) {
      await response.body?.cancel();
      if (response.status === 404 && index + 1 < urls.length) continue;
      throw new UsageError(`Usage HTTP ${response.status}`);
    }
    const chunks = [];
    let size = 0;
    for await (const chunk of response.body) {
      size += chunk.length;
      if (size > 1024 * 1024) throw new UsageError('Usage response is too large');
      chunks.push(chunk);
    }
    return summarize(JSON.parse(Buffer.concat(chunks).toString('utf8')));
  }
  throw new UsageError('Usage endpoint not found');
}

export async function collect(root, queryUsage = query) {
  const snapshot = { version: '2', fingerprint: '', status: 'Unavailable' };
  try {
    const { auth, config, fingerprint } = await readState(root);
    snapshot.fingerprint = fingerprint;
    const { key, urls } = resolveAccount(auth, config);
    const metrics = await queryUsage(key, urls);
    if ((await readState(root)).fingerprint !== fingerprint) throw new UsageError('Account changed during refresh');
    Object.assign(snapshot, metrics, { status: 'OK' });
  } catch (error) {
    // Never persist response bodies, URLs, keys, or raw exception messages.
    snapshot.status = error instanceof UsageError ? error.message : 'Configuration, network, or response error';
  }
  snapshot.updated = String(Math.floor(Date.now() / 1000));
  return snapshot;
}

export async function writeSnapshot(path, snapshot) {
  // The protocol is one flat INI section containing only validated single-line values.
  const lines = Object.entries(snapshot).map(([key, value]) => {
    if (!/^[a-z_]+$/.test(key) || /[\r\n\0]/.test(value)) throw new UsageError('Invalid snapshot field');
    return `${key}=${value}\r\n`;
  });
  await mkdir(dirname(path), { recursive: true });
  const temporary = `${path}.${randomUUID()}.tmp`;
  try {
    await writeFile(temporary, `\uFEFF[usage]\r\n${lines.join('')}`, { encoding: 'utf16le', flag: 'wx' });
    await rename(temporary, path);
  } finally {
    await rm(temporary, { force: true });
  }
}

if (process.argv[1] && import.meta.url === pathToFileURL(resolve(process.argv[1])).href) {
  // Bound the entire helper lifetime, including slow response bodies.
  const deadline = setTimeout(() => process.exit(1), 25000);
  try {
    if (Number(process.versions.node.split('.')[0]) < 22) throw new Error('Unsupported Node.js version');
    let root = process.env.CODEX_HOME || join(homedir(), '.codex');
    if (/^~[\\/]/.test(root)) root = join(homedir(), root.slice(2));
    const cache = join(process.env.LOCALAPPDATA, 'trafficmonitor-claude-usage-plugin', 'codex-api-usage.ini');
    await writeSnapshot(cache, await collect(root));
  } catch {
    process.exitCode = 1;
  } finally {
    clearTimeout(deadline);
  }
}

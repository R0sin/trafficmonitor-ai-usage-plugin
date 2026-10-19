import assert from 'node:assert/strict';
import { test } from 'node:test';
import { mkdtemp, readFile, writeFile, rm } from 'node:fs/promises';
import { tmpdir } from 'node:os';
import { join, resolve } from 'node:path';
import { spawnSync } from 'node:child_process';
import { pathToFileURL } from 'node:url';
const { collect, query, readState, resolveAccount, summarize, UsageError, writeSnapshot } = await import(
  process.env.CODEX_TEST_HELPER_MODULE
    ? pathToFileURL(resolve(process.env.CODEX_TEST_HELPER_MODULE)).href
    : '../helper/codex-api-usage/index.mjs'
);

const auth = Buffer.from('{"auth_mode":"apikey","OPENAI_API_KEY":"test-secret"}');
const config = Buffer.from('model_provider = "custom"\n[model_providers.custom]\nbase_url = \'https://example.com/v1/\'\n');
const body = { mode: 'quota_limited', rate_limits: [
  { window: '7d', limit: 100, used: 120 },
  { window: '5h', limit: '20', used: '2.5', reset_at: '2026-09-09T12:00:00Z' },
  { window: '1d', limit: 50, used: 0 },
], quota: { limit: 1000, used: 250, unit: 'USD' } };

test('TOML account resolution and Sub2API metrics', () => {
  assert.deepEqual(resolveAccount(auth, config), { key: 'test-secret', urls: ['https://example.com/v1/usage'] });
  assert.equal(resolveAccount(auth, Buffer.from(`${config}experimental_bearer_token = "test-secret"\n`)).key, 'test-secret');
  assert.deepEqual(resolveAccount(auth, Buffer.from(config.toString().replace('/v1/', '/'))).urls,
    ['https://example.com/usage', 'https://example.com/v1/usage']);
  const metrics = summarize(body);
  assert.equal(metrics.five_hour_percentage, '12.5');
  assert.equal(metrics.one_day_percentage, '0.0');
  assert.equal(metrics.seven_day_percentage, '100.0');
  assert.equal(metrics.total_percentage, '25.0');
  assert.equal(metrics.five_hour_reset_at, String(Date.parse('2026-09-09T12:00:00Z') / 1000));
  assert.equal(metrics.five_hour_detail, '2.50/20.00 USD');
  for (const reset_at of [null, '', 'invalid', '1960-01-01T00:00:00Z', '+010000-01-01T00:00:00Z']) {
    assert.equal(summarize({ rate_limits: [{ window: '5h', limit: 20, used: 2, reset_at }] }).five_hour_reset_at, undefined);
  }
  assert.ok(metrics.seven_day_detail.includes('120.00/100.00 USD'));
  assert.deepEqual(summarize({ balance: 12, remaining: 8, usage: { today: { requests: 1 } } }), {});
  for (const value of [null, true, '', ' ', 'NaN', 'Infinity', [], {}, 0, -1]) {
    assert.deepEqual(summarize({ quota: { limit: value, used: 5 } }), {});
  }
  assert.deepEqual(summarize({ quota: { limit: 100, used: -1 } }), {});
  assert.deepEqual(summarize({ quota: { limit: 100, remaining: 50 } }), {});
  assert.deepEqual(summarize({ quota: { limit: 100, used: 5, unlimited: true } }), {});
  for (const invalid of [config.toString().replace('https:', 'http:'),
    `${config}env_key = "KEY"\n`]) {
    assert.throws(() => resolveAccount(auth, Buffer.from(invalid)), UsageError);
  }
});

test('custom provider API key remains usable with an OAuth binding', () => {
  const mixedAuth = Buffer.from(JSON.stringify({
    auth_mode: 'chatgpt', OPENAI_API_KEY: 'test-secret',
    tokens: { access_token: 'oauth-secret' },
  }));
  assert.deepEqual(resolveAccount(mixedAuth, config), {
    key: 'test-secret', urls: ['https://example.com/v1/usage'],
  });
  assert.equal(resolveAccount(mixedAuth, Buffer.from(`${config}experimental_bearer_token = "test-secret"\n`)).key,
    'test-secret');
  assert.throws(() => resolveAccount(mixedAuth, Buffer.from('model_provider = "openai"\n')), UsageError);
  assert.throws(() => resolveAccount(Buffer.from('{"auth_mode":"chatgpt","tokens":{"access_token":"oauth-secret"}}'), config), UsageError);
});

test('provider bearer token supplies the API key when auth.json contains only OAuth', () => {
  const oauth = Buffer.from('{"auth_mode":"chatgpt","tokens":{"access_token":"oauth-secret"}}');
  const providerConfig = Buffer.from(`${config}experimental_bearer_token = "provider-secret"\n`);
  assert.deepEqual(resolveAccount(oauth, providerConfig), {
    key: 'provider-secret', urls: ['https://example.com/v1/usage'],
  });
  assert.equal(resolveAccount(auth, providerConfig).key, 'provider-secret');
  for (const token of ['""', '" "', '123', 'false']) {
    assert.throws(() => resolveAccount(auth, Buffer.from(`${config}experimental_bearer_token = ${token}\n`)), UsageError);
  }
  assert.throws(() => resolveAccount(oauth, Buffer.from(`${providerConfig}env_key = "KEY"\n`)), UsageError);
});

test('subscription fallback keeps daily/weekly scope separate from key limits', () => {
  const subscription = { daily_limit_usd: 20, daily_usage_usd: 10, weekly_limit_usd: 100, weekly_usage_usd: 30 };
  const metrics = summarize({ mode: 'unrestricted', subscription });
  assert.equal(metrics.one_day_percentage, '50.0');
  assert.equal(metrics.seven_day_percentage, '30.0');
  assert.equal(metrics.one_day_detail, '10.00/20.00 USD');
  assert.equal(metrics.five_hour_percentage, undefined);
  assert.equal(metrics.total_percentage, undefined);
  assert.deepEqual(summarize({ mode: 'quota_limited', subscription }), {});
  assert.equal(summarize({ subscription, rate_limits: [{ window: '1d', limit: 10, used: 2 }] }).one_day_percentage, '20.0');
  assert.deepEqual(summarize({ rate_limits: [null, { window: '30d', limit: 1, used: 1 }] }), {});
});

test('subscription weekly reset follows its seven-day window without mixing key limits', () => {
  const subscription = {
    weekly_limit_usd: 200, weekly_usage_usd: 109,
    weekly_window_start: '2026-10-15T15:58:07.422626+08:00',
    expires_at: '2027-03-08T11:25:38+08:00',
  };
  const metrics = summarize({ mode: 'unrestricted', subscription });
  assert.equal(metrics.seven_day_reset_at, String(Date.parse('2026-10-22T15:58:07+08:00') / 1000));
  assert.equal(metrics.seven_day_percentage, '54.5');
  for (const weekly_window_start of [undefined, null, '', 'invalid', 123, '+275760-09-13T00:00:00.000Z']) {
    const invalid = summarize({ subscription: { ...subscription, weekly_window_start } });
    assert.equal(invalid.seven_day_reset_at, undefined);
    assert.equal(invalid.seven_day_percentage, '54.5');
  }
  assert.deepEqual(summarize({ mode: 'quota_limited', subscription }), {});
  const keyLimit = { window: '7d', limit: 100, used: 10 };
  assert.equal(summarize({ subscription, rate_limits: [keyLimit] }).seven_day_reset_at, undefined);
  assert.equal(summarize({ subscription, rate_limits: [{ ...keyLimit, reset_at: '2026-10-20T00:00:00Z' }] }).seven_day_reset_at,
    String(Date.parse('2026-10-20T00:00:00Z') / 1000));
});

test('HTTP fallback, authentication, redirect rejection and response size', async () => {
  const calls = [];
  const result = await query('test-secret', ['https://example.com/usage', 'https://example.com/v1/usage'], async (url, options) => {
    calls.push({ url, options });
    return calls.length === 1 ? new Response('missing', { status: 404 }) : Response.json(body);
  });
  assert.equal(result.five_hour_percentage, '12.5');
  assert.equal(calls.length, 2);
  assert.equal(calls[1].options.headers.Authorization, 'Bearer test-secret');
  assert.equal(calls[1].options.redirect, 'manual');
  for (const status of [401, 302, 500]) {
    await assert.rejects(query('secret', ['https://example.com/usage'], async () => new Response('secret', { status })),
      { message: `Usage HTTP ${status}` });
  }
  await assert.rejects(query('secret', ['https://example.com/usage'], async () => new Response('x'.repeat(1024 * 1024 + 1))),
    { message: 'Usage response is too large' });
});

test('account switching, sanitized failures, atomic UTF-16 snapshot and Windows reader', async () => {
  const root = await mkdtemp(join(tmpdir(), 'codex-api-node-test-'));
  try {
    await writeFile(join(root, 'auth.json'), auth);
    await writeFile(join(root, 'config.toml'), config);
    const snapshot = await collect(root, async () => summarize(body));
    assert.equal(snapshot.status, 'OK');
    assert.equal(snapshot.fingerprint, (await readState(root)).fingerprint);
    const path = join(root, 'usage.ini');
    await writeSnapshot(path, snapshot);
    await writeSnapshot(path, snapshot);
    const raw = await readFile(path);
    assert.deepEqual([...raw.subarray(0, 2)], [0xff, 0xfe]);
    assert.ok(raw.toString('utf16le').includes('five_hour_percentage=12.5\r\n'));
    assert.equal(snapshot.version, '2');
    assert.ok(!('balance' in snapshot) && !('tokens' in snapshot) && !('requests' in snapshot));
    assert.ok(!raw.toString('utf16le').includes('test-secret'));
    if (process.platform === 'win32') {
      const check = spawnSync('powershell.exe', ['-NoProfile', '-Command', `
        Add-Type -TypeDefinition 'using System.Runtime.InteropServices; using System.Text; public class Ini { [DllImport("kernel32.dll", CharSet=CharSet.Unicode)] public static extern uint GetPrivateProfileString(string section, string key, string def, StringBuilder result, uint size, string path); }'
        $value = New-Object System.Text.StringBuilder 256
        [void][Ini]::GetPrivateProfileString('usage', 'five_hour_percentage', '', $value, 256, $env:CODEX_TEST_INI)
        if ($value.ToString() -ne '12.5') { exit 1 }
      `], { env: { ...process.env, CODEX_TEST_INI: path }, encoding: 'utf8', timeout: 15000, windowsHide: true });
      assert.equal(check.status, 0, check.stderr);
    }
    const changed = await collect(root, async () => {
      await writeFile(join(root, 'auth.json'), auth.toString().replace('test-secret', 'other-secret'));
      return summarize(body);
    });
    assert.equal(changed.status, 'Account changed during refresh');
    assert.equal(changed.five_hour_percentage, undefined);
    const failed = await collect(root, async () => { throw new Error('test-secret'); });
    assert.equal(failed.five_hour_percentage, undefined);
    assert.ok(!JSON.stringify(failed).includes('test-secret'));
    // Exercise the real launcher with an OAuth fixture, without making network requests.
    await writeFile(join(root, 'auth.json'), '{"auth_mode":"chatgpt"}');
    const launched = spawnSync('powershell.exe', ['-NoProfile', '-ExecutionPolicy', 'Bypass', '-File',
      resolve(process.env.CODEX_TEST_HELPER_SCRIPT || 'scripts/codex-api-helper.ps1')], {
      env: { ...process.env, CODEX_HOME: root, LOCALAPPDATA: root }, encoding: 'utf8', timeout: 30000, windowsHide: true,
    });
    if (process.platform === 'win32') {
      assert.equal(launched.status, 0, launched.stderr);
      const output = await readFile(join(root, 'trafficmonitor-claude-usage-plugin', 'codex-api-usage.ini'), 'utf16le');
      assert.ok(output.includes('No file-based Codex API key'));
    }
  } finally {
    await rm(root, { recursive: true, force: true });
  }
});

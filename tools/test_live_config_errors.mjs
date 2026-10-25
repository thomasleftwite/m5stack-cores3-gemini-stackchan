// Offline regression: never read the real config, connect, or write test reports.
import { spawnSync } from 'node:child_process';
import { fileURLToPath } from 'node:url';

const root = fileURLToPath(new URL('../', import.meta.url));
const cases = {
  malformed: 'return \'{"gemini_api_key": TEST_SECRET_SENTINEL}\';',
  missing: 'throw Error("TEST_SECRET_SENTINEL");',
  missing_key: 'return "{}";',
  wrong_type: 'return \'{"gemini_api_key":{"secret":"TEST_SECRET_SENTINEL"}}\';',
};
for (const [name, action] of Object.entries(cases)) {
  const shim = `
    import fs from 'node:fs';
    import tls from 'node:tls';
    const read = fs.readFileSync;
    fs.readFileSync = function(p, ...args) {
      if (String(p).replaceAll('\\\\', '/').endsWith('/firmware/data/config.json')) { ${action} }
      return read.call(this, p, ...args);
    };
    const forbidden = () => { process.stderr.write('FORBIDDEN_EFFECT'); throw Error('ForbiddenEffect'); };
    fs.mkdirSync = forbidden; fs.writeFileSync = forbidden; tls.connect = forbidden;
  `;
  const result = spawnSync(process.execPath, [
    '--import=data:text/javascript;base64,' + Buffer.from(shim).toString('base64'),
    'tools/validate_gemini_live.mjs', 'text',
  ], { cwd: root, encoding: 'utf8', timeout: 5000 });
  // Do not echo child stdout/stderr on failure: they could be the regression.
  if (result.error || result.status !== 1 || result.stderr || result.stdout.includes('TEST_SECRET_SENTINEL')) {
    throw Error('Unsafe config error regression: ' + name);
  }
  let output;
  try { output = JSON.parse(result.stdout); } catch { throw Error('Invalid sanitized error: ' + name); }
  if (JSON.stringify(output) !== JSON.stringify({ ok: false, error_kind: 'LocalSetupError' })) {
    throw Error('Unexpected sanitized error: ' + name);
  }
  console.log('PASS: ' + name + '; fixed error, no secret fragment or external effect');
}

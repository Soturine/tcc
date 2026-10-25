// Checks release metadata consistency; never creates tags or changes versions.
import { readFileSync } from 'node:fs';
import { resolve, dirname } from 'node:path';
import { fileURLToPath } from 'node:url';

const projectRoot = resolve(dirname(fileURLToPath(import.meta.url)), '..');
const files = ['package.json', 'backend/package.json', 'frontend/package.json', 'backend/package-lock.json', 'frontend/package-lock.json'];
const readJson = file => JSON.parse(readFileSync(resolve(projectRoot, file), 'utf8'));
const root = readJson('package.json');
const expected = root.version;
const semver = /^(0|[1-9]\d*)\.(0|[1-9]\d*)\.(0|[1-9]\d*)(?:-(?:dev|rc)\.(0|[1-9]\d*))?$/;
const failures = [];
if (!semver.test(expected)) failures.push('Invalid project version: ' + expected);
for (const file of files) {
  const value = readJson(file);
  if (value.version !== expected) failures.push(file + ': version ' + value.version + ' != ' + expected);
  if (file.endsWith('package-lock.json') && value.packages?.['']?.version !== expected)
    failures.push(file + ': packages[""].version != ' + expected);
}
const changelog = readFileSync(resolve(projectRoot, 'CHANGELOG.md'), 'utf8');
if (!changelog.includes('## [Unreleased]')) failures.push('CHANGELOG.md has no Unreleased section');
if (!changelog.includes(expected)) failures.push('CHANGELOG.md does not mention ' + expected);
const roadmap = readFileSync(resolve(projectRoot, 'docs/roadmap/version-milestones.md'), 'utf8');
if (!roadmap.includes(expected)) failures.push('Roadmap does not mention ' + expected);
if (failures.length) {
  for (const message of failures) console.error('[version-check] ' + message);
  process.exitCode = 1;
} else {
  console.log('[version-check] OK ' + expected + ': ' + files.length + ' manifests/locks, changelog and roadmap consistent');
}

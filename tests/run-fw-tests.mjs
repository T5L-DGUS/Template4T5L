import { mkdtempSync, rmSync } from 'node:fs';
import { tmpdir } from 'node:os';
import { dirname, join, resolve } from 'node:path';
import { fileURLToPath } from 'node:url';
import { spawnSync } from 'node:child_process';

// CLANG may select a local Clang installation; no compiler is downloaded.
const root = resolve(dirname(fileURLToPath(import.meta.url)), '..');
const compiler = process.env.CLANG || 'clang';
const sibling = name => /[/\\]/.test(compiler)
  ? join(dirname(compiler), name + (process.platform === 'win32' ? '.exe' : '')) : name;
const linker = process.env.LLVM_LINK || sibling('llvm-link');
const interpreter = process.env.LLI || sibling('lli');
const temporary = mkdtempSync(join(tmpdir(), 'fw-protocol-test-'));
function run(command, args) {
  const result = spawnSync(command, args, { cwd: root, encoding: 'utf8' });
  if (result.error) throw result.error;
  if (result.status !== 0) {
    throw new Error(`${command} failed (exit ${result.status}):\n${result.stdout}${result.stderr}`);
  }
  if(result.stdout) process.stdout.write(result.stdout);
}
try {
  const objects = ['tests/fw_protocol_test.c', 'modules/fw_protocol.c'].map((source, index) => {
    const output = join(temporary, `module-${index}.bc`);
    run(compiler, [
      '-std=c99', '-O0', '-Wall', '-Wextra', '-Werror', '-ffreestanding', '-fno-builtin',
      '-DFW_PROTOCOL_TEST', '-Itests', '-emit-llvm', '-c', source, '-o', output,
    ]);
    return output;
  });
  const output = join(temporary, 'fw-tests.bc');
  run(linker, [...objects, '-o', output]);
  // LLVM's interpreter also works when the installed compiler only targets an MCU.
  run(interpreter, ['--force-interpreter=true', output]);
  const video = join(temporary, 'video-tests.bc');
  run(compiler, ['-std=c99','-O0','-Wall','-Wextra','-Werror','-emit-llvm','-c','tests/r11_video_test.c','-o',video]);
  run(interpreter, ['--force-interpreter=true',video]);
  console.log('FW protocol: production C compiled with warnings treated as errors.');
} finally {
  rmSync(temporary, { recursive: true, force: true });
}

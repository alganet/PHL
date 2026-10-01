--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
-l names the file the way it was typed; a run names the resolved path
--DESCRIPTION--
Lint mode hands the compiler the file under the name it was given -- it never
runs the unit, so nothing needs the canonical name -- where a run expands the
main script's path first. So a `/./` segment in the argument survives into a
lint diagnostic and is collapsed out of a run's.
--SKIPIF--
<?php if (PHP_OS == 'WINNT') { echo "skip POSIX-only: '/./' path spelling and shell redirection"; } ?>
--FILE--
<?php
$php  = getenv('PHPT_TARGET_EXECUTABLE');
$dir  = sys_get_temp_dir();
$base = 'phl_typed_' . getmypid() . '.php';
$path = $dir . '/./' . $base;
file_put_contents($dir . '/' . $base, "<?php\n\$x = ;\n");

function run($cmd) {
    $fp = popen($cmd . ' 2>&1', 'r');
    $o = ''; while (!feof($fp)) { $o .= fgets($fp); } pclose($fp);
    return $o;
}
$q = escapeshellarg($php) . ' ';
$a = escapeshellarg($path);

$lint = run($q . '-l ' . $a);
echo strpos($lint, $path) !== false ? "lint: as typed\n" : "lint: resolved\n";
echo strpos($lint, 'Errors parsing ' . $path) !== false ? "summary: as typed\n" : "summary: resolved\n";

$run = run($q . $a);
echo strpos($run, $path) !== false ? "run: as typed\n" : "run: resolved\n";
echo strpos($run, $dir . '/' . $base) !== false ? "run: names the collapsed path\n" : "run: does not\n";
?>
--EXPECT--
lint: as typed
summary: as typed
run: resolved
run: names the collapsed path
--CLEAN--
<?php
@unlink(sys_get_temp_dir() . '/phl_typed_' . getmypid() . '.php');

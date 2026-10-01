--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
php.ini file: which file -c finds, and the name everything then quotes it by
--SKIPIF--
<?php
// Spawns the interpreter through popen() with single-quoted `-r '...'` arguments —
// POSIX shell quoting that cmd.exe does not honour (php fails this on Windows too).
// symlink() is also not a POSIX symlink there.
if (DIRECTORY_SEPARATOR === '\\') { echo 'skip POSIX shell quoting in the subprocess harness'; }
?>
--FILE--
<?php
$phl = getenv('PHPT_TARGET_EXECUTABLE');
$base = realpath(sys_get_temp_dir()) . '/phlinires' . getmypid();
@mkdir($base);
@mkdir("$base/dir");
@mkdir("$base/empty");
// var_export() rather than var_dump(): the resolved path carries the pid, so its
// LENGTH is not stable across runs.
// A file the ini grammar refuses, so every case has a diagnostic to name it in.
file_put_contents("$base/dir/bad.ini", "precision=3\n[unclosed\n");
@symlink("$base/dir/bad.ini", "$base/link.ini");
function ini_run($phl, $arg, $base) {
    $fp = popen("\"$phl\" -c \"$arg\" -r 'echo ini_get(\"precision\"), \" \", var_export(php_ini_loaded_file(), true), \"\\n\";' 2>&1", 'r');
    $out = '';
    while (!feof($fp)) { $out .= fgets($fp); }
    pclose($fp);
    return str_replace($base, 'BASE', trim($out));
}
// The argument is expanded before anything quotes it: '.', '..' and a symlink
// are all resolved, so a refusal inside the file and php_ini_loaded_file() name
// the one canonical path rather than whatever was typed.
$files = array(
    "$base/dir/bad.ini",
    "$base/dir/../dir/bad.ini",
    "$base/./dir/bad.ini",
    "$base/link.ini",
);
foreach ($files as $arg) {
    echo str_replace($base, 'BASE', $arg), "\n  ", ini_run($phl, $arg, $base), "\n";
}
// Anything that is not a readable file falls through to the ini search path,
// which under -c is the argument itself: php-cli.ini first, then php.ini.
// A directory is never opened as a file, though opening one SUCCEEDS on POSIX.
echo "-- a directory holding php.ini\n";
file_put_contents("$base/dir/php.ini", "precision=7\n");
echo "  ", ini_run($phl, "$base/dir", $base), "\n";
echo "-- php-cli.ini wins over php.ini\n";
file_put_contents("$base/dir/php-cli.ini", "precision=11\n");
echo "  ", ini_run($phl, "$base/dir", $base), "\n";
echo "-- a trailing separator names the same directory\n";
echo "  ", ini_run($phl, "$base/dir/", $base), "\n";
// Finding nothing is not an error: php starts on its built-in defaults and says
// nothing at all, on either stream.
echo "-- nothing to find\n";
foreach (array("$base/nope.ini", "$base/empty", "$base/nodir/x.ini") as $arg) {
    echo "  ", ini_run($phl, $arg, $base), "\n";
}
foreach (array("$base/dir/bad.ini", "$base/dir/php.ini", "$base/dir/php-cli.ini", "$base/link.ini") as $f) { @unlink($f); }
@rmdir("$base/dir"); @rmdir("$base/empty"); @rmdir($base);
?>
--EXPECT--
BASE/dir/bad.ini
  PHP:  syntax error, unexpected end of file, expecting ']' in BASE/dir/bad.ini on line 2
3 'BASE/dir/bad.ini'
BASE/dir/../dir/bad.ini
  PHP:  syntax error, unexpected end of file, expecting ']' in BASE/dir/bad.ini on line 2
3 'BASE/dir/bad.ini'
BASE/./dir/bad.ini
  PHP:  syntax error, unexpected end of file, expecting ']' in BASE/dir/bad.ini on line 2
3 'BASE/dir/bad.ini'
BASE/link.ini
  PHP:  syntax error, unexpected end of file, expecting ']' in BASE/dir/bad.ini on line 2
3 'BASE/dir/bad.ini'
-- a directory holding php.ini
  7 'BASE/dir/php.ini'
-- php-cli.ini wins over php.ini
  11 'BASE/dir/php-cli.ini'
-- a trailing separator names the same directory
  11 'BASE/dir/php-cli.ini'
-- nothing to find
  14 false
  14 false
  14 false
--CLEAN--
<?php
unset($phl, $base, $files, $arg, $f);

--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
scandir()'s errno line carries the open's own reason: a file, and an unknown scheme's fallback
--DESCRIPTION--
The second line is the errno the open left, so a name that is a file reads
'(errno 20): Not a directory', and an unregistered scheme -- which php hands to
the plain-files wrapper after warning about the wrapper -- reads the fallback
path's ENOENT under three warnings.
--SKIPIF--
<?php
if (DIRECTORY_SEPARATOR === '\\') {
    die("skip a directory named 'zzz:' cannot exist on Windows");
}
?>
--FILE--
<?php
set_error_handler(function ($no, $msg) {
    if (!(error_reporting() & $no)) { return false; }
    echo "W: $msg\n"; return true;
});
$cwd = getcwd();
$dir = sys_get_temp_dir() . '/phl_scandir_reasons_' . getmypid();
@mkdir($dir);
file_put_contents($dir . '/file.txt', "x\n");
chdir($dir);
echo "--- not a directory\n";
var_dump(scandir('file.txt'));
echo "--- unknown scheme, fallback path missing\n";
var_dump(scandir('zzz://nope'));
chdir($cwd);
unlink($dir . '/file.txt');
rmdir($dir);
echo "done\n";
--EXPECT--
--- not a directory
W: scandir(file.txt): Failed to open directory: Not a directory
W: scandir(): (errno 20): Not a directory
bool(false)
--- unknown scheme, fallback path missing
W: scandir(): Unable to find the wrapper "zzz" - did you forget to enable it when you configured PHP?
W: scandir(zzz://nope): Failed to open directory: No such file or directory
W: scandir(): (errno 2): No such file or directory
bool(false)
done

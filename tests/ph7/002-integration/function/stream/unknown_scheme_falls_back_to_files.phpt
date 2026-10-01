--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
A scheme no wrapper answers to is named once and then handed WHOLE to the plain-files wrapper
--SKIPIF--
<?php
// The fallback resolves `zzz://hit.php` to the plain-file name `zzz:/hit.php`,
// and a colon is not a legal character in a Windows filename -- so the
// directory this builds cannot exist there. The lookup's behaviour is the same
// on both; only the name it lands on is unwritable.
if (PHP_OS_FAMILY === 'Windows') {
    echo "skip a path component named `zzz:` cannot exist on Windows";
}
?>
--FILE--
<?php
set_error_handler(function ($n, $m) { echo "W: $m\n"; return true; });
$base = sys_get_temp_dir() . '/phl_unknown_scheme_' . getmypid();
@mkdir($base . '/zzz:', 0777, true);
chdir($base);
file_put_contents('zzz:/hit.php', "HELLO\n");

var_dump(file_get_contents('zzz://hit.php'));
var_dump(file_exists('zzz://hit.php'));
var_dump(is_file('zzz://hit.php'));
var_dump(is_dir('zzz://'));
var_dump(filesize('zzz://hit.php'));
var_dump(is_readable('zzz://hit.php'));
var_dump(filetype('zzz://hit.php'));
var_dump(scandir('zzz://'));
var_dump(file('zzz://hit.php'));
var_dump(md5_file('zzz://hit.php'));
$fp = fopen('zzz://hit.php', 'r');
var_dump(fgets($fp));
fclose($fp);
$d = opendir('zzz://');
var_dump(is_resource($d));
closedir($d);
var_dump(file_put_contents('zzz://out.php', 'x'));
var_dump(mkdir('zzz://d1'));
var_dump(rmdir('zzz://d1'));
var_dump(touch('zzz://t1'));
var_dump(unlink('zzz://t1'));
var_dump(unlink('zzz://nope.php'));
var_dump(rename('zzz://out.php', 'zzz://ren.php'));
var_dump(file_get_contents('zzz://ren.php'));

echo "== a wrapper that ANSWERS for the scheme and declines is not the fallback\n";
var_dump(file_get_contents('file://example.com/nope'));
var_dump(unlink('file://example.com/nope'));
var_dump(rename('file://example.com/a', 'file://example.com/b'));
var_dump(mkdir('file://example.com/nope'));
var_dump(rmdir('file://example.com/nope'));
var_dump(chmod('file://example.com/nope', 0644));

@unlink($base . '/zzz:/ren.php');
@unlink($base . '/zzz:/hit.php');
@rmdir($base . '/zzz:');
@rmdir($base);
?>
--EXPECT--
W: file_get_contents(): Unable to find the wrapper "zzz" - did you forget to enable it when you configured PHP?
string(6) "HELLO
"
W: file_exists(): Unable to find the wrapper "zzz" - did you forget to enable it when you configured PHP?
bool(true)
W: is_file(): Unable to find the wrapper "zzz" - did you forget to enable it when you configured PHP?
bool(true)
W: is_dir(): Unable to find the wrapper "zzz" - did you forget to enable it when you configured PHP?
bool(true)
W: filesize(): Unable to find the wrapper "zzz" - did you forget to enable it when you configured PHP?
int(6)
W: is_readable(): Unable to find the wrapper "zzz" - did you forget to enable it when you configured PHP?
bool(true)
W: filetype(): Unable to find the wrapper "zzz" - did you forget to enable it when you configured PHP?
string(4) "file"
W: scandir(): Unable to find the wrapper "zzz" - did you forget to enable it when you configured PHP?
array(3) {
  [0]=>
  string(1) "."
  [1]=>
  string(2) ".."
  [2]=>
  string(7) "hit.php"
}
W: file(): Unable to find the wrapper "zzz" - did you forget to enable it when you configured PHP?
array(1) {
  [0]=>
  string(6) "HELLO
"
}
W: md5_file(): Unable to find the wrapper "zzz" - did you forget to enable it when you configured PHP?
string(32) "0084467710d2fc9d8a306e14efbe6d0f"
W: fopen(): Unable to find the wrapper "zzz" - did you forget to enable it when you configured PHP?
string(6) "HELLO
"
W: opendir(): Unable to find the wrapper "zzz" - did you forget to enable it when you configured PHP?
bool(true)
W: file_put_contents(): Unable to find the wrapper "zzz" - did you forget to enable it when you configured PHP?
int(1)
W: mkdir(): Unable to find the wrapper "zzz" - did you forget to enable it when you configured PHP?
bool(true)
W: rmdir(): Unable to find the wrapper "zzz" - did you forget to enable it when you configured PHP?
bool(true)
W: touch(): Unable to find the wrapper "zzz" - did you forget to enable it when you configured PHP?
bool(true)
W: unlink(): Unable to find the wrapper "zzz" - did you forget to enable it when you configured PHP?
bool(true)
W: unlink(): Unable to find the wrapper "zzz" - did you forget to enable it when you configured PHP?
W: unlink(zzz://nope.php): No such file or directory
bool(false)
W: rename(): Unable to find the wrapper "zzz" - did you forget to enable it when you configured PHP?
W: rename(): Unable to find the wrapper "zzz" - did you forget to enable it when you configured PHP?
bool(true)
W: file_get_contents(): Unable to find the wrapper "zzz" - did you forget to enable it when you configured PHP?
string(1) "x"
== a wrapper that ANSWERS for the scheme and declines is not the fallback
W: file_get_contents(): Remote host file access not supported, file://example.com/nope
W: file_get_contents(file://example.com/nope): Failed to open stream: no suitable wrapper could be found
bool(false)
W: unlink(): Unable to locate stream wrapper
bool(false)
W: rename(): Unable to locate stream wrapper
bool(false)
bool(false)
bool(false)
W: chmod(): Cannot call chmod() for a non-standard stream
bool(false)

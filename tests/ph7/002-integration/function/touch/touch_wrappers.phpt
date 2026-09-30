--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
touch() over a WRAPPER, which is three different questions
--DESCRIPTION--
php's touch() asks a userland wrapper through stream_metadata() and says so when
the class has no such method. A BUILT-IN wrapper has no method to call, so php
falls back to opening the url -- which is why `data://` and `php://memory`
answer true for a name that is no file at all, and why a `compress.zlib://` one
refuses AFTER creating the file underneath: the wrapper opens it and only then
discovers it has no direction to compress through.

This engine handed every one of those to the plain-file driver, which stamped
whatever the literal string named -- so a wrapper url was reported as a file it
could not create.
--FILE--
<?php
error_reporting(E_ALL);
$dir = sys_get_temp_dir() . '/phlt-touchw-' . getmypid();
@mkdir($dir);
set_error_handler(function ($n, $s) use ($dir) {
    if (!(error_reporting() & $n)) { return true; }
    echo '  [', $n, '] ', str_replace([$dir, strtr($dir, '/', '\\')], '<dir>', $s), "\n";
    return true;
});
class Meta {
    public $context;
    public static $seen = [];
    function stream_metadata($path, $option, $value) {
        self::$seen[] = [$path, $option, $value];
        return str_contains($path, 'yes');
    }
    function url_stat($p, $f) { return false; }
}
class NoMeta {
    public $context;
    function stream_open($p, $m, $o, &$op) { return true; }
    function stream_read($n) { return ''; }
    function stream_eof() { return true; }
    function stream_stat() { return []; }
    function stream_close() { }
    function url_stat($p, $f) { return false; }
}
stream_wrapper_register('meta', 'Meta');
stream_wrapper_register('nometa', 'NoMeta');

echo "-- a userland wrapper is ASKED, through stream_metadata\n";
var_dump(touch('meta://yes'), touch('meta://no'));
var_dump(touch('meta://yes', 100, 200));
print_r(Meta::$seen);
echo "-- ...and one without the method is not: php takes it as a FILE name\n";
var_dump(touch('nometa://x'));
echo "-- a built-in wrapper answers by whether the url OPENS\n";
var_dump(touch('data://text/plain,x'), touch('php://memory'), touch('php://temp'));
echo "-- ...which is why a compressed one refuses, having created the file first\n";
$gz = $dir . '/t.gz';
var_dump(touch('compress.zlib://' . $gz), file_exists($gz));
echo "-- a scheme nothing is registered under is named, then tried as a file\n";
var_dump(touch('zzz://x'));
echo "-- and a plain path is still a plain path\n";
$f = $dir . '/plain.txt';
var_dump(touch($f), file_exists($f), filesize($f));

foreach (glob($dir . '/*') as $x) { @unlink($x); }
@rmdir($dir);
--EXPECT--
-- a userland wrapper is ASKED, through stream_metadata
bool(true)
bool(false)
bool(true)
Array
(
    [0] => Array
        (
            [0] => meta://yes
            [1] => 1
            [2] => Array
                (
                )

        )

    [1] => Array
        (
            [0] => meta://no
            [1] => 1
            [2] => Array
                (
                )

        )

    [2] => Array
        (
            [0] => meta://yes
            [1] => 1
            [2] => Array
                (
                    [0] => 100
                    [1] => 200
                )

        )

)
-- ...and one without the method is not: php takes it as a FILE name
  [2] touch(): NoMeta::stream_metadata is not implemented!
bool(false)
-- a built-in wrapper answers by whether the url OPENS
bool(true)
bool(true)
bool(true)
-- ...which is why a compressed one refuses, having created the file first
  [2] touch(compress.zlib://<dir>/t.gz): Failed to open stream: operation failed
bool(false)
bool(true)
-- a scheme nothing is registered under is named, then tried as a file
  [2] touch(): Unable to find the wrapper "zzz" - did you forget to enable it when you configured PHP?
  [2] touch(): Unable to create file zzz://x because No such file or directory
bool(false)
-- and a plain path is still a plain path
bool(true)
bool(true)
int(0)

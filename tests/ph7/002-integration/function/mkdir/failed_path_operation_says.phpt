--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
What a failed path OPERATION says: the four names that said nothing
--DESCRIPTION--
php's non-open IO failures warn, and this engine's answer for most of them --
unlink, rmdir, mkdir, rename, readlink -- has said so since the 39th session.
Four names were left answering FALSE in SILENCE, so a script could not tell a
failed operation from a completed one without testing a return value nobody
told it to test: `chmod()`, `touch()`, `link()` and `symlink()`.

php words them in two shapes and this pins both: `chmod()`, `link()` and
`symlink()` name NO path and carry the C library's reason, while `touch()` has
a sentence of its own that names the file AND the reason. The successes beside
them stay silent, and a dangling symlink is a SUCCESS -- the target is not
required to exist. (A symlink written OVER a dangling one is left out: php
answers true there and creates nothing, which is php's own quirk and not a
divergence either engine can be pinned to.)
--SKIPIF--
<?php
if (stripos(PHP_OS, 'WIN') === 0) {
    echo "skip POSIX link semantics";
}
?>
--FILE--
<?php
$dir = sys_get_temp_dir() . '/phlfpo' . getmypid();
@mkdir($dir);
$gone = $dir . '/nope';

function why($label, callable $fn)
{
    global $dir;
    $seen = array();
    set_error_handler(function ($no, $str) use (&$seen) { $seen[] = $str; return true; });
    $r = $fn();
    restore_error_handler();
    printf("%-22s -> %-5s %s\n", $label, var_export($r, true),
        json_encode(str_replace($dir, 'DIR', $seen)));
}

why('chmod missing', fn() => chmod($gone, 0644));
why('touch missing', fn() => touch($dir . '/sub/deep/f'));
why('touch ok', fn() => touch($dir . '/t1'));
why('chmod ok', fn() => chmod($dir . '/t1', 0644));
why('link missing', fn() => link($gone, $dir . '/h1'));
why('link ok', fn() => link($dir . '/t1', $dir . '/h2'));
why('link over one', fn() => link($dir . '/t1', $dir . '/h2'));
why('symlink dangling', fn() => symlink($gone, $dir . '/l1'));
why('symlink over a file', fn() => symlink($dir . '/t1', $dir . '/t1'));

foreach (glob($dir . '/*') as $f) {
    @unlink($f);
}
@rmdir($dir);
?>
--EXPECT--
chmod missing          -> false ["chmod(): No such file or directory"]
touch missing          -> false ["touch(): Unable to create file DIR\/sub\/deep\/f because No such file or directory"]
touch ok               -> true  []
chmod ok               -> true  []
link missing           -> false ["link(): No such file or directory"]
link ok                -> true  []
link over one          -> false ["link(): File exists"]
symlink dangling       -> true  []
symlink over a file    -> false ["symlink(): File exists"]

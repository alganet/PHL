--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
PH7 / PHP: stream_resolve_include_path(), and the SCRIPT-directory fallback under it
--INI--
include_path=.
--FILE--
<?php
$ri_base = sys_get_temp_dir() . '/phl_ri_' . getmypid();
@mkdir($ri_base);
@mkdir($ri_base . '/a');
@mkdir($ri_base . '/b');
file_put_contents($ri_base . '/a/phl_ri_one.php', "<?php return 'A1';\n");
file_put_contents($ri_base . '/b/phl_ri_two.php', "<?php return 'B2';\n");
/* boot.php is INCLUDED from an absolute path and then includes its own
 * neighbour by a bare name — the case the include_path cannot answer. */
file_put_contents($ri_base . '/phl_ri_boot.php',
    "<?php\n"
    . "var_dump(stream_resolve_include_path('phl_ri_side.php') !== false);\n"
    . "var_dump(include 'phl_ri_side.php');\n"
    . "var_dump(is_resource(fopen('phl_ri_side.php', 'r', true)));\n"
    . "var_dump(file_get_contents('phl_ri_side.php', true) !== false);\n"
    . "var_dump(@include 'phl_ri_side.php/nope.php');\n");
file_put_contents($ri_base . '/phl_ri_side.php', "<?php return 'SIDE';\n");

$ri_real = realpath($ri_base);
$ri_show = function ($v) use ($ri_real) {
    return is_string($v)
        ? str_replace([$ri_real, DIRECTORY_SEPARATOR], ['<B>', '/'], $v)
        : var_export($v, true);
};

chdir(DIRECTORY_SEPARATOR);
set_include_path($ri_base . '/a' . PATH_SEPARATOR . $ri_base . '/b');

/* The include_path walk, in order, then the two shapes that skip it. */
foreach (['phl_ri_one.php', 'phl_ri_two.php', 'phl_ri_none.php', '',
          './phl_ri_one.php', '../phl_ri_one.php'] as $ri_n) {
    printf("%-22s => %s\n", var_export($ri_n, true), $ri_show(stream_resolve_include_path($ri_n)));
}
/* An absolute name is realpath'd and never walked; a missing one is false. */
var_dump($ri_show(stream_resolve_include_path($ri_base . '/a/phl_ri_one.php')));
var_dump(stream_resolve_include_path($ri_base . '/a/phl_ri_none.php'));
/* A DIRECTORY resolves — php's resolver asks realpath(), not is_file(). */
var_dump($ri_show(stream_resolve_include_path($ri_base . '/a')));
/* A name carrying a scheme is not walked. file:// is the one php resolves. */
var_dump(stream_resolve_include_path('php://memory'));
var_dump(stream_resolve_include_path('data://text/plain,hi'));
var_dump($ri_show(stream_resolve_include_path('file://' . $ri_base . '/a/phl_ri_one.php')));
var_dump(stream_resolve_include_path('file://phl_ri_one.php'));
/* php's Z_PARAM_PATH refuses a NUL outright. */
try { stream_resolve_include_path("phl_ri_one.php\0x"); }
catch (ValueError $e) { echo get_class($e), ': ', $e->getMessage(), "\n"; }

/* The last resort: the directory of the file that is EXECUTING. Nothing below
 * is on the include_path and the cwd is the root. */
echo "=== boot ===\n";
include $ri_base . '/phl_ri_boot.php';

@unlink($ri_base . '/a/phl_ri_one.php');
@unlink($ri_base . '/b/phl_ri_two.php');
@unlink($ri_base . '/phl_ri_boot.php');
@unlink($ri_base . '/phl_ri_side.php');
@rmdir($ri_base . '/a');
@rmdir($ri_base . '/b');
@rmdir($ri_base);
?>
--EXPECT--
'phl_ri_one.php'       => <B>/a/phl_ri_one.php
'phl_ri_two.php'       => <B>/b/phl_ri_two.php
'phl_ri_none.php'      => false
''                     => <B>/a
'./phl_ri_one.php'     => false
'../phl_ri_one.php'    => false
string(20) "<B>/a/phl_ri_one.php"
bool(false)
string(5) "<B>/a"
bool(false)
bool(false)
string(20) "<B>/a/phl_ri_one.php"
bool(false)
ValueError: stream_resolve_include_path(): Argument #1 ($filename) must not contain any null bytes
=== boot ===
bool(true)
string(4) "SIDE"
bool(true)
bool(true)
bool(false)
--CLEAN--
<?php
unset($ri_base, $ri_real, $ri_show, $ri_n);

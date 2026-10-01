--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
A bare relative include falls back to the directory of the RUNNING file, not the top of the include stack
--DESCRIPTION--
php's last resort for a relative name the include_path did not answer is the
directory of the file whose code is executing -- the running op array's own
filename. This engine read the include-nesting stack instead, which is popped as
soon as an include returns, so a function that pulls a sibling in by bare name
looked beside the ENTRY SCRIPT rather than beside the file that declared it.
Every shape that reaches the same resolver is measured here: the two constructs,
a plain function, a closure, a static and an instance method, a function from a
deeper unit, a function compiled by eval(), and the two library doors
(stream_resolve_include_path() and fopen()'s use_include_path).
--FILE--
<?php
$ifdDir = sys_get_temp_dir() . '/phl_incdir_' . getmypid();
@mkdir($ifdDir . '/a/b/c', 0777, true);

file_put_contents($ifdDir . '/a/S_marker.php',     "<?php \$GLOBALS['ifd'][] = 'a';\n");
file_put_contents($ifdDir . '/a/b/S_marker.php',   "<?php \$GLOBALS['ifd'][] = 'b';\n");
file_put_contents($ifdDir . '/a/b/c/S_marker.php', "<?php \$GLOBALS['ifd'][] = 'c';\n");
file_put_contents($ifdDir . '/a/D_marker.txt',   "a\n");
file_put_contents($ifdDir . '/a/b/D_marker.txt', "b\n");

file_put_contents($ifdDir . '/a/b/lib.php', <<<'LIB'
<?php
require 'S_marker.php';
function ifdFn() { require 'S_marker.php'; }
$GLOBALS['ifdClo'] = function () { require 'S_marker.php'; };
class IfdLib {
    public static function stat() { require 'S_marker.php'; }
    public function inst() { require 'S_marker.php'; }
}
function ifdResolve() {
    $p = stream_resolve_include_path('S_marker.php');
    $GLOBALS['ifd'][] = $p === false ? 'resolve:false' : 'resolve:' . basename(dirname($p));
}
function ifdFopen() {
    $h = @fopen('D_marker.txt', 'r', true);
    $GLOBALS['ifd'][] = $h === false ? 'fopen:false' : 'fopen:' . trim(fread($h, 8));
    if ($h !== false) { fclose($h); }
}
function ifdEval() { eval('function ifdEvaled() { require "S_marker.php"; }'); }
require 'c/lib.php';
LIB);

file_put_contents($ifdDir . '/a/b/c/lib.php', "<?php\nfunction ifdDeep() { require 'S_marker.php'; }\n");

file_put_contents($ifdDir . '/a/main.php', <<<'MAIN'
<?php
require 'b/lib.php';
ifdFn();
($GLOBALS['ifdClo'])();
IfdLib::stat();
(new IfdLib)->inst();
ifdDeep();
ifdResolve();
ifdFopen();
ifdEval();
ifdEvaled();
require 'S_marker.php';
MAIN);

$GLOBALS['ifd'] = [];
require $ifdDir . '/a/main.php';
echo implode("\n", $GLOBALS['ifd']), "\n";

foreach (['/a/main.php', '/a/S_marker.php', '/a/D_marker.txt',
          '/a/b/lib.php', '/a/b/S_marker.php', '/a/b/D_marker.txt',
          '/a/b/c/lib.php', '/a/b/c/S_marker.php'] as $ifdF) {
    @unlink($ifdDir . $ifdF);
}
foreach (['/a/b/c', '/a/b', '/a', ''] as $ifdP) {
    @rmdir($ifdDir . $ifdP);
}
?>
--EXPECT--
b
b
b
b
b
c
resolve:b
fopen:b
b
a
--CLEAN--
<?php
unset($ifdDir, $ifdF, $ifdP);
unset($GLOBALS['ifd'], $GLOBALS['ifdClo']);

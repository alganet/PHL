--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
extract: php installs symbol-table entries named after superglobals (zend half of the twin pair)
--SKIPIF--
<?php
if (!function_exists('zend_version')) {
    echo "skip zend-pinned half of the twin pair";
}
?>
--FILE--
<?php
// php's guard is `if (orig_var && name == "GLOBALS") continue;` — it only
// declines to OVERWRITE an existing entry, so the first extract() adds one.
var_dump(extract(['GLOBALS' => 'clobbered']));
var_dump(extract(['GLOBALS' => 'clobbered'], EXTR_SKIP));
var_dump(extract(['GLOBALS' => 'clobbered'], EXTR_IF_EXISTS));
var_dump(gettype($GLOBALS), isset($GLOBALS['GLOBALS']));
var_dump(extract(['GLOBALS' => 'prefixed'], EXTR_PREFIX_ALL, 'g'));
var_dump($g_GLOBALS);
// A superglobal name lands in the FRAME's symbol table, which php's own
// $_SERVER reads never consult — so the real superglobal survives.
function extrProtect(int $mode) {
    $src = ['_SERVER' => 'clobbered'];
    $n = $mode >= EXTR_PREFIX_SAME && $mode <= EXTR_PREFIX_IF_EXISTS
        ? extract($src, $mode, 'p')
        : extract($src, $mode);
    $v = get_defined_vars();
    unset($v['mode'], $v['src'], $v['n']);
    return "$mode:n=$n:" . implode(',', array_keys($v));
}
foreach ([0, 1, 2, 3, 4, 5, 6] as $mode) {
    echo extrProtect($mode), "\n";
}
var_dump(gettype($_SERVER), isset($_SERVER['argv']));
?>
--EXPECT--
int(1)
int(0)
int(0)
string(5) "array"
bool(true)
int(1)
string(8) "prefixed"
0:n=1:_SERVER
1:n=1:_SERVER
2:n=1:_SERVER
3:n=1:p__SERVER
4:n=1:_SERVER
5:n=0:
6:n=0:
string(5) "array"
bool(true)
--CLEAN--
<?php

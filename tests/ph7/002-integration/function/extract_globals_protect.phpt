--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
extract: PHL never installs a variable named after a superglobal (a recorded divergence)
--SKIPIF--
<?php
if (function_exists('zend_version')) {
    echo "skip PHL-pinned half of the twin pair";
}
?>
--FILE--
<?php
// php only protects $GLOBALS when an entry of that name already exists in the
// symbol table, so `extract(['GLOBALS' => ...])` happily creates one (php's
// $GLOBALS reads are compiled specially, so nothing visibly breaks there).
// In PHL $GLOBALS IS the global symbol table's own slot, so a store would
// destroy it: the key is skipped in every mode instead.
var_dump(extract(['GLOBALS' => 'clobbered']));
var_dump(extract(['GLOBALS' => 'clobbered'], EXTR_SKIP));
var_dump(extract(['GLOBALS' => 'clobbered'], EXTR_IF_EXISTS));
var_dump(gettype($GLOBALS), isset($GLOBALS['GLOBALS']));
// The prefixing modes are unaffected: the prefixed name is not $GLOBALS.
var_dump(extract(['GLOBALS' => 'prefixed'], EXTR_PREFIX_ALL, 'g'));
var_dump($g_GLOBALS);
// The same holds for every other superglobal name, in every mode. php lands
// those in the frame's own symbol table (where its $_SERVER reads never look);
// PHL's name would resolve to the superglobal slot itself, so it is dropped.
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
int(0)
int(0)
int(0)
string(5) "array"
bool(false)
int(1)
string(8) "prefixed"
0:n=0:
1:n=0:
2:n=0:
3:n=1:p__SERVER
4:n=0:
5:n=0:
6:n=0:
string(5) "array"
bool(true)
--CLEAN--
<?php

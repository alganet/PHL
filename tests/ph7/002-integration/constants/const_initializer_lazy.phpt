--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
DIVERGENCE: a `const NAME = <expr>;` initializer is evaluated at the first READ, not at the statement — so one that cannot complete yet is retried (php's half is the _zend twin)
--SKIPIF--
<?php
if (function_exists('zend_version')) {
    echo "skip";
}
?>
--FILE--
<?php
// php evaluates a `const` initializer AT THE STATEMENT: the constant is a value
// from that line on, and an initializer that raises is an uncaught fatal right
// there. PHL registers the constant when the file is COMPILED and runs the
// initializer at the first READ, which is why the throw below lands in the
// reader's own try/catch and why a later define() lets the retry succeed.
//
// The value half is not divergent: the initializer is evaluated ONCE and the
// same value is handed out for ever (const_initializer_evaluated_once.phpt).
// What is NOT frozen is an initializer that did not COMPLETE.
const CIL_LATE = CIL_DEFINED_LATER;
try {
    // Assigned rather than printed: a throw raised by the expansion lands the
    // enclosing catch IN PLACE and the abandoned statement then resumes with the
    // NULL the failed expansion left, so a var_dump() here would print one.
    // That resumption is part of the same divergence — php never reaches this
    // state, because it evaluated (and fataled) at the statement above.
    $cil_v = CIL_LATE;
} catch (Throwable $cil_e) {
    echo 'caught: ', $cil_e->getMessage(), "\n";
}
define('CIL_DEFINED_LATER', 5);
var_dump(CIL_LATE, CIL_LATE);

// A throwing constructor is retried the same way, and each attempt throws.
class CilBoom
{
    public static int $tries = 0;
    public function __construct() { self::$tries++; throw new RuntimeException('boom'); }
}
const CIL_OBJ = new CilBoom();
foreach ([1, 2] as $cil_n) {
    try {
        $cil_x = CIL_OBJ;
    } catch (Throwable $cil_e) {
        echo 'caught: ', $cil_e->getMessage(), "\n";
    }
}
var_dump(CilBoom::$tries);

// A never-read constant's initializer never runs at all.
class CilNever { public static bool $ran = false; public function __construct() { self::$ran = true; } }
const CIL_UNREAD = new CilNever();
var_dump(CilNever::$ran);
echo "AFTER\n";
?>
--EXPECT--
caught: Undefined constant "CIL_DEFINED_LATER"
int(5)
int(5)
caught: boom
caught: boom
int(2)
bool(false)
AFTER
--CLEAN--
<?php
unset($cil_e, $cil_n, $cil_x, $cil_v);

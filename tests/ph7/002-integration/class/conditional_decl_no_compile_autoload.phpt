--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
A conditional class declaration does not autoload its parent at compile time
--FILE--
<?php
// php never resolves a parent class it has not REACHED: a declaration inside a
// conditional block binds when the block runs, and zend_try_early_binding does a
// plain class-table lookup rather than an autoload -- because running user code
// in the middle of compiling a file is exactly what this must not do.
spl_autoload_register(function ($c) {
    echo "autoload($c)\n";
    if ($c === 'EblParent') {
        eval('class EblParent { public $v = "P"; }');
    } elseif ($c === 'EblIface') {
        eval('interface EblIface {}');
    }
});
// The dead block: nothing is declared and, crucially, nothing is LOADED.
if (false) {
    class EblDead extends EblParent {}
}
if (false) {
    interface EblDeadIface extends EblIface {}
}
var_dump(class_exists('EblParent', false), interface_exists('EblIface', false));
var_dump(class_exists('EblDead', false));
// The live block still resolves its parent -- when it RUNS.
if (true) {
    class EblLive extends EblParent {}
}
var_dump(class_exists('EblParent', false));
$ebl = new EblLive();
var_dump($ebl->v, $ebl instanceof EblParent);
// A class inside a function body is the same rule.
function ebl_make() {
    class EblInner extends EblParent {}
    return new EblInner();
}
var_dump(class_exists('EblInner', false));
var_dump(ebl_make() instanceof EblParent);
// An anonymous class is an EXPRESSION, so its parent resolves where it is written.
$ebl_anon = new class extends EblParent {};
var_dump($ebl_anon instanceof EblParent);
?>
--EXPECT--
bool(false)
bool(false)
bool(false)
autoload(EblParent)
bool(true)
string(1) "P"
bool(true)
bool(false)
bool(true)
bool(true)

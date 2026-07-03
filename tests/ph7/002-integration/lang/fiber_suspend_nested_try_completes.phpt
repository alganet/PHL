--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
KNOWN DIVERGENCE (BYTECODE.md stage 4): a fiber suspending inside a nested call's try/finally still has PHL's lossy resume semantics — but the script must COMPLETE (regression: the parked handler's freed owner frame was dereferenced and the script silently truncated with exit 0)
--SKIPIF--
<?php
if (function_exists('zend_version')) {
    echo "skip";
}
--FILE--
<?php
function inner() {
    $x = "inner-var";
    try {
        Fiber::suspend("s");
    } finally {
        echo "fin(", isset($x) ? $x : "", ")\n";
    }
    return "inner-done";
}
$f = new Fiber(function () {
    $r = inner();
    echo "inner returned: ", var_export($r, true), "\n";
    try { throw new Exception("after"); } catch (Exception $e) { echo "caught: ", $e->getMessage(), "\n"; }
    return "done";
});
$f->start();
$f->resume("go");
echo "ret=", $f->getReturn(), "\n";
echo "script-end\n";
?>
--EXPECT--
inner returned: 'go'
caught: after
fin()
ret=done
script-end
--CLEAN--
<?php

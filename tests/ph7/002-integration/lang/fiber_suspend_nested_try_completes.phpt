--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Fiber suspended inside a nested call's try/finally resumes in place: the finally sees its own live locals and the callee returns normally (php-exact; BYTECODE.md stage 4)
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
fin(inner-var)
inner returned: 'inner-done'
caught: after
ret=done
script-end
--CLEAN--
<?php

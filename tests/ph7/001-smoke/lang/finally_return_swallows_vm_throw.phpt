--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
A finally `return` swallows a VM-raised throw and the return value survives
--DESCRIPTION--
php: a `return` inside `finally` swallows the in-flight exception and the
function returns that value. The same-frame swallow path in VmThrowException
cleared VM_FRAME_THROW and returned SXRET_OK relying on OP_THROW's
compiler-given landing pad — but a VM-RAISED throw site (undefined function,
non-callable value, malformed array callable, typed-property TypeError…) has
no such pad, so its router unwound as an exception and the Unwind discard
dropped the parked return: `function f(){ try { nosuchfn(); } finally {
return 5; } }` returned null where php returns 5. The swallow path now
records the resume target (the try's OP_POP_EXCEPTION, whose bHasRet tail
materializes the return) exactly like the in-place-catch landing.
--FILE--
<?php
function frs1() { try { frs_nosuchfn(); } finally { return 5; } }
function frs2() { try { throw new Exception("x"); } finally { return 6; } }
function frs3() { try { $q = 1 + frs_nosuchfn(); } finally { return 7; } }
function frs4() { try { [1, 2, 3](); } finally { return 8; } }
function frs5() { $f = 42; try { $f(); } finally { return 9; } }
function frs6() {
    $t = new class { public int $p = 3; };
    try { $t->p = "xx"; } finally { return 10; }
}
function frs7() { try { frs_nosuchfn(); } finally { return 11; } return 99; }
function frs8() {
    try {
        try { frs_nosuchfn(); } finally { return 12; }
    } finally { echo "outerfin "; }
}
function frs9() { try { frs_nosuchfn(); } catch (Error $e) { return 13; } finally { echo "f9 "; } }
echo frs1(), " ", frs2(), " ", frs3(), " ", frs4(), " ", frs5(), " ", frs6(), " ", frs7(), " ", frs8(), " ", frs9(), "\n";
function frs10() { try { frs_nosuchfn(); } finally {} return 99; }
try { echo frs10(), "\n"; } catch (Error $e) { echo "10:c\n"; }
echo "end\n";
?>
--EXPECT--
5 6 7 8 9 10 11 outerfin 12 f9 13
10:c
end
--CLEAN--
<?php

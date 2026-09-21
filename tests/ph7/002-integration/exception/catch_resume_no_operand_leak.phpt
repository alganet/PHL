--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
A caught throw abandoning mid-expression operands does not leak operand-stack slots
--DESCRIPTION--
Every in-place catch resume used to leave the throw's abandoned mid-expression
operands (a pending `1 +`, an array under construction, the call's result
slot) on the operand stack — one leaked slot per caught throw, so any
try/catch in a loop eventually overflowed the operand stack (ASan
heap-buffer-overflow; ~100 iterations at script top level). The resume paths
now drain to the catching try's recorded stack base
(PH7_RESUME_DRAIN / the OP_THROW landing-pad drain). Each shape below runs
far past the old overflow threshold; the test passes by completing with
correct values.
--FILE--
<?php
const N = 50000;

function thrower() { throw new Exception("x"); }
class Ctor { public function __construct() { throw new Exception("x"); } }
class Meth { public function m() { throw new Exception("x"); } }
class TypedP { public int $p = 3; }
class UninitS { public static int $s; }

$o = new Meth;
$t = new TypedP;

for ($i = 0; $i < N; $i++) {
    try { $a = 1 + thrower(); } catch (Exception $e) {}
    try { $a = [1, 2, $o->m()]; } catch (Exception $e) {}
    try { $a = [1, new Ctor]; } catch (Exception $e) {}
    try { $q = 1 + throw new Exception("x"); } catch (Exception $e) {}
    try { throw new Exception("x"); } catch (Exception $e) {}
    try { $q = [5, $t->p = "xx"]; } catch (TypeError $e) {}
    try { $v = UninitS::$s; } catch (Error $e) {}
    try {
        try { throw new Exception("a"); }
        catch (Exception $e) { throw new Exception("b"); }
    } catch (Exception $e) {}
}
var_dump($t->p);
echo "done\n";
?>
--EXPECT--
int(3)
done
--CLEAN--
<?php
unset($o, $t, $a, $q, $v, $i);

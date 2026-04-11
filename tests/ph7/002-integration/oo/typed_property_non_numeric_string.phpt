--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Typed property: non-numeric string rejected for int/float
--FILE--
<?php
class TpnmCell { public int $n = 0; public float $f = 0.0; }
$c = new TpnmCell();
try { $c->n = "abc"; } catch (TypeError $e) { echo "n1: ", $e->getMessage(), "\n"; }
try { $c->n = "43x"; } catch (TypeError $e) { echo "n2: ", $e->getMessage(), "\n"; }
try { $c->f = "abc"; } catch (TypeError $e) { echo "f1: ", $e->getMessage(), "\n"; }
$c->n = "42";
$c->f = "3.14";
echo "ok $c->n $c->f\n";
?>
--EXPECT--
n1: Cannot assign string to property TpnmCell::$n of type int
n2: Cannot assign string to property TpnmCell::$n of type int
f1: Cannot assign string to property TpnmCell::$f of type float
ok 42 3.14
--CLEAN--
<?php
unset($c);

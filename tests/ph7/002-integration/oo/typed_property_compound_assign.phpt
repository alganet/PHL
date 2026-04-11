--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Typed property: compound assignments go through type enforcement
--FILE--
<?php
class TpcBag { public int $n = 0; public array $items = []; }
$b = new TpcBag();
$b->n += 5;
echo "n=$b->n\n";
$b->n *= 3;
echo "n=$b->n\n";
$b->n -= 1;
echo "n=$b->n\n";
try { $b->n .= "abc"; } catch (TypeError $e) { echo "catcat: ", $e->getMessage(), "\n"; }
echo "n=$b->n\n";
?>
--EXPECT--
n=5
n=15
n=14
catcat: Cannot assign string to property TpcBag::$n of type int
n=14
--CLEAN--
<?php
unset($b);

--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Typed property: int rejects array assignment
--FILE--
<?php
class TpiCell { public int $n = 0; }
$c = new TpiCell();
try {
    $c->n = [1, 2, 3];
} catch (TypeError $e) {
    echo "caught: ", $e->getMessage(), "\n";
}
echo $c->n, "\n";
?>
--EXPECT--
caught: Cannot assign array to property TpiCell::$n of type int
0
--CLEAN--
<?php
unset($c);

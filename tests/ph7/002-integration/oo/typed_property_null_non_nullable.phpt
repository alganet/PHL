--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Typed property: null into non-nullable throws TypeError
--FILE--
<?php
class TpiBox { public int $n = 0; }
$b = new TpiBox();
try {
    $b->n = null;
} catch (TypeError $e) {
    echo "caught: ", $e->getMessage(), "\n";
}
echo $b->n, "\n";
?>
--EXPECT--
caught: Cannot assign null to property TpiBox::$n of type int
0
--CLEAN--
<?php
unset($b);

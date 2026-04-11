--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Typed property: int|string union rejects array assignment
--FILE--
<?php
class TpurHolder { public int|string $x = 0; }
$h = new TpurHolder();
try {
    $h->x = [1, 2];
} catch (TypeError $e) {
    echo $e->getMessage(), "\n";
}
?>
--EXPECT--
Cannot assign array to property TpurHolder::$x of type string|int
--CLEAN--
<?php
unset($h);

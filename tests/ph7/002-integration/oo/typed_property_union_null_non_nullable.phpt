--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Typed property: int|string union without null rejects null assignment
--FILE--
<?php
class TpunnHolder { public int|string $x = 0; }
$h = new TpunnHolder();
try {
    $h->x = null;
} catch (TypeError $e) {
    echo $e->getMessage(), "\n";
}
?>
--EXPECT--
Cannot assign null to property TpunnHolder::$x of type string|int
--CLEAN--
<?php
unset($h);

--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Typed property: array rejects scalar assignment
--FILE--
<?php
class TpiList { public array $items = []; }
$l = new TpiList();
try {
    $l->items = "not an array";
} catch (TypeError $e) {
    echo "caught: ", $e->getMessage(), "\n";
}
echo count($l->items), "\n";
?>
--EXPECT--
caught: Cannot assign string to property TpiList::$items of type array
0
--CLEAN--
<?php
unset($l);

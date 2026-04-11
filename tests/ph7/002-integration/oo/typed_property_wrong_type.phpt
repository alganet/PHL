--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Typed property: assigning object to int throws TypeError
--FILE--
<?php
class TpiHolder { public int $n = 0; }
class TpiThing {}
$h = new TpiHolder();
try {
    $h->n = new TpiThing();
} catch (TypeError $e) {
    echo "caught: ", $e->getMessage(), "\n";
}
echo $h->n, "\n";
?>
--EXPECT--
caught: Cannot assign TpiThing to property TpiHolder::$n of type int
0
--CLEAN--
<?php
unset($h);

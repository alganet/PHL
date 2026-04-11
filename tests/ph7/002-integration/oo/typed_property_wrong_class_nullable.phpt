--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Typed property: nullable class accepts null but rejects wrong class
--FILE--
<?php
class TpiA {}
class TpiC {}
class TpiHost { public ?TpiA $a = null; }
$h = new TpiHost();
echo is_null($h->a) ? "null" : "set", "\n";
$h->a = new TpiA();
echo get_class($h->a), "\n";
$h->a = null;
echo is_null($h->a) ? "null" : "set", "\n";
try {
    $h->a = new TpiC();
} catch (TypeError $e) {
    echo "caught: ", $e->getMessage(), "\n";
}
?>
--EXPECT--
null
TpiA
null
caught: Cannot assign TpiC to property TpiHost::$a of type ?TpiA
--CLEAN--
<?php
unset($h);

--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Typed property: assigning unrelated class throws TypeError
--FILE--
<?php
class TpiAlpha {}
class TpiBeta {}
class TpiSlot { public TpiAlpha $a; }
$s = new TpiSlot();
try {
    $s->a = new TpiBeta();
} catch (TypeError $e) {
    echo "caught: ", $e->getMessage(), "\n";
}
$s->a = new TpiAlpha();
echo "ok\n";
?>
--EXPECT--
caught: Cannot assign TpiBeta to property TpiSlot::$a of type TpiAlpha
ok
--CLEAN--
<?php
unset($s);

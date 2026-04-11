--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Union return type: : Foo|Bar with class instances
--FILE--
<?php
class UrFoo { public $tag = "F"; }
class UrBar { public $tag = "B"; }
function pick(int $i): UrFoo|UrBar {
    return $i > 0 ? new UrFoo() : new UrBar();
}
echo pick(1)->tag, "\n";
echo pick(0)->tag, "\n";
?>
--EXPECT--
F
B
--CLEAN--
<?php

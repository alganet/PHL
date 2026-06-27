--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
An object with __invoke is callable but is NOT an instanceof Closure
--FILE--
<?php
class CincAdder { public function __invoke($x){ return $x + 100; } }
$o = new CincAdder();
echo var_export($o instanceof Closure, true), "\n";
echo var_export(is_callable($o), true), "\n";
echo $o(1), "\n";
echo call_user_func($o, 2), "\n";
?>
--EXPECT--
false
true
101
102
--CLEAN--
<?php

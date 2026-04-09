--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Trailing comma in method and static method calls
--FILE--
<?php
class TcMcCalc {
    public function sum($a, $b) { return $a + $b; }
    public static function mul($a, $b) { return $a * $b; }
}
$c = new TcMcCalc();
echo $c->sum(3, 4,) . "\n";
echo TcMcCalc::mul(5, 6,) . "\n";
?>
--EXPECT--
7
30
--CLEAN--
<?php

--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Named arguments: constructor with variadic collects named overflow with string keys
--FILE--
<?php
class Cbox {
    public $a; public $b; public $rest;
    public function __construct($a, $b, ...$rest) {
        $this->a = $a;
        $this->b = $b;
        $this->rest = $rest;
    }
}
$b1 = new Cbox(b: 20, a: 10);
echo "1: a={$b1->a} b={$b1->b} rest_count=" . count($b1->rest) . "\n";

$b2 = new Cbox(10, 20, 30, 40);
echo "2: a={$b2->a} b={$b2->b} rest=" . implode(",", $b2->rest) . "\n";

$b3 = new Cbox(a: 1, b: 2, x: 99, y: 88);
echo "3: a={$b3->a} b={$b3->b}";
foreach ($b3->rest as $k => $v) {
    echo " $k=$v";
}
echo "\n";
?>
--EXPECT--
1: a=10 b=20 rest_count=0
2: a=10 b=20 rest=30,40
3: a=1 b=2 x=99 y=88
--CLEAN--
<?php

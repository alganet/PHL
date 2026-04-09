--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Trailing comma in constructor call
--FILE--
<?php
class TcCcPoint {
    public $x;
    public $y;
    function __construct($x, $y) { $this->x = $x; $this->y = $y; }
}
$p = new TcCcPoint(1, 2,);
echo $p->x . "\n";
echo $p->y . "\n";
?>
--EXPECT--
1
2
--CLEAN--
<?php

--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Named arguments: constructor calls
--FILE--
<?php
class NamPt {
    public $x;
    public $y;
    public function __construct($x, $y) {
        $this->x = $x;
        $this->y = $y;
    }
}
$p = new NamPt(y: 20, x: 10);
echo "x={$p->x} y={$p->y}\n";
?>
--EXPECT--
x=10 y=20
--CLEAN--
<?php

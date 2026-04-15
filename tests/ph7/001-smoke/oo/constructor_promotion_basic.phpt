--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Constructor property promotion: basic public int
--FILE--
<?php
class CppPoint {
    public function __construct(public int $x) {}
}
$p = new CppPoint(5);
echo $p->x, "\n";
$p->x = 7;
echo $p->x, "\n";
?>
--EXPECT--
5
7
--CLEAN--
<?php
unset($p);

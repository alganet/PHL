--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
An interface may extend several interfaces; instanceof reaches all of them
--FILE--
<?php
interface ImeA { public function a(): string; }
interface ImeB { public function b(): string; }
interface ImeC extends ImeA, ImeB { public function c(): string; }
class ImeImpl implements ImeC {
    public function a(): string { return 'a'; }
    public function b(): string { return 'b'; }
    public function c(): string { return 'c'; }
}
$o = new ImeImpl();
echo ($o instanceof ImeA) ? "A\n" : "no A\n";
echo ($o instanceof ImeB) ? "B\n" : "no B\n";
echo ($o instanceof ImeC) ? "C\n" : "no C\n";
?>
--EXPECT--
A
B
C

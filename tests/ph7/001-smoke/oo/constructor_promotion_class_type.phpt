--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Constructor property promotion: class-typed promoted property
--FILE--
<?php
class CppInner { public int $n = 0; }
class CppOuter {
    public function __construct(public CppInner $inner) {}
}
$i = new CppInner();
$i->n = 5;
$o = new CppOuter($i);
echo $o->inner->n, "\n";
$o->inner->n = 9;
echo $o->inner->n, "\n";
?>
--EXPECT--
5
9
--CLEAN--
<?php
unset($i, $o);

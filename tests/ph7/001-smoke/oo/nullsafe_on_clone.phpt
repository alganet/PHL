--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Nullsafe reads the cloned instance's property
--FILE--
<?php
class NsfCloneBox { public $v = 1; }
$nsfClone_a = new NsfCloneBox();
$nsfClone_a->v = 5;
$nsfClone_b = clone $nsfClone_a;
$nsfClone_b->v = 9;
echo $nsfClone_a?->v, "\n";
echo $nsfClone_b?->v, "\n";
?>
--EXPECT--
5
9
--CLEAN--
<?php
unset($nsfClone_a, $nsfClone_b);

--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Nullsafe expression is usable inside an if condition
--FILE--
<?php
class NsfCondFlag { public $on = true; }
$nsfCond_f = null;
if ($nsfCond_f?->on) {
    echo "branch-a\n";
} else {
    echo "branch-b\n";
}
$nsfCond_g = new NsfCondFlag();
if ($nsfCond_g?->on) {
    echo "branch-c\n";
} else {
    echo "branch-d\n";
}
?>
--EXPECT--
branch-b
branch-c
--CLEAN--
<?php
unset($nsfCond_f, $nsfCond_g);

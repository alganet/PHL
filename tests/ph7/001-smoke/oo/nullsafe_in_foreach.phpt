--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Nullsafe element access inside a loop, each iteration an independent scope
--FILE--
<?php
class NsfForeachWrap { public $v = 0; function __construct($v) { $this->v = $v; } }
$nsfForeach_items = array(new NsfForeachWrap(1), null, new NsfForeachWrap(3), null, new NsfForeachWrap(5));
$nsfForeach_sum = 0;
foreach ($nsfForeach_items as $nsfForeach_it) {
    $nsfForeach_sum += ($nsfForeach_it?->v ?? 0);
}
echo $nsfForeach_sum, "\n";
?>
--EXPECT--
9
--CLEAN--
<?php
unset($nsfForeach_items, $nsfForeach_sum, $nsfForeach_it);

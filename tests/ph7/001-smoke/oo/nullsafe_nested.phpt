--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Nested nullsafe operators in the same chain
--FILE--
<?php
class NsfNestedLeaf { public $v = 99; }
class NsfNestedMid { public $leaf; function __construct() { $this->leaf = new NsfNestedLeaf(); } }
class NsfNestedRoot { public $mid; function __construct($m) { $this->mid = $m; } }

$nsfNested_r1 = new NsfNestedRoot(null);
$nsfNested_x = $nsfNested_r1?->mid?->leaf?->v;
echo ($nsfNested_x === null ? "yes" : "no"), "\n";

$nsfNested_r2 = new NsfNestedRoot(new NsfNestedMid());
echo $nsfNested_r2?->mid?->leaf?->v, "\n";
?>
--EXPECT--
yes
99
--CLEAN--
<?php
unset($nsfNested_r1, $nsfNested_r2, $nsfNested_x);

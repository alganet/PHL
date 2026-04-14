--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Nullsafe reaches inherited properties and methods
--FILE--
<?php
class NsfInheritBase {
    public $inherited = "base-prop";
    public function greet() { return "base-greet"; }
}
class NsfInheritChild extends NsfInheritBase {}
$nsfInherit_c = new NsfInheritChild();
echo $nsfInherit_c?->inherited, "\n";
echo $nsfInherit_c?->greet(), "\n";
$nsfInherit_n = null;
echo ($nsfInherit_n?->inherited ?? "-"), "\n";
?>
--EXPECT--
base-prop
base-greet
-
--CLEAN--
<?php
unset($nsfInherit_c, $nsfInherit_n);

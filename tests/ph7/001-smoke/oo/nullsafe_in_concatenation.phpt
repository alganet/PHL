--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Nullsafe in string concatenation — each `.` operand is an independent chain scope
--FILE--
<?php
class NsfConcatUser { public $name = "bob"; }
$nsfConcat_u = new NsfConcatUser();
$nsfConcat_none = null;
echo "[" . ($nsfConcat_u?->name ?? "-") . "/" . ($nsfConcat_none?->name ?? "-") . "]\n";
?>
--EXPECT--
[bob/-]
--CLEAN--
<?php
unset($nsfConcat_u, $nsfConcat_none);

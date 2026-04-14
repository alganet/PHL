--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Nullsafe property access on a real object returns the property value
--FILE--
<?php
class NsfPropObjBox { public $label = "hello"; }
$nsfPropObj_b = new NsfPropObjBox();
echo $nsfPropObj_b?->label, "\n";
?>
--EXPECT--
hello
--CLEAN--
<?php
unset($nsfPropObj_b);

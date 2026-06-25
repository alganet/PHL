--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
A cloned object receives its own distinct object id
--FILE--
<?php
class C { public $v = 1; }
$a = new C;
$b = clone $a;
echo (spl_object_id($a) !== spl_object_id($b)) ? "distinct\n" : "same\n";
?>
--EXPECT--
distinct
--CLEAN--
<?php

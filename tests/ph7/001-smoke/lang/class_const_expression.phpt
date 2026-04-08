--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
::class used in expressions and comparisons
--FILE--
<?php
class CceMyClass {}
$name = CceMyClass::class;
echo $name . "\n";
echo (CceMyClass::class === "CceMyClass") ? "true" : "false";
echo "\n";
$arr = [CceMyClass::class => "found"];
echo $arr["CceMyClass"] . "\n";
echo strlen(CceMyClass::class) . "\n";
?>
--EXPECT--
CceMyClass
true
found
10
--CLEAN--
<?php
unset($name, $arr);

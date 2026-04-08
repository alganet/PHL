--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
$obj::class returns class name (PHP 8.0+)
--FILE--
<?php
class CcoVarTest {}
$obj = new CcoVarTest();
echo $obj::class . "\n";
echo ($obj::class === "CcoVarTest") ? "true" : "false";
echo "\n";
?>
--EXPECT--
CcoVarTest
true
--CLEAN--
<?php
unset($obj);

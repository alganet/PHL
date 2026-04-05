--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
spl_autoload_functions returns empty array when no autoloaders registered
--FILE--
<?php
$funcs = spl_autoload_functions();
echo is_array($funcs) ? "array" : "not array";
echo "\n";
echo count($funcs) . "\n";
?>
--EXPECT--
array
0
--CLEAN--
<?php
unset($funcs);

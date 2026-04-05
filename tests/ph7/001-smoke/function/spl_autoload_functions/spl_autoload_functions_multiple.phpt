--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
spl_autoload_functions returns all registered autoloaders
--FILE--
<?php
function loader_a($class) {}
function loader_b($class) {}
spl_autoload_register('loader_a');
spl_autoload_register('loader_b');
$funcs = spl_autoload_functions();
echo count($funcs) . "\n";
echo $funcs[0] . "\n";
echo $funcs[1] . "\n";
?>
--EXPECT--
2
loader_a
loader_b
--CLEAN--
<?php
unset($funcs);

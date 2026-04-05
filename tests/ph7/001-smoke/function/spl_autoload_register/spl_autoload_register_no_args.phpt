--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
spl_autoload_register with no arguments registers default spl_autoload
--FILE--
<?php
// Clear any leaked autoloaders from previous tests
foreach (spl_autoload_functions() as $f) spl_autoload_unregister($f);

spl_autoload_register();
$funcs = spl_autoload_functions();
echo count($funcs) . "\n";
spl_autoload_unregister('spl_autoload');
?>
--EXPECT--
1
--CLEAN--
<?php
unset($funcs);

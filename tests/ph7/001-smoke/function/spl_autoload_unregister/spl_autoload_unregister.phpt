--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
spl_autoload_unregister removes a named function from the autoload stack
--FILE--
<?php
// Clear any leaked autoloaders from previous tests
foreach (spl_autoload_functions() as $f) spl_autoload_unregister($f);

function spl_unregister_test_loader($class) {
    echo "spl_unregister_test_loader: $class\n";
}
spl_autoload_register('spl_unregister_test_loader');
echo count(spl_autoload_functions()) . "\n";
$result = spl_autoload_unregister('spl_unregister_test_loader');
echo count(spl_autoload_functions()) . "\n";
echo ($result ? "true" : "false") . "\n";
?>
--EXPECT--
1
0
true
--CLEAN--
<?php

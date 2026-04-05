--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
spl_autoload_unregister returns false when callback is not in the autoload stack
--FILE--
<?php
// Clear any leaked autoloaders from previous tests
foreach (spl_autoload_functions() as $f) spl_autoload_unregister($f);

// Define a real function but never register it as an autoloader
function spl_unregister_nf_loader($class) {}

// Unregistering a function that exists but is not in the autoload stack
$result = spl_autoload_unregister('spl_unregister_nf_loader');
echo ($result ? "true" : "false") . "\n";
?>
--EXPECT--
false
--CLEAN--
<?php
unset($result);

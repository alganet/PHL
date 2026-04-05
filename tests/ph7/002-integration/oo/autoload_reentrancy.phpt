--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Autoload reentrancy guard prevents infinite recursion
--FILE--
<?php
$al_reentry_loader = function ($class) {
    echo "autoload: $class\n";
    // This would cause infinite recursion without the reentrancy guard,
    // since class_exists triggers autoload which calls class_exists again
    class_exists($class, true);
};
spl_autoload_register($al_reentry_loader);
// Should call autoload once, not loop
if (class_exists('AutoloadReentrancyTest')) {
    echo "true" . PHP_EOL;
} else {
    echo "false" . PHP_EOL;
}
echo "no infinite loop\n";
spl_autoload_unregister($al_reentry_loader);
?>
--EXPECT--
autoload: AutoloadReentrancyTest
false
no infinite loop
--CLEAN--
<?php
unset($al_reentry_loader);

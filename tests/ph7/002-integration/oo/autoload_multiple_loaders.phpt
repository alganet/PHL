--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Multiple autoloaders are tried in order until class is found
--FILE--
<?php
$al_multi_1 = function ($class) {
    echo "loader1: $class\n";
    // Does not define the class
};
$al_multi_2 = function ($class) {
    echo "loader2: $class\n";
    if ($class === 'AutoloadMultiTest') {
        eval('class AutoloadMultiTest {}');
    }
};
$al_multi_3 = function ($class) {
    echo "loader3: $class\n";
};
spl_autoload_register($al_multi_1);
spl_autoload_register($al_multi_2);
spl_autoload_register($al_multi_3);

// loader1 and loader2 should be called; loader3 should NOT (class found after loader2)
$result = class_exists('AutoloadMultiTest');
echo "exists: " . ($result ? "true" : "false") . "\n";

spl_autoload_unregister($al_multi_1);
spl_autoload_unregister($al_multi_2);
spl_autoload_unregister($al_multi_3);
?>
--EXPECT--
loader1: AutoloadMultiTest
loader2: AutoloadMultiTest
exists: true
--CLEAN--
<?php
unset($result, $al_multi_1, $al_multi_2, $al_multi_3);

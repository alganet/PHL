--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
spl_autoload_register registers a callback and triggers it on class lookup
--FILE--
<?php
$spl_test_loader = function ($class) {
    echo "autoload: $class\n";
    if ($class === 'SplAutoloadTestClass') {
        eval('class SplAutoloadTestClass { public function bar() { return 42; } }');
    }
};
spl_autoload_register($spl_test_loader);
$obj = new SplAutoloadTestClass;
echo $obj->bar() . "\n";
spl_autoload_unregister($spl_test_loader);
?>
--EXPECT--
autoload: SplAutoloadTestClass
42
--CLEAN--
<?php
unset($obj, $spl_test_loader);

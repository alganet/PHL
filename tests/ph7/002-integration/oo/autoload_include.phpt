--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Autoload triggers file include to load a class definition
--FILE--
<?php
$al_inc_loader = function ($class) {
    if ($class === 'IncludedClass') {
        require __DIR__ . '/autoload_include.php';
    }
};
spl_autoload_register($al_inc_loader);
$obj = new IncludedClass;
echo $obj->greet() . "\n";
spl_autoload_unregister($al_inc_loader);
?>
--EXPECT--
hello from included file
--CLEAN--
<?php
unset($obj, $al_inc_loader);

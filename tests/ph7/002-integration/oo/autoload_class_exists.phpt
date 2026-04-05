--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
class_exists honors the $autoload parameter
--FILE--
<?php
$al_exists_loader = function ($class) {
    echo "autoload: $class\n";
    if ($class === 'AutoloadExistsTestClass') {
        eval('class AutoloadExistsTestClass {}');
    }
};
spl_autoload_register($al_exists_loader);

// autoload=true (default): triggers autoload
echo "with autoload: " . (class_exists('AutoloadExistsTestClass') ? "true" : "false") . "\n";

// autoload=false: direct lookup only
echo "without autoload: " . (class_exists('AutoloadExistsMissing', false) ? "true" : "false") . "\n";

// Already loaded class: no autoload trigger
echo "already loaded: " . (class_exists('AutoloadExistsTestClass') ? "true" : "false") . "\n";

spl_autoload_unregister($al_exists_loader);
?>
--EXPECT--
autoload: AutoloadExistsTestClass
with autoload: true
without autoload: false
already loaded: true
--CLEAN--
<?php
unset($al_exists_loader);

--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
spl_autoload_register with prepend=true inserts at front of stack
--FILE--
<?php
$spl_prep_f1 = function ($c) { echo "first\n"; };
$spl_prep_f2 = function ($c) { echo "second\n"; };
$spl_prep_f0 = function ($c) { echo "prepended\n"; };
spl_autoload_register($spl_prep_f1);
spl_autoload_register($spl_prep_f2);
spl_autoload_register($spl_prep_f0, true, true);
class_exists('SplAutoloadPrependTestClass');
spl_autoload_unregister($spl_prep_f0);
spl_autoload_unregister($spl_prep_f1);
spl_autoload_unregister($spl_prep_f2);
?>
--EXPECT--
prepended
first
second
--CLEAN--
<?php
unset($spl_prep_f0, $spl_prep_f1, $spl_prep_f2);

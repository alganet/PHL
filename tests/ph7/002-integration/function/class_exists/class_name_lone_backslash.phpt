--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--SKIPIF--
<?php if (function_exists('zend_version') && PHP_VERSION_ID < 80511) echo 'skip php before 8.5.11 autoloads the empty name a lone "\\" strips to (GH-23232)'; ?>
--TEST--
A lone namespace separator names no class and never reaches the autoloader
--FILE--
<?php
// php strips the global-namespace anchor and autoloads the STRIPPED name -- but a
// lone "\" leaves no name at all, so it names no class and never reaches the
// autoloader (php 8.5.11, GH-23232), and neither does a truly empty "".
spl_autoload_register(function ($n) { echo "AUTOLOAD[$n]\n"; });
var_dump(class_exists('\\'));
var_dump(class_exists(''));
var_dump(method_exists('\\', 'm'));
var_dump(interface_exists('\\'), trait_exists('\\'));
?>
--EXPECT--
bool(false)
bool(false)
bool(false)
bool(false)
bool(false)

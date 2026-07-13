--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
ReflectionConstant origin metadata: file, extension, internal
--FILE--
<?php
define('REFL_C5_DEF', 1);
const REFL_C5_CONST = 2;
$rd = new ReflectionConstant('REFL_C5_DEF');
echo basename($rd->getFileName()) === basename(__FILE__) ? 'same-file' : 'diff-file', "\n";
echo $rd->getExtensionName() === false ? 'false' : 'ext', "\n";
echo $rd->getExtension() === null ? 'null' : 'obj', "\n";
$rc = new ReflectionConstant('REFL_C5_CONST');
echo basename($rc->getFileName()) === basename(__FILE__) ? 'same-file' : 'diff-file', "\n";
$ri = new ReflectionConstant('PHP_INT_MAX');
echo $ri->getFileName() === false ? 'no-file' : 'file', "\n";
echo $ri->getExtensionName(), "\n";
echo get_class($ri->getExtension()), ':', $ri->getExtension()->getName(), "\n";
?>
--EXPECT--
same-file
false
null
same-file
no-file
Core
ReflectionExtension:Core

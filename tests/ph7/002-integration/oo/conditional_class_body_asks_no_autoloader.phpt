--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Compiling a conditional class body never asks the autoloader for what it extends
--DESCRIPTION--
php compiles a class written inside `if (false)` with its file but links it only
when the statement runs, so nothing it names is looked up. The shape is
nikic/php-parser's class alias: the file declares the parent itself, then holds
`class Alias extends Parent {}` in a dead branch. Had the compile asked the
autoloader for the parent, the autoloader would have declared it first and the
file's own declaration would be a redeclaration.
--FILE--
<?php
spl_autoload_register(function ($c) {
    echo "autoload $c\n";
    eval('class CcbAlParent {}');
});
eval('
if (false) {
    class CcbAlAlias extends CcbAlParent implements CcbAlIface { use CcbAlTrait; }
}
echo "compiled\n";
class CcbAlParent {}
');
var_dump(class_exists('CcbAlParent', false), class_exists('CcbAlAlias', false));
?>
--EXPECT--
compiled
bool(true)
bool(false)

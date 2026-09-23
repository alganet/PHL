--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Reflection: an internal parameter default that is a class-constant expression
--FILE--
<?php
/* php's stub keeps the SOURCE of a constant-expression default, so the two
 * surfaces disagree on purpose: the export line prints the expression and
 * getDefaultValue() prints what it evaluates to. */
function rIdcShow($class, $method) {
    $p = (new ReflectionMethod($class, $method))->getParameters();
    foreach ($p as $one) {
        if (!$one->isDefaultValueAvailable()) { continue; }
        $line = (string)$one;
        echo trim(preg_replace('/^.*?<optional> /', '', $line)), "\n";
        var_dump($one->getDefaultValue(), $one->isDefaultValueConstant());
    }
}
rIdcShow('SplFileInfo', 'setInfoClass');
rIdcShow('SplFileInfo', 'getFileInfo');
rIdcShow('SplFileInfo', 'getBasename');
?>
--EXPECT--
string $class = SplFileInfo::class ]
string(11) "SplFileInfo"
bool(false)
?string $class = null ]
NULL
bool(false)
string $suffix = "" ]
string(0) ""
bool(false)

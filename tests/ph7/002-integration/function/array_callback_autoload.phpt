--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
A [ClassName, staticMethod] callback autoloads the class (array_map/is_callable)
--FILE--
<?php
spl_autoload_register(function ($c) {
    if ($c === 'AcaLazyHelper') {
        eval('class AcaLazyHelper { public static function fmt($v) { return "<$v>"; } }');
    }
});
// class not yet loaded; the array callable must autoload it
var_dump(is_callable(['AcaLazyHelper', 'fmt']));
$r = array_map(['AcaLazyHelper', 'fmt'], ['a', 'b']);
echo implode(',', $r), "\n";
?>
--EXPECT--
bool(true)
<a>,<b>

--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
PHL: array() with a key and no value should produce a compile error
--SKIPIF--
<?php
if (function_exists('zend_version')) {
    echo "skip";
}
?>
--FILE--
<?php
$x = array(0 => );
?>
--EXPECTF--
%AParse error:%Asyntax error, unexpected token "=>"%A
--CLEAN--
<?php
unset($x);

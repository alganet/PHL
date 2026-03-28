--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
PHL: array() with ampersand and missing variable produces compile error
--SKIPIF--
<?php
if (function_exists('zend_version')) {
    echo "skip";
}
?>
--FILE--
<?php
$x = array(&);
?>
--EXPECTF--
%s Error:  array(): Missing referenced variable %s
--CLEAN--
<?php
unset($x);

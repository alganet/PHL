--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
PH7: sprintf left-justify flag (%-s) coverage
--SKIPIF--
<?php
if (function_exists('zend_version')) {
    echo "skip";
}
?>
--FILE--
<?php
// Test left-justify flag - padding should be on the right
$ret = sprintf("%-10s", "Hi");
var_dump($ret);
?>
--EXPECTF--
string(10 'Hi        ')
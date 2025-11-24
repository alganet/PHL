--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
PH7: MAXINT maps to PHP_INT_MAX
--SKIPIF--
<?php
if (function_exists('zend_version')) {
    echo "skip";
}
--FILE--
<?php
// MAXINT mirrors PHP_INT_MAX
echo "MAXINT=" . MAXINT . "\n";
?>
--EXPECTF--
MAXINT=%d
--CLEAN--
<?php
// nothing to clean
?>

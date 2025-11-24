--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
PH7: UTF8 constant
--SKIPIF--
<?php
if (function_exists('zend_version')) {
    echo "skip";
}
--FILE--
<?php
echo "UTF8=" . UTF8 . "\n";
?>
--EXPECTF--
UTF8=%s


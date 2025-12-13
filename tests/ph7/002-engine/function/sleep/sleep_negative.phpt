--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
sleep() should return FALSE on negative values
--SKIPIF--
<?php
if (function_exists('zend_version')) {
    echo "skip";
}
?>
--FILE--
<?php
echo sleep(-1) ? "true\n" : "false\n";
echo sleep('abc') ? "true\n" : "false\n";
?>
--EXPECT--
false
false

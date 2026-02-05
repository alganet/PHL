--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Array with boolean value
--SKIPIF--
<?php if (function_exists('zend_version')) echo 'skip'; ?>
--FILE--
<?php
// Test array with boolean value
$a = array("key" => true);
echo "Array created\n";
?>
--EXPECT--
Array created
--CLEAN--
<?php
unset($a);

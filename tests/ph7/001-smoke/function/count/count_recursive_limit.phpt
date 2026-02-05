--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
count with COUNT_RECURSIVE on deeply nested array (recursion limit)
--SKIPIF--
<?php if (function_exists('zend_version')) echo 'skip Zend PHP has different recursion limit'; ?>
--FILE--
<?php
// Create a deeply nested array (>31 levels)
$nested = array();
for ($i = 0; $i < 35; $i++) {
    $nested = array('level' => $nested);
}
$nested['leaf'] = 'value';

// Count recursively
$count = count($nested, COUNT_RECURSIVE);
echo "Recursive count: $count\n";
?>
--EXPECT--
Recursive count: 33
--CLEAN--
<?php
unset($nested, $count);
